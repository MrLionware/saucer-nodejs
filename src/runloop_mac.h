#pragma once

// Forward declaration
typedef struct saucer_application saucer_application;

namespace saucer_nodejs {

// Transform process type to foreground application (must be called FIRST)
// This should be called BEFORE saucer_application_init()
void TransformToForegroundApp();

// Activate NSApplication for proper macOS app behavior (cmd+tab, Dock, etc)
// Uses multiple techniques for maximum compatibility
// Should be called AFTER saucer_application_init()
void ActivateNSApplication();

// Diagnose current activation policy and print status
// Useful for debugging cmd+tab visibility issues
void DiagnoseActivationPolicy();

// Initialize CFRunLoop integration with libuv
void InitializeRunLoop(saucer_application* app);

// Run one iteration of integrated event loops
void RunOnceIntegrated();

// Cleanup CFRunLoop integration
void CleanupRunLoop();

} // namespace saucer_nodejs
