/**
 * macOS-specific implementations for premium features
 */

#include "platform.hpp"

#ifdef __APPLE__

#import <Cocoa/Cocoa.h>
// Note: Not using UserNotifications because it requires a proper app bundle
// Using osascript as a fallback for notifications

namespace saucer_nodejs {

// ============================================================================
// Clipboard Implementation (macOS)
// ============================================================================

Napi::Object Clipboard::Init(Napi::Env env, Napi::Object exports) {
  Napi::Object clipboard = Napi::Object::New(env);
  
  clipboard.Set("readText", Napi::Function::New(env, &Clipboard::ReadText));
  clipboard.Set("writeText", Napi::Function::New(env, &Clipboard::WriteText));
  clipboard.Set("hasText", Napi::Function::New(env, &Clipboard::HasText));
  clipboard.Set("hasImage", Napi::Function::New(env, &Clipboard::HasImage));
  clipboard.Set("clear", Napi::Function::New(env, &Clipboard::Clear));
  
  exports.Set("clipboard", clipboard);
  return exports;
}

Clipboard::Clipboard(const Napi::CallbackInfo& info) 
  : Napi::ObjectWrap<Clipboard>(info) {}

Napi::Value Clipboard::ReadText(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  @autoreleasepool {
    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
    NSString* text = [pasteboard stringForType:NSPasteboardTypeString];
    
    if (text) {
      return Napi::String::New(env, [text UTF8String]);
    }
  }
  
  return Napi::String::New(env, "");
}

void Clipboard::WriteText(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  if (info.Length() == 0 || !info[0].IsString()) {
    Napi::TypeError::New(env, "String required").ThrowAsJavaScriptException();
    return;
  }
  
  std::string text = info[0].As<Napi::String>().Utf8Value();
  
  @autoreleasepool {
    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard clearContents];
    [pasteboard setString:[NSString stringWithUTF8String:text.c_str()] 
                  forType:NSPasteboardTypeString];
  }
}

Napi::Value Clipboard::HasText(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  @autoreleasepool {
    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
    NSArray* types = [pasteboard types];
    bool hasText = [types containsObject:NSPasteboardTypeString];
    return Napi::Boolean::New(env, hasText);
  }
}

Napi::Value Clipboard::HasImage(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  @autoreleasepool {
    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
    NSArray* types = [pasteboard types];
    bool hasImage = [types containsObject:NSPasteboardTypePNG] || 
                    [types containsObject:NSPasteboardTypeTIFF];
    return Napi::Boolean::New(env, hasImage);
  }
}

Napi::Value Clipboard::ReadImage(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  @autoreleasepool {
    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
    NSData* imageData = [pasteboard dataForType:NSPasteboardTypePNG];
    
    if (!imageData) {
      imageData = [pasteboard dataForType:NSPasteboardTypeTIFF];
    }
    
    if (imageData) {
      return Napi::Buffer<uint8_t>::Copy(env, 
        static_cast<const uint8_t*>([imageData bytes]), 
        [imageData length]);
    }
  }
  
  return env.Null();
}

void Clipboard::WriteImage(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  if (info.Length() == 0 || !info[0].IsBuffer()) {
    Napi::TypeError::New(env, "Buffer required").ThrowAsJavaScriptException();
    return;
  }
  
  Napi::Buffer<uint8_t> buffer = info[0].As<Napi::Buffer<uint8_t>>();
  
  @autoreleasepool {
    NSData* imageData = [NSData dataWithBytes:buffer.Data() length:buffer.Length()];
    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard clearContents];
    [pasteboard setData:imageData forType:NSPasteboardTypePNG];
  }
}

void Clipboard::Clear(const Napi::CallbackInfo& info) {
  @autoreleasepool {
    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard clearContents];
  }
}

// ============================================================================
// Notification Implementation (macOS)
// Using osascript as fallback since UNUserNotificationCenter requires a bundle
// ============================================================================

Napi::Object Notification::Init(Napi::Env env, Napi::Object exports) {
  Napi::Function func = DefineClass(env, "Notification", {
    InstanceMethod("show", &Notification::Show),
    StaticMethod("isSupported", &Notification::IsSupported),
    StaticMethod("requestPermission", &Notification::RequestPermission),
  });
  
  Napi::FunctionReference* constructor = new Napi::FunctionReference();
  *constructor = Napi::Persistent(func);
  
  exports.Set("Notification", func);
  return exports;
}

Notification::Notification(const Napi::CallbackInfo& info) 
  : Napi::ObjectWrap<Notification>(info) {
  Napi::Env env = info.Env();
  
  if (info.Length() > 0 && info[0].IsObject()) {
    Napi::Object opts = info[0].As<Napi::Object>();
    
    if (opts.Has("title") && opts.Get("title").IsString()) {
      title_ = opts.Get("title").As<Napi::String>().Utf8Value();
    }
    if (opts.Has("body") && opts.Get("body").IsString()) {
      body_ = opts.Get("body").As<Napi::String>().Utf8Value();
    }
    if (opts.Has("icon") && opts.Get("icon").IsString()) {
      icon_path_ = opts.Get("icon").As<Napi::String>().Utf8Value();
    }
  }
}

