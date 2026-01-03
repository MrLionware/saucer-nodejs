/**
 * Linux-specific implementations for premium features
 */

#include "platform.hpp"

#if defined(__linux__) && !defined(__ANDROID__)

#include <gtk/gtk.h>
#include <cstring>

namespace saucer_nodejs {

// ============================================================================
// Clipboard Implementation (Linux/GTK)
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
  
  GtkClipboard* clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
  gchar* text = gtk_clipboard_wait_for_text(clipboard);
  
  if (text) {
    Napi::String result = Napi::String::New(env, text);
    g_free(text);
    return result;
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
  
  GtkClipboard* clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
  gtk_clipboard_set_text(clipboard, text.c_str(), text.length());
  gtk_clipboard_store(clipboard);
}

Napi::Value Clipboard::HasText(const Napi::CallbackInfo& info) {
  GtkClipboard* clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
  bool hasText = gtk_clipboard_wait_is_text_available(clipboard);
  return Napi::Boolean::New(info.Env(), hasText);
}

Napi::Value Clipboard::HasImage(const Napi::CallbackInfo& info) {
  GtkClipboard* clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
  bool hasImage = gtk_clipboard_wait_is_image_available(clipboard);
  return Napi::Boolean::New(info.Env(), hasImage);
}

Napi::Value Clipboard::ReadImage(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  GtkClipboard* clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
  GdkPixbuf* pixbuf = gtk_clipboard_wait_for_image(clipboard);
  
  if (pixbuf) {
    gchar* buffer;
    gsize size;
    GError* error = nullptr;
    
    if (gdk_pixbuf_save_to_buffer(pixbuf, &buffer, &size, "png", &error, nullptr)) {
      Napi::Buffer<uint8_t> result = Napi::Buffer<uint8_t>::Copy(env, 
        reinterpret_cast<uint8_t*>(buffer), size);
      g_free(buffer);
      g_object_unref(pixbuf);
      return result;
    }
    
    if (error) g_error_free(error);
    g_object_unref(pixbuf);
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
  
  GInputStream* stream = g_memory_input_stream_new_from_data(
    buffer.Data(), buffer.Length(), nullptr);
  
  GError* error = nullptr;
  GdkPixbuf* pixbuf = gdk_pixbuf_new_from_stream(stream, nullptr, &error);
  g_object_unref(stream);
  
  if (pixbuf) {
    GtkClipboard* clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_image(clipboard, pixbuf);
    gtk_clipboard_store(clipboard);
    g_object_unref(pixbuf);
  }
  
  if (error) g_error_free(error);
}

void Clipboard::Clear(const Napi::CallbackInfo& info) {
  GtkClipboard* clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
  gtk_clipboard_clear(clipboard);
}

// ============================================================================
// Notification Implementation (Linux/libnotify)
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
  if (info.Length() > 0 && info[0].IsObject()) {
    Napi::Object opts = info[0].As<Napi::Object>();
    
    if (opts.Has("title") && opts.Get("title").IsString()) {
      title_ = opts.Get("title").As<Napi::String>().Utf8Value();
    }
    if (opts.Has("body") && opts.Get("body").IsString()) {
      body_ = opts.Get("body").As<Napi::String>().Utf8Value();
    }
  }
}

Notification::~Notification() {}

void Notification::Show(const Napi::CallbackInfo& info) {
  // Use notify-send as a simple fallback
  // For proper implementation, would use libnotify directly
  std::string cmd = "notify-send \"" + title_ + "\" \"" + body_ + "\" 2>/dev/null &";
  system(cmd.c_str());
}

Napi::Value Notification::IsSupported(const Napi::CallbackInfo& info) {
  // Check if notify-send is available
  int result = system("which notify-send > /dev/null 2>&1");
  return Napi::Boolean::New(info.Env(), result == 0);
}

Napi::Value Notification::RequestPermission(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  auto deferred = Napi::Promise::Deferred::New(env);
  deferred.Resolve(Napi::String::New(env, "granted"));
  return deferred.Promise();
}

// ============================================================================
// SystemTray Implementation (Linux/GTK)
// ============================================================================

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
  // TODO: Initialize GTK status icon (deprecated) or AppIndicator
}

SystemTray::~SystemTray() {
  if (click_tsfn_) {
    click_tsfn_.Release();
  }
}

void SystemTray::SetIcon(const Napi::CallbackInfo& info) {
  // TODO: Implement
}

void SystemTray::SetTooltip(const Napi::CallbackInfo& info) {
  // TODO: Implement
}

void SystemTray::SetMenu(const Napi::CallbackInfo& info) {
  // TODO: Implement
}

void SystemTray::Show(const Napi::CallbackInfo& info) {
  // TODO: Implement
}

void SystemTray::Hide(const Napi::CallbackInfo& info) {
  // TODO: Implement
}

void SystemTray::Destroy(const Napi::CallbackInfo& info) {
  // TODO: Implement
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
}

} // namespace saucer_nodejs

// ============================================================================
// Window/Webview Extensions Implementation (Linux)
// These are in global namespace to match the saucer_handle type
// TODO: Implement using GTK and WebKitGTK APIs
// ============================================================================

void saucer_window_position_ext(saucer_handle* handle, int* x, int* y) {
  // TODO: Use gtk_window_get_position
  *x = 0;
  *y = 0;
}

void saucer_window_set_position_ext(saucer_handle* handle, int x, int y) {
  // TODO: Use gtk_window_move
}

bool saucer_window_fullscreen_ext(saucer_handle* handle) {
  // TODO: Check GdkWindowState
  return false;
}

void saucer_window_set_fullscreen_ext(saucer_handle* handle, bool enabled) {
  // TODO: Use gtk_window_fullscreen / gtk_window_unfullscreen
}

double saucer_webview_zoom_ext(saucer_handle* handle) {
  // TODO: Use webkit_web_view_get_zoom_level
  return 1.0;
}

void saucer_webview_set_zoom_ext(saucer_handle* handle, double level) {
  // TODO: Use webkit_web_view_set_zoom_level
}

#endif // __linux__
