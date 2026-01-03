#include <uv.h>
#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>
#import <ApplicationServices/ApplicationServices.h>
#include <saucer/app.h>
#include <iostream>
#include <iomanip>

namespace saucer_nodejs {

// CFRunLoop integration for smooth NSApp/libuv interleaving
// Based on Electron's approach

static uv_loop_t* g_uv_loop = nullptr;
static CFRunLoopSourceRef g_cf_source = nullptr;
static CFRunLoopRef g_main_runloop = nullptr;
static saucer_application* g_saucer_app = nullptr;

// This callback is invoked by CFRunLoop when libuv needs to process events
static void UvRunLoopSourceCallback(void* info) {
  if (g_uv_loop) {
    // Run one iteration of the libuv event loop
    uv_run(g_uv_loop, UV_RUN_NOWAIT);
  }

  // Also process saucer events
  if (g_saucer_app) {
    saucer_application_run_once(g_saucer_app);
  }
}

// Schedule the CFRunLoop source to be called
static void ScheduleUvWork() {
  if (g_cf_source && g_main_runloop) {
    CFRunLoopSourceSignal(g_cf_source);
    CFRunLoopWakeUp(g_main_runloop);
  }
}

// Wakeup callback for libuv - called when libuv has work to do
static void UvWakeup(uv_async_t* handle) {
  ScheduleUvWork();
}

// Transform process to foreground app - MUST be called before NSApplication init
// This is the critical fix for cmd+tab visibility
void TransformToForegroundApp() {
  std::cout << "[Saucer] Transforming process to foreground application..." << std::endl;

  @try {
    ProcessSerialNumber psn = { 0, kCurrentProcess };
    OSStatus status = TransformProcessType(&psn, kProcessTransformToForegroundApplication);

    if (status == noErr) {
      std::cout << "[Saucer]   ✓ Process transformation successful" << std::endl;
    } else if (status == paramErr || status == -50) {
      // paramErr (-50) means already transformed - this is OK
      std::cout << "[Saucer]   ✓ Already transformed (OK)" << std::endl;
    } else {
      std::cout << "[Saucer]   ⚠️  TransformProcessType returned: " << status << std::endl;
      std::cout << "[Saucer]      Continuing anyway..." << std::endl;
    }
  } @catch (NSException* exception) {
    std::cout << "[Saucer]   ⚠️  Exception: " << [[exception description] UTF8String] << std::endl;
    std::cout << "[Saucer]      Will continue with other activation methods" << std::endl;
  }
}

// Diagnostic function to check and print current activation state
void DiagnoseActivationPolicy() {
  @autoreleasepool {
    NSApplication* app = [NSApplication sharedApplication];
    if (!app) {
      std::cout << "[Saucer] Cannot diagnose - NSApplication is nil" << std::endl;
      return;
    }

    NSApplicationActivationPolicy policy = [app activationPolicy];

    const char* policyName = "Unknown";
    switch (policy) {
      case NSApplicationActivationPolicyRegular:
        policyName = "Regular (should appear in cmd+tab ✓)";
        break;
      case NSApplicationActivationPolicyAccessory:
        policyName = "Accessory (HIDDEN from cmd+tab ✗)";
        break;
      case NSApplicationActivationPolicyProhibited:
        policyName = "Prohibited (background only ✗)";
        break;
    }

    NSRunningApplication* currentApp = [NSRunningApplication currentApplication];

    // Safely get bundle ID (may be nil for Node.js process)
    NSString* bundleId = [[NSBundle mainBundle] bundleIdentifier];
    const char* bundleIdStr = bundleId ? [bundleId UTF8String] : "(none - running as node)";

    std::cout << "\n========================================" << std::endl;
    std::cout << "macOS Application Activation Diagnostics" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Activation Policy: " << policyName << std::endl;
    std::cout << "Is Active:         " << ([app isActive] ? "YES ✓" : "NO ✗") << std::endl;
    std::cout << "Is Running:        " << ([app isRunning] ? "YES ✓" : "NO ✗") << std::endl;
    std::cout << "Is Hidden:         " << ([app isHidden] ? "YES ✗" : "NO ✓") << std::endl;
    std::cout << "Process ID:        " << [[NSProcessInfo processInfo] processIdentifier] << std::endl;
    std::cout << "Bundle ID:         " << bundleIdStr << std::endl;
    std::cout << "App Active:        " << (currentApp && [currentApp isActive] ? "YES ✓" : "NO ✗") << std::endl;
    std::cout << "========================================\n" << std::endl;
  }
}

// Activate NSApplication so it appears in cmd+tab and behaves like a regular app
// NOTE: TransformToForegroundApp() should be called BEFORE this function
void ActivateNSApplication() {
  @autoreleasepool {
    std::cout << "[Saucer] Verifying NSApplication activation..." << std::endl;

    // Get NSApplication instance (saucer already created it)
    NSApplication* app = [NSApplication sharedApplication];

    if (!app) {
      std::cout << "[Saucer]   ✗ NSApplication is nil!" << std::endl;
      return;
    }

    // CRITICAL: Tell NSApp it has finished launching
    // This is required for cmd+tab to work - Electron does this too
    if (![app isRunning]) {
      std::cout << "[Saucer]   → Calling finishLaunching..." << std::endl;
      @try {
        [app finishLaunching];
        std::cout << "[Saucer]   ✓ NSApp finishLaunching called" << std::endl;
      } @catch (NSException* exception) {
        std::cout << "[Saucer]   ⚠️  finishLaunching exception (may be OK)" << std::endl;
      }
    }

    // Verify activation policy (saucer library should have set this)
    NSApplicationActivationPolicy currentPolicy = [app activationPolicy];

    if (currentPolicy == NSApplicationActivationPolicyRegular) {
      std::cout << "[Saucer]   ✓ Policy is Regular (good!)" << std::endl;
    } else {
      std::cout << "[Saucer]   ⚠️  Policy is not Regular, setting..." << std::endl;
      @try {
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        std::cout << "[Saucer]   ✓ Policy set to Regular" << std::endl;
      } @catch (NSException* exception) {
        std::cout << "[Saucer]   ⚠️  Could not set policy" << std::endl;
      }
    }

    // Additional activation to ensure visibility
    @try {
      [app activateIgnoringOtherApps:YES];

      NSRunningApplication* currentApp = [NSRunningApplication currentApplication];
      if (currentApp) {
        [currentApp activateWithOptions:NSApplicationActivateAllWindows];
      }

      std::cout << "[Saucer]   ✓ Application activated" << std::endl;
    } @catch (NSException* exception) {
      std::cout << "[Saucer]   ⚠️  Activation warning (may already be active)" << std::endl;
    }

    // Delayed activation for timing-sensitive macOS versions
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(100 * NSEC_PER_MSEC)),
                   dispatch_get_main_queue(), ^{
      @autoreleasepool {
        @try {
          [[NSApplication sharedApplication] activateIgnoringOtherApps:YES];
        } @catch (NSException*) {
          // Silent
        }
      }
    });