Notification::~Notification() {}

// Helper to escape strings for AppleScript
static std::string EscapeAppleScript(const std::string& s) {
  std::string result;
  for (char c : s) {
    if (c == '\"' || c == '\\') {
      result += '\\';
    }
    result += c;
  }
  return result;
}

void Notification::Show(const Napi::CallbackInfo& info) {
  // Use osascript to display notification (works without bundle)
  std::string escapedTitle = EscapeAppleScript(title_);
  std::string escapedBody = EscapeAppleScript(body_);
  
  std::string script = "display notification \"" + escapedBody + "\" with title \"" + escapedTitle + "\"";
  std::string cmd = "osascript -e '" + script + "' 2>/dev/null &";
  
  system(cmd.c_str());
}

Napi::Value Notification::IsSupported(const Napi::CallbackInfo& info) {
  // osascript is always available on macOS
  return Napi::Boolean::New(info.Env(), true);
}

Napi::Value Notification::RequestPermission(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  auto deferred = Napi::Promise::Deferred::New(env);
  // osascript doesn't need permission
  deferred.Resolve(Napi::String::New(env, "granted"));
  return deferred.Promise();
}

// ============================================================================
// SystemTray Implementation (macOS) - Menu Bar Extra
// ============================================================================

// Store reference to the status item
static NSStatusItem* g_statusItem = nil;

Napi::Object SystemTray::Init(Napi::Env env, Napi::Object exports) {
  Napi::Function func = DefineClass(env, "SystemTray", {
    InstanceMethod("setIcon", &SystemTray::SetIcon),
    InstanceMethod("setTooltip", &SystemTray::SetTooltip),
    InstanceMethod("setMenu", &SystemTray::SetMenu),
    InstanceMethod("show", &SystemTray::Show),
    InstanceMethod("hide", &SystemTray::Hide),
    InstanceMethod("destroy", &SystemTray::Destroy),
    InstanceMethod("onClick", &SystemTray::OnClick),
  });
  
  Napi::FunctionReference* constructor = new Napi::FunctionReference();
  *constructor = Napi::Persistent(func);
  
  exports.Set("SystemTray", func);
  return exports;
}

SystemTray::SystemTray(const Napi::CallbackInfo& info) 
  : Napi::ObjectWrap<SystemTray>(info) {
  @autoreleasepool {
    NSStatusBar* statusBar = [NSStatusBar systemStatusBar];
    g_statusItem = [statusBar statusItemWithLength:NSSquareStatusItemLength];
    [g_statusItem retain];
    
    // Default icon (can be changed with setIcon)
    g_statusItem.button.title = @"◆";
    native_tray_ = (__bridge void*)g_statusItem;
  }
}

SystemTray::~SystemTray() {
  @autoreleasepool {
    if (g_statusItem) {
      [[NSStatusBar systemStatusBar] removeStatusItem:g_statusItem];
      [g_statusItem release];
      g_statusItem = nil;
    }
  }
  
  if (click_tsfn_) {
    click_tsfn_.Release();
  }
}

void SystemTray::SetIcon(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  if (info.Length() == 0) {
    Napi::TypeError::New(env, "Icon path or buffer required").ThrowAsJavaScriptException();
    return;
  }
  
  @autoreleasepool {
    NSImage* image = nil;
    
    if (info[0].IsString()) {
      std::string path = info[0].As<Napi::String>().Utf8Value();
      image = [[NSImage alloc] initWithContentsOfFile:
        [NSString stringWithUTF8String:path.c_str()]];
    } else if (info[0].IsBuffer()) {
      Napi::Buffer<uint8_t> buffer = info[0].As<Napi::Buffer<uint8_t>>();
      NSData* data = [NSData dataWithBytes:buffer.Data() length:buffer.Length()];
      image = [[NSImage alloc] initWithData:data];
    }
    
    if (image && g_statusItem) {
      // Resize for menu bar
      [image setSize:NSMakeSize(18, 18)];
      g_statusItem.button.image = image;
      g_statusItem.button.title = @"";
      [image release];
    }
  }
}

void SystemTray::SetTooltip(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  if (info.Length() == 0 || !info[0].IsString()) {
    Napi::TypeError::New(env, "Tooltip string required").ThrowAsJavaScriptException();
    return;
  }
  
  std::string tooltip = info[0].As<Napi::String>().Utf8Value();
  
  @autoreleasepool {
    if (g_statusItem) {
      g_statusItem.button.toolTip = [NSString stringWithUTF8String:tooltip.c_str()];
    }
  }
}

