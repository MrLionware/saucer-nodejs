/**
 * Windows-specific implementations for premium features
 */

#include "platform.hpp"

#ifdef _WIN32

#include <windows.h>
#include <shellapi.h>

namespace saucer_nodejs {

// ============================================================================
// Clipboard Implementation (Windows)
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
  
  if (!OpenClipboard(nullptr)) {
    return Napi::String::New(env, "");
  }
  
  HANDLE hData = GetClipboardData(CF_UNICODETEXT);
  if (hData == nullptr) {
    CloseClipboard();
    return Napi::String::New(env, "");
  }
  
  wchar_t* pszText = static_cast<wchar_t*>(GlobalLock(hData));
  if (pszText == nullptr) {
    CloseClipboard();
    return Napi::String::New(env, "");
  }
  
  // Convert wide string to UTF-8
  int size_needed = WideCharToMultiByte(CP_UTF8, 0, pszText, -1, nullptr, 0, nullptr, nullptr);
  std::string result(size_needed - 1, 0);
  WideCharToMultiByte(CP_UTF8, 0, pszText, -1, &result[0], size_needed, nullptr, nullptr);
  
  GlobalUnlock(hData);
  CloseClipboard();
  
  return Napi::String::New(env, result);
}

void Clipboard::WriteText(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  if (info.Length() == 0 || !info[0].IsString()) {
    Napi::TypeError::New(env, "String required").ThrowAsJavaScriptException();
    return;
  }
  
  std::string text = info[0].As<Napi::String>().Utf8Value();
  
  // Convert UTF-8 to wide string
  int size_needed = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
  std::wstring wtext(size_needed - 1, 0);
  MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &wtext[0], size_needed);
  
  if (!OpenClipboard(nullptr)) {
    return;
  }
  
  EmptyClipboard();
  
  HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, (wtext.size() + 1) * sizeof(wchar_t));
  if (hGlobal) {
    wchar_t* pszDest = static_cast<wchar_t*>(GlobalLock(hGlobal));
    if (pszDest) {
      wcscpy(pszDest, wtext.c_str());
      GlobalUnlock(hGlobal);
      SetClipboardData(CF_UNICODETEXT, hGlobal);
    }
  }
  
  CloseClipboard();
}

Napi::Value Clipboard::HasText(const Napi::CallbackInfo& info) {
  return Napi::Boolean::New(info.Env(), IsClipboardFormatAvailable(CF_UNICODETEXT));
}

Napi::Value Clipboard::HasImage(const Napi::CallbackInfo& info) {
  return Napi::Boolean::New(info.Env(), 
    IsClipboardFormatAvailable(CF_BITMAP) || IsClipboardFormatAvailable(CF_DIB));
}

Napi::Value Clipboard::ReadImage(const Napi::CallbackInfo& info) {
  // TODO: Implement image reading for Windows
  return info.Env().Null();
}

void Clipboard::WriteImage(const Napi::CallbackInfo& info) {
  // TODO: Implement image writing for Windows
}

void Clipboard::Clear(const Napi::CallbackInfo& info) {
  if (OpenClipboard(nullptr)) {
    EmptyClipboard();
    CloseClipboard();
  }
}

// ============================================================================
// Notification Implementation (Windows)
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
  // Simple balloon notification using Shell_NotifyIcon
  // For full Windows 10/11 toast notifications, would need WinRT APIs
  
  NOTIFYICONDATAW nid = {0};
  nid.cbSize = sizeof(nid);
  nid.uFlags = NIF_INFO;
  nid.dwInfoFlags = NIIF_INFO;
  
  // Convert title and body to wide strings
  int titleSize = MultiByteToWideChar(CP_UTF8, 0, title_.c_str(), -1, nullptr, 0);
  int bodySize = MultiByteToWideChar(CP_UTF8, 0, body_.c_str(), -1, nullptr, 0);
  
  MultiByteToWideChar(CP_UTF8, 0, title_.c_str(), -1, nid.szInfoTitle, 
    min(titleSize, static_cast<int>(sizeof(nid.szInfoTitle) / sizeof(wchar_t))));
  MultiByteToWideChar(CP_UTF8, 0, body_.c_str(), -1, nid.szInfo, 
    min(bodySize, static_cast<int>(sizeof(nid.szInfo) / sizeof(wchar_t))));
  
  Shell_NotifyIconW(NIM_MODIFY, &nid);
}

Napi::Value Notification::IsSupported(const Napi::CallbackInfo& info) {
  return Napi::Boolean::New(info.Env(), true);
}

Napi::Value Notification::RequestPermission(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  auto deferred = Napi::Promise::Deferred::New(env);
  deferred.Resolve(Napi::String::New(env, "granted"));
  return deferred.Promise();
}

// ============================================================================
// SystemTray Implementation (Windows)
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
  // TODO: Initialize Windows system tray icon
}

SystemTray::~SystemTray() {
  // TODO: Cleanup Windows system tray icon
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
// Window/Webview Extensions Implementation (Windows)
// These are in global namespace to match the saucer_handle type
// TODO: Implement using Win32 and WebView2 APIs
// ============================================================================

void saucer_window_position_ext(saucer_handle* handle, int* x, int* y) {
  // TODO: Use GetWindowRect
  *x = 0;
  *y = 0;
}

void saucer_window_set_position_ext(saucer_handle* handle, int x, int y) {
  // TODO: Use SetWindowPos
}

bool saucer_window_fullscreen_ext(saucer_handle* handle) {
  // TODO: Check window style
  return false;
}

void saucer_window_set_fullscreen_ext(saucer_handle* handle, bool enabled) {
  // TODO: Toggle fullscreen using window style manipulation
}

double saucer_webview_zoom_ext(saucer_handle* handle) {
  // TODO: Use ICoreWebView2Controller::get_ZoomFactor
  return 1.0;
}

void saucer_webview_set_zoom_ext(saucer_handle* handle, double level) {
  // TODO: Use ICoreWebView2Controller::put_ZoomFactor
}

#endif // _WIN32
