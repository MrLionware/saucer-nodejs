/**
 * Platform-specific premium features for saucer-nodejs
 * 
 * This file provides implementations for:
 * - Clipboard API
 * - System Notifications
 * - System Tray (future)
 * - Progress Bar (future)
 */

#pragma once

#include <napi.h>
#include <string>
#include <vector>

namespace saucer_nodejs {

// ============================================================================
// Clipboard Class - Cross-platform clipboard access
// ============================================================================

class Clipboard : public Napi::ObjectWrap<Clipboard> {
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  Clipboard(const Napi::CallbackInfo& info);

private:
  // Text operations
  static Napi::Value ReadText(const Napi::CallbackInfo& info);
  static void WriteText(const Napi::CallbackInfo& info);
  
  // Image operations (future)
  static Napi::Value ReadImage(const Napi::CallbackInfo& info);
  static void WriteImage(const Napi::CallbackInfo& info);
  
  // Check operations
  static Napi::Value HasText(const Napi::CallbackInfo& info);
  static Napi::Value HasImage(const Napi::CallbackInfo& info);
  
  // Clear
  static void Clear(const Napi::CallbackInfo& info);
};

// ============================================================================
// Notification Class - System notifications
// ============================================================================

class Notification : public Napi::ObjectWrap<Notification> {
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  Notification(const Napi::CallbackInfo& info);
  ~Notification();

private:
  std::string title_;
  std::string body_;
  std::string icon_path_;
  
  void Show(const Napi::CallbackInfo& info);
  
  static Napi::Value IsSupported(const Napi::CallbackInfo& info);
  static Napi::Value RequestPermission(const Napi::CallbackInfo& info);
};

// ============================================================================
// SystemTray Class - System tray icon (macOS menu bar, Windows system tray)
// ============================================================================

class SystemTray : public Napi::ObjectWrap<SystemTray> {
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  SystemTray(const Napi::CallbackInfo& info);
  ~SystemTray();

private:
  void* native_tray_ = nullptr;
  Napi::ThreadSafeFunction click_tsfn_;
  
  void SetIcon(const Napi::CallbackInfo& info);
  void SetTooltip(const Napi::CallbackInfo& info);
  void SetMenu(const Napi::CallbackInfo& info);
  void Show(const Napi::CallbackInfo& info);
  void Hide(const Napi::CallbackInfo& info);
  void Destroy(const Napi::CallbackInfo& info);
  
  void OnClick(const Napi::CallbackInfo& info);
};

} // namespace saucer_nodejs

// ============================================================================
// Window/Webview Extensions - Functions not in saucer C bindings
// These operate directly on native platform objects
// Declared in global namespace since saucer_handle is defined globally
// ============================================================================

// Forward declare the saucer handle type (from saucer bindings)
struct saucer_handle;

// Window Position
void saucer_window_position_ext(saucer_handle* handle, int* x, int* y);
void saucer_window_set_position_ext(saucer_handle* handle, int x, int y);

// Window Fullscreen
bool saucer_window_fullscreen_ext(saucer_handle* handle);
void saucer_window_set_fullscreen_ext(saucer_handle* handle, bool enabled);

// Webview Zoom
double saucer_webview_zoom_ext(saucer_handle* handle);
void saucer_webview_set_zoom_ext(saucer_handle* handle, double level);