void SystemTray::SetMenu(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  if (info.Length() == 0 || !info[0].IsArray()) {
    Napi::TypeError::New(env, "Menu items array required").ThrowAsJavaScriptException();
    return;
  }
  
  Napi::Array items = info[0].As<Napi::Array>();
  
  @autoreleasepool {
    NSMenu* menu = [[NSMenu alloc] init];
    
    for (uint32_t i = 0; i < items.Length(); i++) {
      Napi::Value item = items.Get(i);
      
      if (item.IsObject()) {
        Napi::Object itemObj = item.As<Napi::Object>();
        
        if (itemObj.Has("type") && itemObj.Get("type").IsString()) {
          std::string type = itemObj.Get("type").As<Napi::String>().Utf8Value();
          
          if (type == "separator") {
            [menu addItem:[NSMenuItem separatorItem]];
            continue;
          }
        }
        
        std::string label = "";
        if (itemObj.Has("label") && itemObj.Get("label").IsString()) {
          label = itemObj.Get("label").As<Napi::String>().Utf8Value();
        }
        
        NSMenuItem* menuItem = [[NSMenuItem alloc] 
          initWithTitle:[NSString stringWithUTF8String:label.c_str()]
          action:nil
          keyEquivalent:@""];
        
        [menu addItem:menuItem];
        [menuItem release];
      }
    }
    
    if (g_statusItem) {
      g_statusItem.menu = menu;
    }
    
    [menu release];
  }
}

void SystemTray::Show(const Napi::CallbackInfo& info) {
  @autoreleasepool {
    if (g_statusItem) {
      g_statusItem.visible = YES;
    }
  }
}

void SystemTray::Hide(const Napi::CallbackInfo& info) {
  @autoreleasepool {
    if (g_statusItem) {
      g_statusItem.visible = NO;
    }
  }
}

void SystemTray::Destroy(const Napi::CallbackInfo& info) {
  @autoreleasepool {
    if (g_statusItem) {
      [[NSStatusBar systemStatusBar] removeStatusItem:g_statusItem];
      [g_statusItem release];
      g_statusItem = nil;
      native_tray_ = nullptr;
    }
  }
}

void SystemTray::OnClick(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  if (info.Length() == 0 || !info[0].IsFunction()) {
    Napi::TypeError::New(env, "Callback function required").ThrowAsJavaScriptException();
    return;
  }
  
  click_tsfn_ = Napi::ThreadSafeFunction::New(
    env,
    info[0].As<Napi::Function>(),
    "SystemTray.onClick",
    0,
    1
  );
  
  // Note: Full click handling would require more complex Objective-C delegation
}

} // namespace saucer_nodejs

// ============================================================================
// Window/Webview Extensions Implementation (macOS)
// These are in global namespace to match the saucer_handle type
// ============================================================================

// Include saucer headers to access native() method
#include <saucer/modules/stable/webkit.hpp>

// Import the webview handle definition
#include "private/webview.hpp"

// Extension functions are in global namespace

void saucer_window_position_ext(saucer_handle* handle, int* x, int* y) {
  @autoreleasepool {
    auto natives = handle->view.parent().native<true>();
    NSWindow* window = natives.window;
    if (window) {
      NSRect frame = [window frame];
      // macOS uses bottom-left origin, convert to top-left
      NSScreen* screen = [window screen] ?: [NSScreen mainScreen];
      CGFloat screenHeight = screen.frame.size.height;
      *x = static_cast<int>(frame.origin.x);
      *y = static_cast<int>(screenHeight - frame.origin.y - frame.size.height);
    } else {
      *x = 0;
      *y = 0;
    }
  }
}

void saucer_window_set_position_ext(saucer_handle* handle, int x, int y) {
  @autoreleasepool {
    auto natives = handle->view.parent().native<true>();
    NSWindow* window = natives.window;
    if (window) {
      NSRect frame = [window frame];
      // macOS uses bottom-left origin, convert from top-left
      NSScreen* screen = [window screen] ?: [NSScreen mainScreen];
      CGFloat screenHeight = screen.frame.size.height;
      CGFloat newY = screenHeight - y - frame.size.height;
      [window setFrameOrigin:NSMakePoint(x, newY)];
    }
  }
}

bool saucer_window_fullscreen_ext(saucer_handle* handle) {
  @autoreleasepool {
    auto natives = handle->view.parent().native<true>();
    NSWindow* window = natives.window;
    if (window) {
      return ([window styleMask] & NSWindowStyleMaskFullScreen) != 0;
    }
    return false;
  }
}

void saucer_window_set_fullscreen_ext(saucer_handle* handle, bool enabled) {
  @autoreleasepool {
    auto natives = handle->view.parent().native<true>();
    NSWindow* window = natives.window;
    if (window) {
      bool isFullscreen = ([window styleMask] & NSWindowStyleMaskFullScreen) != 0;
      if (enabled != isFullscreen) {
        [window toggleFullScreen:nil];
      }
    }
  }
}

double saucer_webview_zoom_ext(saucer_handle* handle) {
  @autoreleasepool {
    auto natives = handle->view.native<true>();
    WKWebView* webview = natives.webview;
    if (webview) {
      // WKWebView uses pageZoom for content zoom
      return webview.pageZoom;
    }
    return 1.0;
  }
}

void saucer_webview_set_zoom_ext(saucer_handle* handle, double level) {
  @autoreleasepool {
    auto natives = handle->view.native<true>();
    WKWebView* webview = natives.webview;
    if (webview) {
      webview.pageZoom = level;
    }
  }
}

#endif // __APPLE__