    std::cout << "[Saucer] ✓ Activation complete\n" << std::endl;

    // Print diagnostics
    @try {
      DiagnoseActivationPolicy();
    } @catch (NSException*) {
      std::cout << "[Saucer] Could not print diagnostics\n" << std::endl;
    }
  }
}

void InitializeRunLoop(saucer_application* app) {
  g_uv_loop = uv_default_loop();
  g_main_runloop = CFRunLoopGetMain();
  g_saucer_app = app;

  // Create a CFRunLoopSource for libuv
  CFRunLoopSourceContext source_context = {};
  source_context.perform = UvRunLoopSourceCallback;

  g_cf_source = CFRunLoopSourceCreate(kCFAllocatorDefault, 0, &source_context);
  CFRunLoopAddSource(g_main_runloop, g_cf_source, kCFRunLoopCommonModes);

  // Install a libuv async handle to wake up the CFRunLoop when libuv has work
  static uv_async_t async_wakeup;
  uv_async_init(g_uv_loop, &async_wakeup, UvWakeup);
  uv_unref((uv_handle_t*)&async_wakeup);

  // Make libuv call our wakeup when it has pending work
  // We'll use a prepare handle to check for work
  static uv_prepare_t prepare;
  uv_prepare_init(g_uv_loop, &prepare);
  uv_prepare_start(&prepare, [](uv_prepare_t* handle) {
    ScheduleUvWork();
  });
  uv_unref((uv_handle_t*)&prepare);

  std::cout << "CFRunLoop integration initialized" << std::endl;
}

void RunOnceIntegrated() {
  if (g_uv_loop) {
    // Process any pending libuv events
    uv_run(g_uv_loop, UV_RUN_NOWAIT);

    // Signal that we need to be called again if there's work
    ScheduleUvWork();
  }
}

void CleanupRunLoop() {
  if (g_cf_source) {
    CFRunLoopSourceInvalidate(g_cf_source);
    CFRelease(g_cf_source);
    g_cf_source = nullptr;
  }
  g_main_runloop = nullptr;
  g_uv_loop = nullptr;
}

} // namespace saucer_nodejs
