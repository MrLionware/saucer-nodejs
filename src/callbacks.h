#pragma once

#include <napi.h>
#include <memory>
#include <functional>

namespace saucer_nodejs {

// ============================================================================
// ThreadSafe Callback Wrapper
// Allows C callbacks to safely call JavaScript functions
// ============================================================================

template<typename... Args>
class ThreadSafeCallback {
public:
  ThreadSafeCallback(Napi::Env env, Napi::Function callback) {
    tsfn_ = Napi::ThreadSafeFunction::New(
      env,
      callback,
      "SaucerCallback",
      0, // unlimited queue
      1, // initial thread count
      [](Napi::Env, void*, void*) {} // no context finalizer
    );
  }

  ~ThreadSafeCallback() {
    if (tsfn_) {
      tsfn_.Release();
    }
  }

  // Call the JavaScript function from any thread
  void Call(Args... args) {
    auto data = new std::tuple<Args...>(args...);

    auto status = tsfn_.BlockingCall(data, [](Napi::Env env, Napi::Function jsCallback, auto* data) {
      try {
        CallImpl(env, jsCallback, data, std::index_sequence_for<Args...>{});
      } catch (const Napi::Error& e) {
        // Error already thrown to JS
      }
      delete data;
    });

    if (status != napi_ok) {
      delete data;
    }
  }

  // Call without blocking
  void CallNonBlocking(Args... args) {
    auto data = new std::tuple<Args...>(args...);

    auto status = tsfn_.NonBlockingCall(data, [](Napi::Env env, Napi::Function jsCallback, auto* data) {
      try {
        CallImpl(env, jsCallback, data, std::index_sequence_for<Args...>{});
      } catch (const Napi::Error& e) {
        // Error already thrown to JS
      }
      delete data;
    });

    if (status != napi_ok) {
      delete data;
    }
  }

  void Release() {
    if (tsfn_) {
      tsfn_.Release();
    }
  }

private:
  Napi::ThreadSafeFunction tsfn_;

  template<size_t... Is>
  static void CallImpl(Napi::Env env, Napi::Function jsCallback, std::tuple<Args...>* data, std::index_sequence<Is...>) {
    jsCallback.Call({ConvertArg(env, std::get<Is>(*data))...});
  }

  // Convert C++ types to Napi::Value
  static Napi::Value ConvertArg(Napi::Env env, const std::string& str) {
    return Napi::String::New(env, str);
  }

  static Napi::Value ConvertArg(Napi::Env env, const char* str) {
    return Napi::String::New(env, str ? str : "");
  }

  static Napi::Value ConvertArg(Napi::Env env, bool val) {
    return Napi::Boolean::New(env, val);
  }

  static Napi::Value ConvertArg(Napi::Env env, int val) {
    return Napi::Number::New(env, val);
  }

  static Napi::Value ConvertArg(Napi::Env env, uint64_t val) {
    return Napi::Number::New(env, static_cast<double>(val));
  }
};

// ============================================================================
// Message Handler - for webview.onMessage
// ============================================================================

class MessageHandler {
public:
  MessageHandler(Napi::Env env, Napi::Function callback)
    : callback_(std::make_shared<ThreadSafeCallback<std::string>>(env, callback)) {}

  // C callback that can be passed to saucer_webview_on_message
  static bool OnMessage(void*, const char* message) {
    if (current_handler_ && message) {
      current_handler_->callback_->CallNonBlocking(std::string(message));
      return true;
    }
    return false;
  }

  void SetAsCurrent() {
    current_handler_ = this;
  }

  void Release() {
    if (current_handler_ == this) {
      current_handler_ = nullptr;
    }
    if (callback_) {
      callback_->Release();
    }
  }

private:
  std::shared_ptr<ThreadSafeCallback<std::string>> callback_;
  static thread_local MessageHandler* current_handler_;
};

thread_local MessageHandler* MessageHandler::current_handler_ = nullptr;

// ============================================================================
// Event Callback Wrapper - for window and webview events
// ============================================================================

template<typename... Args>
struct EventCallback {
  std::shared_ptr<ThreadSafeCallback<Args...>> callback;
  void* handle; // saucer_handle*
  uint64_t id;
  bool once;

  EventCallback(Napi::Env env, Napi::Function fn, void* h, uint64_t i, bool o)
    : callback(std::make_shared<ThreadSafeCallback<Args...>>(env, fn))
    , handle(h)
    , id(i)
    , once(o) {}

  void Call(Args... args) {
    callback->CallNonBlocking(args...);
  }

  void Release() {
    if (callback) {
      callback->Release();
    }
  }
};

} // namespace saucer_nodejs
