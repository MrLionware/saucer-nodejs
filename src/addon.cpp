#include <napi.h>

#include <uv.h>

#include <saucer/app.h>

#include <saucer/webview.h>

#include <saucer/window.h>

#include <saucer/options.h>

#include <saucer/preferences.h>

#include <saucer/script.h>

#include <saucer/scheme.h>

#include <saucer/stash.h>

#include <saucer/icon.h>

#include <saucer/navigation.h>

#include <saucer/memory.h>

#include <saucer/desktop.h>

#include <saucer/pdf.h>

#include "private/webview.hpp"

#include <glaze/glaze.hpp>
#include <glaze/json/generic.hpp>
#include <glaze/json/read.hpp>

#include <saucer/serializers/glaze/glaze.hpp>

#include <memory>

#include <unordered_map>

#include <queue>

#include <mutex>

#include <vector>

#include <algorithm>

#include <string>

#include <tuple>

#include <future>

#include <string>



#ifdef __APPLE__

#include "runloop_mac.h"

#endif

// Platform-specific premium features
#include "platform.hpp"

// Glaze v6.4 declares a generic fallback for convert_from_generic but does not
// provide a direct generic_json -> generic_json definition in all toolchains.
// MSVC can instantiate that unresolved path while parsing nested containers.
namespace glz {
template <num_mode Mode>
inline error_ctx convert_from_generic(generic_json<Mode>& result, const generic_json<Mode>& source) {
  result = source;
  return {};
}
} // namespace glz



namespace saucer_nodejs {



// Helper to convert C string to JS string and free it

inline Napi::String CStrToJS(Napi::Env env, char* str) {

  if (!str) return Napi::String::New(env, "");

  auto result = Napi::String::New(env, str);

  saucer_memory_free(str);

  return result;

}



// Forward declarations

class Application;

class Webview;



// ============================================================================

// Application Class - Manages the saucer application and event loop integration

// ============================================================================



class Application : public Napi::ObjectWrap<Application> {

public:

  static Napi::Object Init(Napi::Env env, Napi::Object exports);

  Application(const Napi::CallbackInfo& info);

  ~Application();



  // Public access to app handle (needed by Webview)

  saucer_application* GetApp() { return app_; }



private:

  saucer_application* app_ = nullptr;

  uv_timer_t* timer_handle_ = nullptr;

  uv_check_t* check_handle_ = nullptr;

  uv_prepare_t* prepare_handle_ = nullptr;

  bool running_ = false;

  uint64_t last_check_time_ = 0;

  bool owns_app_handle_ = false;



  // Methods

  Napi::Value IsThreadSafe(const Napi::CallbackInfo& info);

  Napi::Value Quit(const Napi::CallbackInfo& info);

  void Run(const Napi::CallbackInfo& info);

  void Post(const Napi::CallbackInfo& info);

  Napi::Value Dispatch(const Napi::CallbackInfo& info);

  Napi::Value PoolSubmit(const Napi::CallbackInfo& info);

  Napi::Value PoolEmplace(const Napi::CallbackInfo& info);

  Napi::Value Make(const Napi::CallbackInfo& info);

  Napi::Value Native(const Napi::CallbackInfo& info);



  // Static helpers

  static Napi::Value InitApp(const Napi::CallbackInfo& info);

  static Napi::Value Active(const Napi::CallbackInfo& info);



  // Event loop integration

  static void OnTimer(uv_timer_t* handle);

  static void OnCheck(uv_check_t* handle);

  static void OnPrepare(uv_prepare_t* handle);

  void StartEventLoop();

  void StopEventLoop();



  // Callback plumbing

  struct PostTask {

    Napi::ThreadSafeFunction tsfn;

  };



  struct DispatchTask {

    Napi::ThreadSafeFunction tsfn;

    std::shared_ptr<Napi::Promise::Deferred> deferred;

  };



  struct PoolTask {

    Napi::ThreadSafeFunction tsfn;

    std::shared_ptr<Napi::Promise::Deferred> deferred;

  };



  static void ProcessPostTask();

  static void ProcessDispatchTask();

  static void ProcessPoolTask();



  // Global task queues (one application is expected)

  static std::mutex post_mutex_;

  static std::queue<std::shared_ptr<PostTask>> post_queue_;



  static std::mutex dispatch_mutex_;

  static std::queue<std::shared_ptr<DispatchTask>> dispatch_queue_;



  static std::mutex pool_mutex_;

  static std::queue<std::shared_ptr<PoolTask>> pool_queue_;



  // Track active JS instance for singleton behavior

  static std::mutex active_mutex_;

  static Napi::ObjectReference active_instance_;

};



// ============================================================================

// Webview Class - Wraps saucer webview with event callbacks

// ============================================================================



class Webview : public Napi::ObjectWrap<Webview> {

public:

  static Napi::Object Init(Napi::Env env, Napi::Object exports);

  Webview(const Napi::CallbackInfo& info);

  ~Webview();

  // Public accessor for PDF module
  saucer_handle* GetWebview() { return webview_; }

private:

  saucer_handle* webview_ = nullptr;

  saucer_application* app_ = nullptr;

  Napi::ObjectReference parent_ref_;



  // Thread-safe function for callbacks

  struct CallbackData {

    Napi::ThreadSafeFunction tsfn;

    uint64_t id;

    bool once;

    Napi::FunctionReference callback_ref;

  };

  std::unordered_map<std::string, std::vector<std::shared_ptr<CallbackData>>> event_callbacks_;

  std::mutex callback_mutex_;



  Napi::ThreadSafeFunction message_tsfn_;

  Napi::FunctionReference message_handler_ref_;

  bool minimized_hint_valid_ = false;

  bool minimized_hint_ = false;

  std::string preload_script_;



  struct ExposedCallback {

    std::string name;

    std::shared_ptr<Napi::ThreadSafeFunction> tsfn;

  };



  std::vector<std::shared_ptr<ExposedCallback>> exposed_callbacks_;

  std::mutex exposed_mutex_;



  static std::unordered_map<saucer_handle*, Webview*> instances_;

  static std::mutex instance_mutex_;



  // Window methods
  Napi::Value GetFocused(const Napi::CallbackInfo& info);

  Napi::Value GetVisible(const Napi::CallbackInfo& info);

  void SetVisible(const Napi::CallbackInfo& info, const Napi::Value& value);

  Napi::Value GetMinimized(const Napi::CallbackInfo& info);

  void SetMinimized(const Napi::CallbackInfo& info, const Napi::Value& value);

  Napi::Value GetMaximized(const Napi::CallbackInfo& info);

  void SetMaximized(const Napi::CallbackInfo& info, const Napi::Value& value);

  Napi::Value GetResizable(const Napi::CallbackInfo& info);

  void SetResizable(const Napi::CallbackInfo& info, const Napi::Value& value);

  Napi::Value GetDecorations(const Napi::CallbackInfo& info);

  void SetDecorations(const Napi::CallbackInfo& info, const Napi::Value& value);

  Napi::Value GetAlwaysOnTop(const Napi::CallbackInfo& info);

  void SetAlwaysOnTop(const Napi::CallbackInfo& info, const Napi::Value& value);

  Napi::Value GetClickThrough(const Napi::CallbackInfo& info);

  void SetClickThrough(const Napi::CallbackInfo& info, const Napi::Value& value);

  Napi::Value GetTitle(const Napi::CallbackInfo& info);

  void SetTitle(const Napi::CallbackInfo& info, const Napi::Value& value);

  Napi::Value GetSize(const Napi::CallbackInfo& info);

  void SetSize(const Napi::CallbackInfo& info, const Napi::Value& value);

  Napi::Value GetMaxSize(const Napi::CallbackInfo& info);

  void SetMaxSize(const Napi::CallbackInfo& info, const Napi::Value& value);

  Napi::Value GetMinSize(const Napi::CallbackInfo& info);

  void SetMinSize(const Napi::CallbackInfo& info, const Napi::Value& value);

  // Position (extension - not in saucer C bindings)
  Napi::Value GetPosition(const Napi::CallbackInfo& info);
  void SetPosition(const Napi::CallbackInfo& info, const Napi::Value& value);

  // Fullscreen (extension - not in saucer C bindings)
  Napi::Value GetFullscreen(const Napi::CallbackInfo& info);
  void SetFullscreen(const Napi::CallbackInfo& info, const Napi::Value& value);

  // Zoom (extension - not in saucer C bindings)
  Napi::Value GetZoom(const Napi::CallbackInfo& info);
  void SetZoom(const Napi::CallbackInfo& info, const Napi::Value& value);

  Napi::Value GetParent(const Napi::CallbackInfo& info);

  void Show(const Napi::CallbackInfo& info);

  void Hide(const Napi::CallbackInfo& info);

  void Close(const Napi::CallbackInfo& info);

  void Focus(const Napi::CallbackInfo& info);

  void StartDrag(const Napi::CallbackInfo& info);

  void StartResize(const Napi::CallbackInfo& info);

  void SetIcon(const Napi::CallbackInfo& info);



  // Webview methods

  Napi::Value GetUrl(const Napi::CallbackInfo& info);

  void SetUrl(const Napi::CallbackInfo& info, const Napi::Value& value);

  Napi::Value GetDevTools(const Napi::CallbackInfo& info);

  void SetDevTools(const Napi::CallbackInfo& info, const Napi::Value& value);

  Napi::Value GetBackgroundColor(const Napi::CallbackInfo& info);

  void SetBackgroundColor(const Napi::CallbackInfo& info, const Napi::Value& value);

  Napi::Value GetForceDarkMode(const Napi::CallbackInfo& info);

  void SetForceDarkMode(const Napi::CallbackInfo& info, const Napi::Value& value);

  Napi::Value GetContextMenu(const Napi::CallbackInfo& info);

  void SetContextMenu(const Napi::CallbackInfo& info, const Napi::Value& value);

  Napi::Value GetFavicon(const Napi::CallbackInfo& info);

  Napi::Value GetPageTitle(const Napi::CallbackInfo& info);

  void Navigate(const Napi::CallbackInfo& info);

  void SetFile(const Napi::CallbackInfo& info);

  void LoadHtml(const Napi::CallbackInfo& info);

  void Execute(const Napi::CallbackInfo& info);

  void Reload(const Napi::CallbackInfo& info);

  void Back(const Napi::CallbackInfo& info);

  void Forward(const Napi::CallbackInfo& info);

  void Expose(const Napi::CallbackInfo& info);

  void ClearExposed(const Napi::CallbackInfo& info);

  Napi::Value Evaluate(const Napi::CallbackInfo& info);

  void Inject(const Napi::CallbackInfo& info);

  void Embed(const Napi::CallbackInfo& info);

  void Serve(const Napi::CallbackInfo& info);

  void ClearEmbedded(const Napi::CallbackInfo& info);

  void ClearScripts(const Napi::CallbackInfo& info);

  void HandleScheme(const Napi::CallbackInfo& info);

  void RemoveScheme(const Napi::CallbackInfo& info);

  static void RegisterScheme(const Napi::CallbackInfo& info);

  // Scheme handler storage
  struct SchemeHandler {
    std::string name;
    std::shared_ptr<Napi::ThreadSafeFunction> tsfn;
  };

  std::vector<std::shared_ptr<SchemeHandler>> scheme_handlers_;
  std::mutex scheme_mutex_;



  // Event handling

  void On(const Napi::CallbackInfo& info);

  void Once(const Napi::CallbackInfo& info);

  void Off(const Napi::CallbackInfo& info);



  // Message handling

  void OnMessage(const Napi::CallbackInfo& info);



  // Helper methods

  static bool MessageCallback(saucer_handle* handle, const char* message);

  static std::string StringifyForRPC(Napi::Env env, Napi::Value value);
  static glz::json_t SerializeForRPC(Napi::Env env, Napi::Value value);

  static Napi::Value ParseJson(Napi::Env env, const std::string& json);

  static std::vector<glz::json_t> CollectJsonArgs(const Napi::CallbackInfo& info, size_t startIndex);

  static Webview* FromHandle(saucer_handle* handle);



  // Event helpers

  bool RegisterEvent(const std::string& event, Napi::Function cb, bool once);

  void EmitEvent(const std::string& event, std::function<void(Napi::Env, Napi::Function)> invoker);

  bool EvaluatePolicy(const std::string& event, std::function<Napi::Value(Napi::Env, Napi::Function)> invoker);

  void RemoveOnceCallbacks(const std::string& event);

  void RemoveCallbackById(const std::string& event, uint64_t id);

  void RemoveCallbackByFunction(const std::string& event, Napi::Function cb);

  void RemoveAllCallbacks(const std::string& event);

  bool HasCallbacks(const std::string& event);



  // Window event forwarders

  static void OnWindowDecorated(saucer_handle* handle, bool decorated);

  static void OnWindowMaximize(saucer_handle* handle, bool maximized);

  static void OnWindowMinimize(saucer_handle* handle, bool minimized);

  static void OnWindowClosed(saucer_handle* handle);

  static void OnWindowResize(saucer_handle* handle, int width, int height);

  static void OnWindowFocus(saucer_handle* handle, bool focused);

  static SAUCER_POLICY OnWindowClose(saucer_handle* handle);



  // Webview event forwarders

  static void OnWebDomReady(saucer_handle* handle);

  static void OnWebNavigated(saucer_handle* handle, const char* url);

  static SAUCER_POLICY OnWebNavigate(saucer_handle* handle, saucer_navigation* nav);

  static void OnWebFavicon(saucer_handle* handle, saucer_icon* icon);

  static void OnWebTitle(saucer_handle* handle, const char* title);

  static void OnWebLoad(saucer_handle* handle, SAUCER_STATE state);

};



// ============================================================================

// Application Implementation

// ============================================================================



std::mutex Application::post_mutex_;

std::queue<std::shared_ptr<Application::PostTask>> Application::post_queue_;



std::mutex Application::dispatch_mutex_;

std::queue<std::shared_ptr<Application::DispatchTask>> Application::dispatch_queue_;



std::mutex Application::pool_mutex_;

std::queue<std::shared_ptr<Application::PoolTask>> Application::pool_queue_;



std::mutex Application::active_mutex_;

Napi::ObjectReference Application::active_instance_;



Napi::Object Application::Init(Napi::Env env, Napi::Object exports) {

  Napi::Function func = DefineClass(env, "Application", {

    StaticMethod("init", &Application::InitApp),

    StaticMethod("active", &Application::Active),



    InstanceMethod("isThreadSafe", &Application::IsThreadSafe),

    InstanceMethod("quit", &Application::Quit),

    InstanceMethod("run", &Application::Run),

    InstanceMethod("post", &Application::Post),

    InstanceMethod("dispatch", &Application::Dispatch),

    InstanceMethod("poolSubmit", &Application::PoolSubmit),

    InstanceMethod("poolEmplace", &Application::PoolEmplace),

    InstanceMethod("make", &Application::Make),

    InstanceMethod("native", &Application::Native),

    InstanceMethod("nativeHandle", &Application::Native),

  });



  Napi::FunctionReference* constructor = new Napi::FunctionReference();

  *constructor = Napi::Persistent(func);

  env.SetInstanceData(constructor);



  exports.Set("Application", func);

  return exports;

}



Application::Application(const Napi::CallbackInfo& info)

  : Napi::ObjectWrap<Application>(info) {

  Napi::Env env = info.Env();



  // Support wrapping an existing native handle (from Application.active)

  const bool wrapping_existing = info.Length() > 0 && info[0].IsExternal();



  {

    std::scoped_lock lock(active_mutex_);

    if (!active_instance_.IsEmpty() && !wrapping_existing) {

      Napi::Error::New(env, "Application already initialized. Use Application.active() instead.").ThrowAsJavaScriptException();

      return;

    }

  }



  if (wrapping_existing) {

    app_ = info[0].As<Napi::External<saucer_application>>().Data();

    owns_app_handle_ = true;

  } else {

#ifdef __APPLE__

    // CRITICAL: Transform process type BEFORE initializing NSApplication

    // This must happen before saucer_application_init() creates NSApplication

    TransformToForegroundApp();

#endif



    // Parse options

    saucer_options* options = saucer_options_new("com.saucer.nodejs");



    if (info.Length() > 0 && info[0].IsObject()) {

      Napi::Object opts = info[0].As<Napi::Object>();



      if (opts.Has("id") && opts.Get("id").IsString()) {

        std::string id = opts.Get("id").As<Napi::String>().Utf8Value();

        if (!id.empty()) {

          saucer_options_free(options);

          options = saucer_options_new(id.c_str());

        }

      }



      if (opts.Has("threads")) {

        size_t threads = opts.Get("threads").As<Napi::Number>().Uint32Value();

        saucer_options_set_threads(options, threads);

      }

    }



    // Initialize application (this creates NSApplication on macOS)

    app_ = saucer_application_init(options);

    owns_app_handle_ = true;

    saucer_options_free(options);



    if (!app_) {

      Napi::Error::New(env, "Failed to initialize saucer application").ThrowAsJavaScriptException();

      return;

    }



#ifdef __APPLE__

    // Verify and complete activation (after NSApplication is created)

    ActivateNSApplication();

#endif

  }



  // Start event loop integration

  StartEventLoop();



  // Cache the active instance so Application.active() can return it

  std::scoped_lock lock(active_mutex_);

  if (active_instance_.IsEmpty()) {

    active_instance_ = Napi::Persistent(info.This().As<Napi::Object>());

    active_instance_.SuppressDestruct();

  }

}



Application::~Application() {

  StopEventLoop();



  if (app_ && owns_app_handle_) {

    saucer_application_free(app_);

    app_ = nullptr;

  }



  {

    std::scoped_lock lock(active_mutex_);

    if (!active_instance_.IsEmpty() && active_instance_.Value().StrictEquals(Value())) {

      active_instance_.Reset();

    }

  }

}



Napi::Value Application::InitApp(const Napi::CallbackInfo& info) {

  Napi::Env env = info.Env();

  {

    std::scoped_lock lock(active_mutex_);

    if (!active_instance_.IsEmpty()) {

      return active_instance_.Value();

    }

  }



  auto* constructor = env.GetInstanceData<Napi::FunctionReference>();

  std::vector<napi_value> args;

  if (info.Length() > 0) {

    args.push_back(info[0]);

  }



  Napi::Object instance = constructor->New(args);



  {

    std::scoped_lock lock(active_mutex_);

    if (active_instance_.IsEmpty()) {

      active_instance_ = Napi::Persistent(instance);

      active_instance_.SuppressDestruct();

    }

    return active_instance_.Value();

  }

}



Napi::Value Application::Active(const Napi::CallbackInfo& info) {

  Napi::Env env = info.Env();

  {

    std::scoped_lock lock(active_mutex_);

    if (!active_instance_.IsEmpty()) {

      return active_instance_.Value();

    }

  }



  saucer_application* handle = saucer_application_active();

  if (!handle) {

    return env.Null();

  }



  auto* constructor = env.GetInstanceData<Napi::FunctionReference>();

  Napi::Object instance = constructor->New({ Napi::External<saucer_application>::New(env, handle) });



  {

    std::scoped_lock lock(active_mutex_);

    if (active_instance_.IsEmpty()) {

      active_instance_ = Napi::Persistent(instance);

      active_instance_.SuppressDestruct();

    }

    return active_instance_.Value();

  }

}



void Application::StartEventLoop() {

  if (check_handle_) return;



  // Hybrid approach: uv_check_t (runs after I/O, less intrusive to JS execution)

  // with a fallback timer for when the event loop is idle

  check_handle_ = new uv_check_t();

  check_handle_->data = this;

  uv_check_init(uv_default_loop(), check_handle_);

  uv_check_start(check_handle_, OnCheck);

  uv_unref(reinterpret_cast<uv_handle_t*>(check_handle_));  // Don't keep loop alive



  // Also run before I/O (prepare phase) to catch events before blocking

  prepare_handle_ = new uv_prepare_t();

  prepare_handle_->data = this;

  uv_prepare_init(uv_default_loop(), prepare_handle_);

  uv_prepare_start(prepare_handle_, OnPrepare);

  uv_unref(reinterpret_cast<uv_handle_t*>(prepare_handle_));



  // Fallback timer at 1ms (approx 1000fps) for maximum smoothness

  timer_handle_ = new uv_timer_t();

  timer_handle_->data = this;

  uv_timer_init(uv_default_loop(), timer_handle_);

  uv_timer_start(timer_handle_, OnTimer, 0, 1);

  uv_ref(reinterpret_cast<uv_handle_t*>(timer_handle_));



  running_ = true;

}



void Application::StopEventLoop() {

  running_ = false;



  if (check_handle_) {

    uv_check_stop(check_handle_);

    uv_close(reinterpret_cast<uv_handle_t*>(check_handle_),

      [](uv_handle_t* handle) {

        delete reinterpret_cast<uv_check_t*>(handle);

      });

    check_handle_ = nullptr;

  }



  if (prepare_handle_) {

    uv_prepare_stop(prepare_handle_);

    uv_close(reinterpret_cast<uv_handle_t*>(prepare_handle_),

      [](uv_handle_t* handle) {

        delete reinterpret_cast<uv_prepare_t*>(handle);

      });

    prepare_handle_ = nullptr;

  }



  if (timer_handle_) {

    uv_timer_stop(timer_handle_);

    uv_close(reinterpret_cast<uv_handle_t*>(timer_handle_),

      [](uv_handle_t* handle) {

        delete reinterpret_cast<uv_timer_t*>(handle);

      });

    timer_handle_ = nullptr;

  }

}



// This callback runs every 16ms (60fps timer)

// It calls saucer_application_run_once() to process saucer events

void Application::OnTimer(uv_timer_t* handle) {

  Application* app = static_cast<Application*>(handle->data);

  if (app && app->running_ && app->app_) {

    // Run one iteration of saucer's event loop

    // This is non-blocking and processes pending events

    saucer_application_run_once(app->app_);

  }

}



// This callback runs after I/O on each event loop iteration

// It's throttled to avoid running too frequently and interfering with JS execution

void Application::OnCheck(uv_check_t* handle) {

  Application* app = static_cast<Application*>(handle->data);

  if (app && app->running_ && app->app_) {

    // Run on every check to ensure maximum responsiveness

    // This allows the webview to update as fast as possible

    saucer_application_run_once(app->app_);

  }

}



// This callback runs before I/O on each event loop iteration

void Application::OnPrepare(uv_prepare_t* handle) {

  Application* app = static_cast<Application*>(handle->data);

  if (app && app->running_ && app->app_) {

    // Run before polling to ensure we don't wait for I/O if there are UI events

    saucer_application_run_once(app->app_);

  }

}



void Application::Run(const Napi::CallbackInfo& info) {

  // In our integrated approach, we don't need to call run()

  // The event loop is already running via uv_timer_t

  // This method exists for compatibility but does nothing

}



Napi::Value Application::IsThreadSafe(const Napi::CallbackInfo& info) {

  bool result = app_ ? saucer_application_thread_safe(app_) : false;

  return Napi::Boolean::New(info.Env(), result);

}



Napi::Value Application::Quit(const Napi::CallbackInfo& info) {

  if (app_) {

    saucer_application_quit(app_);

  }

  return info.Env().Undefined();

}



void Application::Post(const Napi::CallbackInfo& info) {

  Napi::Env env = info.Env();



  if (info.Length() == 0 || !info[0].IsFunction()) {

    Napi::TypeError::New(env, "Callback must be a function").ThrowAsJavaScriptException();

    return;

  }



  if (!app_) {

    Napi::Error::New(env, "Application handle is not available").ThrowAsJavaScriptException();

    return;

  }



  auto tsfn = Napi::ThreadSafeFunction::New(

    env,

    info[0].As<Napi::Function>(),

    "saucer.application.post",

    0,

    1

  );



  auto task = std::make_shared<PostTask>();

  task->tsfn = tsfn;



  {

    std::scoped_lock lock(post_mutex_);

    post_queue_.push(task);

  }



  saucer_application_post(app_, &Application::ProcessPostTask);

}



Napi::Value Application::Dispatch(const Napi::CallbackInfo& info) {

  Napi::Env env = info.Env();



  if (info.Length() == 0 || !info[0].IsFunction()) {

    Napi::TypeError::New(env, "Callback must be a function").ThrowAsJavaScriptException();

    return env.Undefined();

  }



  if (!app_) {

    Napi::Error::New(env, "Application handle is not available").ThrowAsJavaScriptException();

    return env.Undefined();

  }



  auto deferred = std::make_shared<Napi::Promise::Deferred>(Napi::Promise::Deferred::New(env));

  auto tsfn = Napi::ThreadSafeFunction::New(

    env,

    info[0].As<Napi::Function>(),

    "saucer.application.dispatch",

    0,

    1

  );



  auto task = std::make_shared<DispatchTask>();

  task->tsfn = tsfn;

  task->deferred = deferred;



  {

    std::scoped_lock lock(dispatch_mutex_);

    dispatch_queue_.push(task);

  }



  saucer_application_post(app_, &Application::ProcessDispatchTask);

  return deferred->Promise();

}



Napi::Value Application::PoolSubmit(const Napi::CallbackInfo& info) {

  Napi::Env env = info.Env();



  if (info.Length() == 0 || !info[0].IsFunction()) {

    Napi::TypeError::New(env, "Callback must be a function").ThrowAsJavaScriptException();

    return env.Undefined();

  }



  if (!app_) {

    Napi::Error::New(env, "Application handle is not available").ThrowAsJavaScriptException();

    return env.Undefined();

  }



  auto deferred = std::make_shared<Napi::Promise::Deferred>(Napi::Promise::Deferred::New(env));

  auto tsfn = Napi::ThreadSafeFunction::New(

    env,

    info[0].As<Napi::Function>(),

    "saucer.application.poolSubmit",

    0,

    1

  );



  auto task = std::make_shared<PoolTask>();

  task->tsfn = tsfn;

  task->deferred = deferred;



  {

    std::scoped_lock lock(pool_mutex_);

    pool_queue_.push(task);

  }



  // Use non-blocking emplace to avoid deadlocks while still awaiting completion via the promise

  saucer_application_pool_emplace(app_, &Application::ProcessPoolTask);

  return deferred->Promise();

}



Napi::Value Application::PoolEmplace(const Napi::CallbackInfo& info) {

  Napi::Env env = info.Env();



  if (info.Length() == 0 || !info[0].IsFunction()) {

    Napi::TypeError::New(env, "Callback must be a function").ThrowAsJavaScriptException();

    return env.Undefined();

  }



  if (!app_) {

    Napi::Error::New(env, "Application handle is not available").ThrowAsJavaScriptException();

    return env.Undefined();

  }



  auto tsfn = Napi::ThreadSafeFunction::New(

    env,

    info[0].As<Napi::Function>(),

    "saucer.application.poolEmplace",

    0,

    1

  );



  auto task = std::make_shared<PoolTask>();

  task->tsfn = tsfn;



  {

    std::scoped_lock lock(pool_mutex_);

    pool_queue_.push(task);

  }



  saucer_application_pool_emplace(app_, &Application::ProcessPoolTask);

  return env.Undefined();

}



Napi::Value Application::Make(const Napi::CallbackInfo& info) {

  // JS equivalent: run the factory on the UI thread and return its result

  return Dispatch(info);

}



Napi::Value Application::Native(const Napi::CallbackInfo& info) {

  if (!app_) {

    return info.Env().Null();

  }



  const uint64_t ptr = reinterpret_cast<uint64_t>(app_);

  return Napi::BigInt::New(info.Env(), ptr);

}



void Application::ProcessPostTask() {

  std::shared_ptr<PostTask> task;

  {

    std::scoped_lock lock(post_mutex_);

    if (post_queue_.empty()) return;

    task = post_queue_.front();

    post_queue_.pop();

  }



  task->tsfn.NonBlockingCall([](Napi::Env env, Napi::Function jsCallback) {

    Napi::HandleScope scope(env);

    try {

      jsCallback.Call({});

    } catch (const Napi::Error& err) {

      err.ThrowAsJavaScriptException();

    } catch (...) {

      Napi::Error::New(env, "post callback failed").ThrowAsJavaScriptException();

    }

  });



  task->tsfn.Release();

}



void Application::ProcessDispatchTask() {

  std::shared_ptr<DispatchTask> task;

  {

    std::scoped_lock lock(dispatch_mutex_);

    if (dispatch_queue_.empty()) return;

    task = dispatch_queue_.front();

    dispatch_queue_.pop();

  }



  // Keep task alive until JS callback runs

  auto* holder = new std::shared_ptr<DispatchTask>(task);



  task->tsfn.NonBlockingCall(holder,

    [](Napi::Env env, Napi::Function jsCallback, std::shared_ptr<DispatchTask>* holder) {

      auto& data = **holder;

      Napi::HandleScope scope(env);

      try {

        Napi::Value result = jsCallback.Call({});

        if (data.deferred) {

          data.deferred->Resolve(result);

        }

      } catch (const Napi::Error& err) {

        if (data.deferred) {

          data.deferred->Reject(err.Value());

        }

      } catch (...) {

        if (data.deferred) {

          data.deferred->Reject(Napi::String::New(env, "dispatch callback failed"));

        }

      }



      data.tsfn.Release();

      delete holder;

    });

}



void Application::ProcessPoolTask() {

  std::shared_ptr<PoolTask> task;

  {

    std::scoped_lock lock(pool_mutex_);

    if (pool_queue_.empty()) return;

    task = pool_queue_.front();

    pool_queue_.pop();

  }



  // Keep task alive until JS callback runs

  auto* holder = new std::shared_ptr<PoolTask>(task);



  task->tsfn.NonBlockingCall(holder,

    [](Napi::Env env, Napi::Function jsCallback, std::shared_ptr<PoolTask>* holder) {

      auto& data = **holder;

      Napi::HandleScope scope(env);

      try {

        Napi::Value result = jsCallback.Call({});

        if (data.deferred) {

          data.deferred->Resolve(result);

        }

      } catch (const Napi::Error& err) {

        if (data.deferred) {

          data.deferred->Reject(err.Value());

        }

      } catch (...) {

        if (data.deferred) {

          data.deferred->Reject(Napi::String::New(env, "pool callback failed"));

        }

      }



      data.tsfn.Release();

      delete holder;

    });

}



// ============================================================================

// Webview Implementation

// ============================================================================



std::unordered_map<saucer_handle*, Webview*> Webview::instances_;

std::mutex Webview::instance_mutex_;



static bool MapWindowEventName(const std::string& name, SAUCER_WINDOW_EVENT& out) {

  if (name == "decorated") { out = SAUCER_WINDOW_EVENT_DECORATED; return true; }

  if (name == "maximize") { out = SAUCER_WINDOW_EVENT_MAXIMIZE; return true; }

  if (name == "minimize") { out = SAUCER_WINDOW_EVENT_MINIMIZE; return true; }

  if (name == "closed") { out = SAUCER_WINDOW_EVENT_CLOSED; return true; }

  if (name == "resize") { out = SAUCER_WINDOW_EVENT_RESIZE; return true; }

  if (name == "focus") { out = SAUCER_WINDOW_EVENT_FOCUS; return true; }

  if (name == "close") { out = SAUCER_WINDOW_EVENT_CLOSE; return true; }

  return false;

}



static bool MapWebEventName(const std::string& name, SAUCER_WEB_EVENT& out) {

  if (name == "dom-ready") { out = SAUCER_WEB_EVENT_DOM_READY; return true; }

  if (name == "navigated") { out = SAUCER_WEB_EVENT_NAVIGATED; return true; }

  if (name == "navigate") { out = SAUCER_WEB_EVENT_NAVIGATE; return true; }

  if (name == "favicon") { out = SAUCER_WEB_EVENT_FAVICON; return true; }

  if (name == "title") { out = SAUCER_WEB_EVENT_TITLE; return true; }

  if (name == "load") { out = SAUCER_WEB_EVENT_LOAD; return true; }

  return false;

}



Napi::Object Webview::Init(Napi::Env env, Napi::Object exports) {

  Napi::Function func = DefineClass(env, "Webview", {

    // Window properties

    InstanceAccessor("focused", &Webview::GetFocused, nullptr),

    InstanceAccessor("visible", &Webview::GetVisible, &Webview::SetVisible),

    InstanceAccessor("minimized", &Webview::GetMinimized, &Webview::SetMinimized),

    InstanceAccessor("maximized", &Webview::GetMaximized, &Webview::SetMaximized),

    InstanceAccessor("resizable", &Webview::GetResizable, &Webview::SetResizable),

    InstanceAccessor("decorations", &Webview::GetDecorations, &Webview::SetDecorations),

    InstanceAccessor("alwaysOnTop", &Webview::GetAlwaysOnTop, &Webview::SetAlwaysOnTop),

    InstanceAccessor("clickThrough", &Webview::GetClickThrough, &Webview::SetClickThrough),

    InstanceAccessor("title", &Webview::GetTitle, &Webview::SetTitle),

    InstanceAccessor("size", &Webview::GetSize, &Webview::SetSize),

    InstanceAccessor("maxSize", &Webview::GetMaxSize, &Webview::SetMaxSize),

    InstanceAccessor("minSize", &Webview::GetMinSize, &Webview::SetMinSize),

    // Extension properties (not in saucer C bindings)
    InstanceAccessor("position", &Webview::GetPosition, &Webview::SetPosition),
    InstanceAccessor("fullscreen", &Webview::GetFullscreen, &Webview::SetFullscreen),
    InstanceAccessor("zoom", &Webview::GetZoom, &Webview::SetZoom),

    InstanceAccessor("parent", &Webview::GetParent, nullptr),



    // Window methods

    InstanceMethod("show", &Webview::Show),

    InstanceMethod("hide", &Webview::Hide),

    InstanceMethod("close", &Webview::Close),

    InstanceMethod("focus", &Webview::Focus),

    InstanceMethod("startDrag", &Webview::StartDrag),

    InstanceMethod("startResize", &Webview::StartResize),

    InstanceMethod("setIcon", &Webview::SetIcon),



    // Webview properties

    InstanceAccessor("url", &Webview::GetUrl, &Webview::SetUrl),

    InstanceAccessor("devTools", &Webview::GetDevTools, &Webview::SetDevTools),

    InstanceAccessor("backgroundColor", &Webview::GetBackgroundColor, &Webview::SetBackgroundColor),

    InstanceAccessor("forceDarkMode", &Webview::GetForceDarkMode, &Webview::SetForceDarkMode),

    InstanceAccessor("contextMenu", &Webview::GetContextMenu, &Webview::SetContextMenu),

    InstanceAccessor("favicon", &Webview::GetFavicon, nullptr),

    InstanceAccessor("pageTitle", &Webview::GetPageTitle, nullptr),



    // Webview methods

    InstanceMethod("navigate", &Webview::Navigate),

    InstanceMethod("setFile", &Webview::SetFile),

    InstanceMethod("loadHtml", &Webview::LoadHtml),

    InstanceMethod("execute", &Webview::Execute),
    InstanceMethod("evaluate", &Webview::Evaluate),
    InstanceMethod("expose", &Webview::Expose),
    InstanceMethod("clearExposed", &Webview::ClearExposed),

    InstanceMethod("reload", &Webview::Reload),

    InstanceMethod("back", &Webview::Back),

    InstanceMethod("forward", &Webview::Forward),

    InstanceMethod("inject", &Webview::Inject),

    InstanceMethod("embed", &Webview::Embed),

    InstanceMethod("serve", &Webview::Serve),

    InstanceMethod("clearEmbedded", &Webview::ClearEmbedded),

    InstanceMethod("clearScripts", &Webview::ClearScripts),

    InstanceMethod("handleScheme", &Webview::HandleScheme),

    InstanceMethod("removeScheme", &Webview::RemoveScheme),

    StaticMethod("registerScheme", &Webview::RegisterScheme),



    // Event handling

    InstanceMethod("on", &Webview::On),

    InstanceMethod("once", &Webview::Once),

    InstanceMethod("off", &Webview::Off),

    InstanceMethod("onMessage", &Webview::OnMessage),

  });



  Napi::FunctionReference* constructor = new Napi::FunctionReference();

  *constructor = Napi::Persistent(func);

  exports.Set("Webview", func);

  return exports;

}



Webview::Webview(const Napi::CallbackInfo& info)

  : Napi::ObjectWrap<Webview>(info) {

  Napi::Env env = info.Env();



  // Get application reference

  if (info.Length() == 0 || !info[0].IsObject()) {

    Napi::TypeError::New(env, "Application instance required").ThrowAsJavaScriptException();

    return;

  }



  Application* app_obj = Napi::ObjectWrap<Application>::Unwrap(info[0].As<Napi::Object>());

  app_ = app_obj->GetApp();

  parent_ref_ = Napi::Persistent(info[0].As<Napi::Object>());
  parent_ref_.SuppressDestruct();



  // Create preferences

  saucer_preferences* prefs = saucer_preferences_new(app_);



  if (info.Length() > 1 && info[1].IsObject()) {

    Napi::Object opts = info[1].As<Napi::Object>();



    if (opts.Has("persistentCookies") && opts.Get("persistentCookies").IsBoolean()) {

      saucer_preferences_set_persistent_cookies(prefs,

        opts.Get("persistentCookies").As<Napi::Boolean>().Value());

    }



    if (opts.Has("hardwareAcceleration") && opts.Get("hardwareAcceleration").IsBoolean()) {

      saucer_preferences_set_hardware_acceleration(prefs,

        opts.Get("hardwareAcceleration").As<Napi::Boolean>().Value());

    }

    if (opts.Has("storagePath") && opts.Get("storagePath").IsString()) {

      std::string path = opts.Get("storagePath").As<Napi::String>().Utf8Value();

      saucer_preferences_set_storage_path(prefs, path.c_str());

    }

    if (opts.Has("userAgent") && opts.Get("userAgent").IsString()) {

      std::string ua = opts.Get("userAgent").As<Napi::String>().Utf8Value();

      saucer_preferences_set_user_agent(prefs, ua.c_str());

    }

    if (opts.Has("browserFlags") && opts.Get("browserFlags").IsArray()) {

      Napi::Array flags = opts.Get("browserFlags").As<Napi::Array>();

      for (uint32_t i = 0; i < flags.Length(); i++) {

        if (flags.Get(i).IsString()) {

          std::string flag = flags.Get(i).As<Napi::String>().Utf8Value();

          saucer_preferences_add_browser_flag(prefs, flag.c_str());

        }

      }

    }

    // Store preload script for injection after webview creation
    if (opts.Has("preload") && opts.Get("preload").IsString()) {
      preload_script_ = opts.Get("preload").As<Napi::String>().Utf8Value();
    }

  }



  // Create webview

  webview_ = saucer_new(prefs);

  saucer_preferences_free(prefs);

  // Keep the window hidden until explicitly shown from JS
  if (webview_) {
    saucer_window_hide(webview_);
  }



  if (!webview_) {

    Napi::Error::New(env, "Failed to create webview").ThrowAsJavaScriptException();

    return;

  }

  // Inject preload script if provided (at creation time - before page loads)
  if (!preload_script_.empty()) {
    saucer_script* script = saucer_script_new(preload_script_.c_str(), SAUCER_LOAD_TIME_CREATION);
    saucer_script_set_permanent(script, true);  // Persist across navigations
    saucer_script_set_frame(script, SAUCER_WEB_FRAME_TOP);
    saucer_webview_inject(webview_, script);
    saucer_script_free(script);
  }



  {

    std::scoped_lock lock(instance_mutex_);

    instances_[webview_] = this;

  }

}



Webview::~Webview() {

  {

    std::scoped_lock lock(instance_mutex_);

    instances_.erase(webview_);

  }

  {

    std::scoped_lock lock(exposed_mutex_);

    for (auto& entry : exposed_callbacks_) {

      if (entry && entry->tsfn) {

        entry->tsfn->Release();

      }

    }

    exposed_callbacks_.clear();

  }

  {
    std::scoped_lock lock(scheme_mutex_);
    for (auto& entry : scheme_handlers_) {
      if (entry && entry->tsfn) {
        entry->tsfn->Release();
      }
    }
    scheme_handlers_.clear();
  }



  if (message_tsfn_) {

    message_tsfn_.Release();

  }



  if (!message_handler_ref_.IsEmpty()) {

    message_handler_ref_.Reset();

  }

  if (!parent_ref_.IsEmpty()) {

    parent_ref_.Reset();

  }



  if (webview_) {

    saucer_free(webview_);

    webview_ = nullptr;

  }

}



// Window methods

Napi::Value Webview::GetFocused(const Napi::CallbackInfo& info) {

  return Napi::Boolean::New(info.Env(), saucer_window_focused(webview_));

}

Napi::Value Webview::GetVisible(const Napi::CallbackInfo& info) {

  return Napi::Boolean::New(info.Env(), saucer_window_visible(webview_));

}



void Webview::SetVisible(const Napi::CallbackInfo& info, const Napi::Value& value) {

  if (value.As<Napi::Boolean>().Value()) {

    saucer_window_show(webview_);

  } else {

    saucer_window_hide(webview_);

  }

}



Napi::Value Webview::GetMinimized(const Napi::CallbackInfo& info) {

  if (!minimized_hint_valid_) {
    minimized_hint_ = saucer_window_minimized(webview_);
    minimized_hint_valid_ = true;
  }

  return Napi::Boolean::New(info.Env(), minimized_hint_);

}



void Webview::SetMinimized(const Napi::CallbackInfo& info, const Napi::Value& value) {
  bool target = value.As<Napi::Boolean>().Value();

  // macOS refuses to miniaturize an invisible window; ensure it is shown first
  if (target && !saucer_window_visible(webview_)) {
    saucer_window_show(webview_);
  }

  saucer_window_set_minimized(webview_, target);
  minimized_hint_ = target;
  minimized_hint_valid_ = true;

}



Napi::Value Webview::GetMaximized(const Napi::CallbackInfo& info) {

  return Napi::Boolean::New(info.Env(), saucer_window_maximized(webview_));

}



void Webview::SetMaximized(const Napi::CallbackInfo& info, const Napi::Value& value) {

  saucer_window_set_maximized(webview_, value.As<Napi::Boolean>().Value());

}



Napi::Value Webview::GetResizable(const Napi::CallbackInfo& info) {

  return Napi::Boolean::New(info.Env(), saucer_window_resizable(webview_));

}



void Webview::SetResizable(const Napi::CallbackInfo& info, const Napi::Value& value) {

  saucer_window_set_resizable(webview_, value.As<Napi::Boolean>().Value());

}



Napi::Value Webview::GetDecorations(const Napi::CallbackInfo& info) {

  return Napi::Boolean::New(info.Env(), saucer_window_decorations(webview_));

}



void Webview::SetDecorations(const Napi::CallbackInfo& info, const Napi::Value& value) {

  saucer_window_set_decorations(webview_, value.As<Napi::Boolean>().Value());

}



Napi::Value Webview::GetAlwaysOnTop(const Napi::CallbackInfo& info) {

  return Napi::Boolean::New(info.Env(), saucer_window_always_on_top(webview_));

}



void Webview::SetAlwaysOnTop(const Napi::CallbackInfo& info, const Napi::Value& value) {

  saucer_window_set_always_on_top(webview_, value.As<Napi::Boolean>().Value());

}



Napi::Value Webview::GetClickThrough(const Napi::CallbackInfo& info) {

  return Napi::Boolean::New(info.Env(), saucer_window_click_through(webview_));

}



void Webview::SetClickThrough(const Napi::CallbackInfo& info, const Napi::Value& value) {

  saucer_window_set_click_through(webview_, value.As<Napi::Boolean>().Value());

}



Napi::Value Webview::GetTitle(const Napi::CallbackInfo& info) {

  return CStrToJS(info.Env(), saucer_window_title(webview_));

}



void Webview::SetTitle(const Napi::CallbackInfo& info, const Napi::Value& value) {

  std::string title = value.As<Napi::String>().Utf8Value();

  saucer_window_set_title(webview_, title.c_str());

}



Napi::Value Webview::GetSize(const Napi::CallbackInfo& info) {

  int width, height;

  saucer_window_size(webview_, &width, &height);



  Napi::Object result = Napi::Object::New(info.Env());

  result.Set("width", Napi::Number::New(info.Env(), width));

  result.Set("height", Napi::Number::New(info.Env(), height));

  return result;

}



void Webview::SetSize(const Napi::CallbackInfo& info, const Napi::Value& value) {

  Napi::Object size = value.As<Napi::Object>();

  int width = size.Get("width").As<Napi::Number>().Int32Value();

  int height = size.Get("height").As<Napi::Number>().Int32Value();

  saucer_window_set_size(webview_, width, height);

}

Napi::Value Webview::GetMaxSize(const Napi::CallbackInfo& info) {

  int width, height;

  saucer_window_max_size(webview_, &width, &height);



  Napi::Object result = Napi::Object::New(info.Env());

  result.Set("width", Napi::Number::New(info.Env(), width));

  result.Set("height", Napi::Number::New(info.Env(), height));

  return result;

}



void Webview::SetMaxSize(const Napi::CallbackInfo& info, const Napi::Value& value) {

  Napi::Object size = value.As<Napi::Object>();

  int width = size.Get("width").As<Napi::Number>().Int32Value();

  int height = size.Get("height").As<Napi::Number>().Int32Value();

  saucer_window_set_max_size(webview_, width, height);

}



Napi::Value Webview::GetMinSize(const Napi::CallbackInfo& info) {

  int width, height;

  saucer_window_min_size(webview_, &width, &height);



  Napi::Object result = Napi::Object::New(info.Env());

  result.Set("width", Napi::Number::New(info.Env(), width));

  result.Set("height", Napi::Number::New(info.Env(), height));

  return result;

}



void Webview::SetMinSize(const Napi::CallbackInfo& info, const Napi::Value& value) {

  Napi::Object size = value.As<Napi::Object>();

  int width = size.Get("width").As<Napi::Number>().Int32Value();

  int height = size.Get("height").As<Napi::Number>().Int32Value();

  saucer_window_set_min_size(webview_, width, height);

}

// ============================================================================
// Extension Properties (not in saucer C bindings)
// ============================================================================

Napi::Value Webview::GetPosition(const Napi::CallbackInfo& info) {
  int x, y;
  ::saucer_window_position_ext(webview_, &x, &y);
  
  Napi::Object result = Napi::Object::New(info.Env());
  result.Set("x", Napi::Number::New(info.Env(), x));
  result.Set("y", Napi::Number::New(info.Env(), y));
  return result;
}

void Webview::SetPosition(const Napi::CallbackInfo& info, const Napi::Value& value) {
  if (!value.IsObject()) {
    Napi::TypeError::New(info.Env(), "Position must be an object with x and y").ThrowAsJavaScriptException();
    return;
  }
  
  Napi::Object pos = value.As<Napi::Object>();
  int x = pos.Get("x").As<Napi::Number>().Int32Value();
  int y = pos.Get("y").As<Napi::Number>().Int32Value();
  
  ::saucer_window_set_position_ext(webview_, x, y);
}

Napi::Value Webview::GetFullscreen(const Napi::CallbackInfo& info) {
  bool isFullscreen = ::saucer_window_fullscreen_ext(webview_);
  return Napi::Boolean::New(info.Env(), isFullscreen);
}

void Webview::SetFullscreen(const Napi::CallbackInfo& info, const Napi::Value& value) {
  bool enabled = value.As<Napi::Boolean>().Value();
  ::saucer_window_set_fullscreen_ext(webview_, enabled);
}

Napi::Value Webview::GetZoom(const Napi::CallbackInfo& info) {
  double zoom = ::saucer_webview_zoom_ext(webview_);
  return Napi::Number::New(info.Env(), zoom);
}

void Webview::SetZoom(const Napi::CallbackInfo& info, const Napi::Value& value) {
  double level = value.As<Napi::Number>().DoubleValue();
  ::saucer_webview_set_zoom_ext(webview_, level);
}



Napi::Value Webview::GetParent(const Napi::CallbackInfo& info) {

  if (parent_ref_.IsEmpty()) {

    return info.Env().Null();

  }



  return parent_ref_.Value();

}



void Webview::Show(const Napi::CallbackInfo& info) {

  saucer_window_show(webview_);

}



void Webview::Hide(const Napi::CallbackInfo& info) {

  saucer_window_hide(webview_);

}



void Webview::Close(const Napi::CallbackInfo& info) {

  saucer_window_close(webview_);

}

void Webview::Focus(const Napi::CallbackInfo& info) {
  saucer_window_focus(webview_);
}

void Webview::StartDrag(const Napi::CallbackInfo& info) {
  saucer_window_start_drag(webview_);
}

void Webview::StartResize(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // Default to bottom-right corner if no edge specified
  SAUCER_WINDOW_EDGE edge = static_cast<SAUCER_WINDOW_EDGE>(
    SAUCER_WINDOW_EDGE_BOTTOM | SAUCER_WINDOW_EDGE_RIGHT
  );

  if (info.Length() > 0) {
    if (info[0].IsNumber()) {
      // Accept numeric edge flags directly
      edge = static_cast<SAUCER_WINDOW_EDGE>(info[0].As<Napi::Number>().Int32Value());
    } else if (info[0].IsString()) {
      // Accept string edge names
      std::string edgeStr = info[0].As<Napi::String>().Utf8Value();
      edge = static_cast<SAUCER_WINDOW_EDGE>(0);

      if (edgeStr.find("top") != std::string::npos) {
        edge = static_cast<SAUCER_WINDOW_EDGE>(edge | SAUCER_WINDOW_EDGE_TOP);
      }
      if (edgeStr.find("bottom") != std::string::npos) {
        edge = static_cast<SAUCER_WINDOW_EDGE>(edge | SAUCER_WINDOW_EDGE_BOTTOM);
      }
      if (edgeStr.find("left") != std::string::npos) {
        edge = static_cast<SAUCER_WINDOW_EDGE>(edge | SAUCER_WINDOW_EDGE_LEFT);
      }
      if (edgeStr.find("right") != std::string::npos) {
        edge = static_cast<SAUCER_WINDOW_EDGE>(edge | SAUCER_WINDOW_EDGE_RIGHT);
      }

      // If no valid edge found, default to bottom-right
      if (edge == 0) {
        edge = static_cast<SAUCER_WINDOW_EDGE>(
          SAUCER_WINDOW_EDGE_BOTTOM | SAUCER_WINDOW_EDGE_RIGHT
        );
      }
    } else if (info[0].IsObject()) {
      // Accept object with edge properties
      Napi::Object opts = info[0].As<Napi::Object>();
      edge = static_cast<SAUCER_WINDOW_EDGE>(0);

      if (opts.Has("top") && opts.Get("top").ToBoolean().Value()) {
        edge = static_cast<SAUCER_WINDOW_EDGE>(edge | SAUCER_WINDOW_EDGE_TOP);
      }
      if (opts.Has("bottom") && opts.Get("bottom").ToBoolean().Value()) {
        edge = static_cast<SAUCER_WINDOW_EDGE>(edge | SAUCER_WINDOW_EDGE_BOTTOM);
      }
      if (opts.Has("left") && opts.Get("left").ToBoolean().Value()) {
        edge = static_cast<SAUCER_WINDOW_EDGE>(edge | SAUCER_WINDOW_EDGE_LEFT);
      }
      if (opts.Has("right") && opts.Get("right").ToBoolean().Value()) {
        edge = static_cast<SAUCER_WINDOW_EDGE>(edge | SAUCER_WINDOW_EDGE_RIGHT);
      }

      // If no valid edge found, default to bottom-right
      if (edge == 0) {
        edge = static_cast<SAUCER_WINDOW_EDGE>(
          SAUCER_WINDOW_EDGE_BOTTOM | SAUCER_WINDOW_EDGE_RIGHT
        );
      }
    }
  }

  saucer_window_start_resize(webview_, edge);
}

void Webview::SetIcon(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() == 0) {
    Napi::TypeError::New(env, "Usage: setIcon(pathOrBuffer)").ThrowAsJavaScriptException();
    return;
  }

  saucer_icon* icon = nullptr;

  if (info[0].IsString()) {
    // Load icon from file path
    std::string path = info[0].As<Napi::String>().Utf8Value();
    saucer_icon_from_file(&icon, path.c_str());
  } else if (info[0].IsBuffer()) {
    // Load icon from Buffer data
    Napi::Buffer<uint8_t> buf = info[0].As<Napi::Buffer<uint8_t>>();
    saucer_stash* stash = saucer_stash_from(buf.Data(), buf.Length());
    saucer_icon_from_data(&icon, stash);
    saucer_stash_free(stash);
  } else {
    Napi::TypeError::New(env, "setIcon requires a file path string or Buffer").ThrowAsJavaScriptException();
    return;
  }

  if (icon) {
    saucer_window_set_icon(webview_, icon);
    saucer_icon_free(icon);
  }
}



// Webview methods

Napi::Value Webview::GetUrl(const Napi::CallbackInfo& info) {

  return CStrToJS(info.Env(), saucer_webview_url(webview_));

}



void Webview::SetUrl(const Napi::CallbackInfo& info, const Napi::Value& value) {

  std::string url = value.As<Napi::String>().Utf8Value();

  saucer_webview_set_url(webview_, url.c_str());

}



Napi::Value Webview::GetDevTools(const Napi::CallbackInfo& info) {

  return Napi::Boolean::New(info.Env(), saucer_webview_dev_tools(webview_));

}



void Webview::SetDevTools(const Napi::CallbackInfo& info, const Napi::Value& value) {

  saucer_webview_set_dev_tools(webview_, value.As<Napi::Boolean>().Value());

}



void Webview::Navigate(const Napi::CallbackInfo& info) {

  if (info.Length() == 0 || !info[0].IsString()) {

    Napi::TypeError::New(info.Env(), "URL string required").ThrowAsJavaScriptException();

    return;

  }



  std::string url = info[0].As<Napi::String>().Utf8Value();

  saucer_webview_set_url(webview_, url.c_str());

}

void Webview::SetFile(const Napi::CallbackInfo& info) {

  if (info.Length() == 0 || !info[0].IsString()) {

    Napi::TypeError::New(info.Env(), "File path string required").ThrowAsJavaScriptException();

    return;

  }

  std::string file = info[0].As<Napi::String>().Utf8Value();

  saucer_webview_set_file(webview_, file.c_str());

}

// Helper function to URL-encode a string for data URI
static std::string UrlEncode(const std::string& str) {
  std::string encoded;
  encoded.reserve(str.size() * 3);  // Worst case: all characters need encoding
  
  for (unsigned char c : str) {
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' ||
        c == '!' || c == '\'' || c == '(' || c == ')' || c == '*') {
      encoded += c;
    } else if (c == ' ') {
      encoded += "%20";
    } else {
      static const char hex[] = "0123456789ABCDEF";
      encoded += '%';
      encoded += hex[(c >> 4) & 0xF];
      encoded += hex[c & 0xF];
    }
  }
  return encoded;
}

void Webview::LoadHtml(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() == 0 || !info[0].IsString()) {
    Napi::TypeError::New(env, "HTML string required").ThrowAsJavaScriptException();
    return;
  }

  std::string html = info[0].As<Napi::String>().Utf8Value();
  
  // Use data URI with proper URL encoding
  std::string dataUri = "data:text/html;charset=utf-8," + UrlEncode(html);
  saucer_webview_set_url(webview_, dataUri.c_str());
}



void Webview::Execute(const Napi::CallbackInfo& info) {

  Napi::Env env = info.Env();



  if (info.Length() == 0 || !info[0].IsString()) {

    Napi::TypeError::New(env, "JavaScript code string required").ThrowAsJavaScriptException();

    return;

  }



  const bool hasArgs = info.Length() > 1;

  std::string code = info[0].As<Napi::String>().Utf8Value();

  if (!hasArgs) {

    saucer_webview_execute(webview_, code.c_str());

    return;

  }



  std::vector<glz::json_t> args;

  try {

    args = CollectJsonArgs(info, 1);

  } catch (const Napi::Error& err) {

    err.ThrowAsJavaScriptException();

    return;

  } catch (...) {

    Napi::Error::New(env, "Failed to serialize execution arguments").ThrowAsJavaScriptException();

    return;

  }



  auto* handle = static_cast<saucer_handle*>(webview_);

  if (!handle) {

    saucer_webview_execute(webview_, code.c_str());

    return;

  }



  switch (args.size()) {

    case 1: handle->execute(code, args[0]); break;

    case 2: handle->execute(code, args[0], args[1]); break;

    case 3: handle->execute(code, args[0], args[1], args[2]); break;

    case 4: handle->execute(code, args[0], args[1], args[2], args[3]); break;

    case 5: handle->execute(code, args[0], args[1], args[2], args[3], args[4]); break;

    case 6: handle->execute(code, args[0], args[1], args[2], args[3], args[4], args[5]); break;

    case 7: handle->execute(code, args[0], args[1], args[2], args[3], args[4], args[5], args[6]); break;

    case 8: handle->execute(code, args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]); break;

    default:

      Napi::RangeError::New(env, "Too many arguments for execute").ThrowAsJavaScriptException();

      break;

  }

}



void Webview::Reload(const Napi::CallbackInfo& info) {

  saucer_webview_reload(webview_);

}



void Webview::Back(const Napi::CallbackInfo& info) {

  saucer_webview_back(webview_);

}



void Webview::Forward(const Napi::CallbackInfo& info) {

  saucer_webview_forward(webview_);

}



// Background color property

Napi::Value Webview::GetBackgroundColor(const Napi::CallbackInfo& info) {
  uint8_t r, g, b, a;
  saucer_webview_background(webview_, &r, &g, &b, &a);

  Napi::Array result = Napi::Array::New(info.Env(), 4);
  result.Set(0u, Napi::Number::New(info.Env(), r));
  result.Set(1u, Napi::Number::New(info.Env(), g));
  result.Set(2u, Napi::Number::New(info.Env(), b));
  result.Set(3u, Napi::Number::New(info.Env(), a));
  return result;
}

void Webview::SetBackgroundColor(const Napi::CallbackInfo& info, const Napi::Value& value) {
  Napi::Env env = info.Env();

  if (!value.IsArray()) {
    Napi::TypeError::New(env, "backgroundColor must be an array [r, g, b, a]").ThrowAsJavaScriptException();
    return;
  }

  Napi::Array arr = value.As<Napi::Array>();
  if (arr.Length() < 4) {
    Napi::TypeError::New(env, "backgroundColor must have 4 elements [r, g, b, a]").ThrowAsJavaScriptException();
    return;
  }

  uint8_t r = static_cast<uint8_t>(arr.Get(0u).As<Napi::Number>().Uint32Value());
  uint8_t g = static_cast<uint8_t>(arr.Get(1u).As<Napi::Number>().Uint32Value());
  uint8_t b = static_cast<uint8_t>(arr.Get(2u).As<Napi::Number>().Uint32Value());
  uint8_t a = static_cast<uint8_t>(arr.Get(3u).As<Napi::Number>().Uint32Value());

  saucer_webview_set_background(webview_, r, g, b, a);
}

// Force dark mode property

Napi::Value Webview::GetForceDarkMode(const Napi::CallbackInfo& info) {
  return Napi::Boolean::New(info.Env(), saucer_webview_force_dark_mode(webview_));
}

void Webview::SetForceDarkMode(const Napi::CallbackInfo& info, const Napi::Value& value) {
  saucer_webview_set_force_dark_mode(webview_, value.As<Napi::Boolean>().Value());
}

// Context menu property

Napi::Value Webview::GetContextMenu(const Napi::CallbackInfo& info) {
  return Napi::Boolean::New(info.Env(), saucer_webview_context_menu(webview_));
}

void Webview::SetContextMenu(const Napi::CallbackInfo& info, const Napi::Value& value) {
  saucer_webview_set_context_menu(webview_, value.As<Napi::Boolean>().Value());
}

// Favicon getter - returns the current page favicon as a Buffer (or null if empty)
Napi::Value Webview::GetFavicon(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  saucer_icon* icon = saucer_webview_favicon(webview_);
  if (!icon || saucer_icon_empty(icon)) {
    if (icon) {
      saucer_icon_free(icon);
    }
    return env.Null();
  }

  saucer_stash* stash = saucer_icon_data(icon);
  if (!stash) {
    saucer_icon_free(icon);
    return env.Null();
  }

  const uint8_t* data = saucer_stash_data(stash);
  size_t size = saucer_stash_size(stash);

  Napi::Buffer<uint8_t> buffer = Napi::Buffer<uint8_t>::Copy(env, data, size);

  saucer_stash_free(stash);
  saucer_icon_free(icon);

  return buffer;
}

// Page title getter - returns the current page title
Napi::Value Webview::GetPageTitle(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  char* title = saucer_webview_page_title(webview_);
  if (!title) {
    return env.Null();
  }

  Napi::String result = Napi::String::New(env, title);
  saucer_memory_free(title);

  return result;
}

// Script injection

void Webview::Inject(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() == 0 || !info[0].IsObject()) {
    Napi::TypeError::New(env, "Usage: inject({ code, time?, frame?, permanent? })").ThrowAsJavaScriptException();
    return;
  }

  Napi::Object opts = info[0].As<Napi::Object>();

  if (!opts.Has("code") || !opts.Get("code").IsString()) {
    Napi::TypeError::New(env, "inject requires a 'code' string property").ThrowAsJavaScriptException();
    return;
  }

  std::string code = opts.Get("code").As<Napi::String>().Utf8Value();

  // Default to SAUCER_LOAD_TIME_READY
  SAUCER_LOAD_TIME time = SAUCER_LOAD_TIME_READY;
  if (opts.Has("time") && opts.Get("time").IsString()) {
    std::string timeStr = opts.Get("time").As<Napi::String>().Utf8Value();
    if (timeStr == "creation") {
      time = SAUCER_LOAD_TIME_CREATION;
    }
  }

  saucer_script* script = saucer_script_new(code.c_str(), time);

  // Set frame if provided
  if (opts.Has("frame") && opts.Get("frame").IsString()) {
    std::string frameStr = opts.Get("frame").As<Napi::String>().Utf8Value();
    if (frameStr == "all") {
      saucer_script_set_frame(script, SAUCER_WEB_FRAME_ALL);
    } else {
      saucer_script_set_frame(script, SAUCER_WEB_FRAME_TOP);
    }
  }

  // Set permanent if provided
  if (opts.Has("permanent") && opts.Get("permanent").IsBoolean()) {
    saucer_script_set_permanent(script, opts.Get("permanent").As<Napi::Boolean>().Value());
  }

  saucer_webview_inject(webview_, script);
  saucer_script_free(script);
}

// File embedding

void Webview::Embed(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() == 0 || !info[0].IsObject()) {
    Napi::TypeError::New(env, "Usage: embed({ name: { content, mime } }, policy?)").ThrowAsJavaScriptException();
    return;
  }

  Napi::Object files = info[0].As<Napi::Object>();

  // Parse launch policy from second argument
  SAUCER_LAUNCH policy = SAUCER_LAUNCH_SYNC;
  if (info.Length() > 1 && info[1].IsString()) {
    std::string policyStr = info[1].As<Napi::String>().Utf8Value();
    if (policyStr == "async") {
      policy = SAUCER_LAUNCH_ASYNC;
    }
  }

  Napi::Array names = files.GetPropertyNames();
  for (uint32_t i = 0; i < names.Length(); ++i) {
    Napi::Value nameVal = names.Get(i);
    if (!nameVal.IsString()) continue;

    std::string name = nameVal.As<Napi::String>().Utf8Value();
    Napi::Value fileVal = files.Get(name);

    if (!fileVal.IsObject()) continue;
    Napi::Object file = fileVal.As<Napi::Object>();

    // Get content - can be string or Buffer
    saucer_stash* stash = nullptr;
    if (file.Has("content")) {
      Napi::Value contentVal = file.Get("content");
      if (contentVal.IsString()) {
        std::string content = contentVal.As<Napi::String>().Utf8Value();
        stash = saucer_stash_from(reinterpret_cast<const uint8_t*>(content.data()), content.size());
      } else if (contentVal.IsBuffer()) {
        Napi::Buffer<uint8_t> buf = contentVal.As<Napi::Buffer<uint8_t>>();
        stash = saucer_stash_from(buf.Data(), buf.Length());
      }
    }

    if (!stash) continue;

    // Get mime type
    std::string mime = "application/octet-stream";
    if (file.Has("mime") && file.Get("mime").IsString()) {
      mime = file.Get("mime").As<Napi::String>().Utf8Value();
    }

    saucer_embedded_file* embedded = saucer_embed(stash, mime.c_str());
    saucer_webview_embed_file(webview_, name.c_str(), embedded, policy);
    saucer_embed_free(embedded);
    saucer_stash_free(stash);
  }
}

void Webview::Serve(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() == 0 || !info[0].IsString()) {
    Napi::TypeError::New(env, "Usage: serve(filename)").ThrowAsJavaScriptException();
    return;
  }

  std::string file = info[0].As<Napi::String>().Utf8Value();
  saucer_webview_serve(webview_, file.c_str());
}

void Webview::ClearEmbedded(const Napi::CallbackInfo& info) {
  if (info.Length() > 0 && info[0].IsString()) {
    std::string file = info[0].As<Napi::String>().Utf8Value();
    saucer_webview_clear_embedded_file(webview_, file.c_str());
  } else {
    saucer_webview_clear_embedded(webview_);
  }
}

void Webview::ClearScripts(const Napi::CallbackInfo& info) {
  saucer_webview_clear_scripts(webview_);
}

// Custom URL scheme handling

void Webview::HandleScheme(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 2 || !info[0].IsString() || !info[1].IsFunction()) {
    Napi::TypeError::New(env, "Usage: handleScheme(name, handler, policy?)").ThrowAsJavaScriptException();
    return;
  }

  std::string name = info[0].As<Napi::String>().Utf8Value();
  Napi::Function handler = info[1].As<Napi::Function>();

  SAUCER_LAUNCH policy = SAUCER_LAUNCH_SYNC;
  if (info.Length() > 2 && info[2].IsString()) {
    std::string policyStr = info[2].As<Napi::String>().Utf8Value();
    if (policyStr == "async") {
      policy = SAUCER_LAUNCH_ASYNC;
    }
  }

  auto scheme_entry = std::make_shared<SchemeHandler>();
  scheme_entry->name = name;
  scheme_entry->tsfn = std::make_shared<Napi::ThreadSafeFunction>(
    Napi::ThreadSafeFunction::New(
      env,
      handler,
      "saucer.webview.handleScheme",
      0,
      1
    )
  );

  {
    std::scoped_lock lock(scheme_mutex_);
    scheme_handlers_.push_back(scheme_entry);
  }

  saucer_webview_handle_scheme(webview_, name.c_str(),
    [](saucer_handle* handle, saucer_scheme_request* request, saucer_scheme_executor* executor) {
      Webview* self = FromHandle(handle);
      if (!self) {
        saucer_scheme_executor_reject(executor, SAUCER_REQUEST_ERROR_FAILED);
        saucer_scheme_request_free(request);
        saucer_scheme_executor_free(executor);
        return;
      }

      // Find the handler for this request's URL scheme
      char* url = saucer_scheme_request_url(request);
      std::string urlStr = url ? url : "";
      saucer_memory_free(url);

      std::shared_ptr<SchemeHandler> handler_entry;
      {
        std::scoped_lock lock(self->scheme_mutex_);
        for (auto& h : self->scheme_handlers_) {
          if (urlStr.find(h->name + "://") == 0 || urlStr.find(h->name + ":") == 0) {
            handler_entry = h;
            break;
          }
        }
      }

      if (!handler_entry || !handler_entry->tsfn) {
        saucer_scheme_executor_reject(executor, SAUCER_REQUEST_ERROR_NOT_FOUND);
        saucer_scheme_request_free(request);
        saucer_scheme_executor_free(executor);
        return;
      }

      // Build request object for JS
      char* method = saucer_scheme_request_method(request);
      std::string methodStr = method ? method : "GET";
      saucer_memory_free(method);

      saucer_stash* contentStash = saucer_scheme_request_content(request);
      std::vector<uint8_t> contentData;
      if (contentStash) {
        const uint8_t* data = saucer_stash_data(contentStash);
        size_t size = saucer_stash_size(contentStash);
        contentData.assign(data, data + size);
        saucer_stash_free(contentStash);
      }

      char** headerKeys = nullptr;
      char** headerVals = nullptr;
      size_t headerCount = 0;
      saucer_scheme_request_headers(request, &headerKeys, &headerVals, &headerCount);
      std::vector<std::pair<std::string, std::string>> headers;
      for (size_t i = 0; i < headerCount; ++i) {
        headers.emplace_back(headerKeys[i] ? headerKeys[i] : "", headerVals[i] ? headerVals[i] : "");
        saucer_memory_free(headerKeys[i]);
        saucer_memory_free(headerVals[i]);
      }
      saucer_memory_free(headerKeys);
      saucer_memory_free(headerVals);

      saucer_scheme_request_free(request);

      struct SchemePayload {
        std::string url;
        std::string method;
        std::vector<uint8_t> content;
        std::vector<std::pair<std::string, std::string>> headers;
        saucer_scheme_executor* executor;
      };

      auto* payload = new SchemePayload{
        std::move(urlStr),
        std::move(methodStr),
        std::move(contentData),
        std::move(headers),
        executor
      };

      handler_entry->tsfn->NonBlockingCall(payload,
        [](Napi::Env env, Napi::Function jsCallback, SchemePayload* data) {
          Napi::HandleScope scope(env);

          // Build request object
          Napi::Object reqObj = Napi::Object::New(env);
          reqObj.Set("url", Napi::String::New(env, data->url));
          reqObj.Set("method", Napi::String::New(env, data->method));

          if (!data->content.empty()) {
            reqObj.Set("content", Napi::Buffer<uint8_t>::Copy(env, data->content.data(), data->content.size()));
          } else {
            reqObj.Set("content", env.Null());
          }

          Napi::Object headersObj = Napi::Object::New(env);
          for (const auto& [key, val] : data->headers) {
            headersObj.Set(key, Napi::String::New(env, val));
          }
          reqObj.Set("headers", headersObj);

          auto resolveResponse = [env, executor = data->executor](Napi::Value result) {
            if (result.IsObject()) {
              Napi::Object resObj = result.As<Napi::Object>();

              // Get data - can be string or Buffer
              saucer_stash* stash = nullptr;
              if (resObj.Has("data")) {
                Napi::Value dataVal = resObj.Get("data");
                if (dataVal.IsString()) {
                  std::string dataStr = dataVal.As<Napi::String>().Utf8Value();
                  stash = saucer_stash_from(reinterpret_cast<const uint8_t*>(dataStr.data()), dataStr.size());
                } else if (dataVal.IsBuffer()) {
                  Napi::Buffer<uint8_t> buf = dataVal.As<Napi::Buffer<uint8_t>>();
                  stash = saucer_stash_from(buf.Data(), buf.Length());
                }
              }

              if (!stash) {
                saucer_scheme_executor_reject(executor, SAUCER_REQUEST_ERROR_FAILED);
                saucer_scheme_executor_free(executor);
                return;
              }

              std::string mime = "text/html";
              if (resObj.Has("mime") && resObj.Get("mime").IsString()) {
                mime = resObj.Get("mime").As<Napi::String>().Utf8Value();
              }

              saucer_scheme_response* response = saucer_scheme_response_new(stash, mime.c_str());
              saucer_stash_free(stash);

              if (resObj.Has("status") && resObj.Get("status").IsNumber()) {
                saucer_scheme_response_set_status(response, resObj.Get("status").As<Napi::Number>().Int32Value());
              }

              if (resObj.Has("headers") && resObj.Get("headers").IsObject()) {
                Napi::Object hdrs = resObj.Get("headers").As<Napi::Object>();
                Napi::Array keys = hdrs.GetPropertyNames();
                for (uint32_t i = 0; i < keys.Length(); ++i) {
                  std::string key = keys.Get(i).As<Napi::String>().Utf8Value();
                  if (hdrs.Get(key).IsString()) {
                    std::string val = hdrs.Get(key).As<Napi::String>().Utf8Value();
                    saucer_scheme_response_add_header(response, key.c_str(), val.c_str());
                  }
                }
              }

              saucer_scheme_executor_resolve(executor, response);
              saucer_scheme_response_free(response);
            } else {
              saucer_scheme_executor_reject(executor, SAUCER_REQUEST_ERROR_FAILED);
            }
            saucer_scheme_executor_free(executor);
          };

          auto rejectResponse = [executor = data->executor](SAUCER_SCHEME_ERROR error) {
            saucer_scheme_executor_reject(executor, error);
            saucer_scheme_executor_free(executor);
          };

          try {
            Napi::Value result = jsCallback.Call({ reqObj });

            if (result.IsPromise()) {
              auto promise = result.As<Napi::Promise>();

              auto* resolveData = new std::function<void(Napi::Value)>(resolveResponse);
              auto* rejectData = new std::function<void(SAUCER_SCHEME_ERROR)>(rejectResponse);

              auto onResolve = Napi::Function::New(env, [resolveData](const Napi::CallbackInfo& info) {
                (*resolveData)(info.Length() > 0 ? info[0] : info.Env().Undefined());
                delete resolveData;
                return info.Env().Undefined();
              });

              auto onReject = Napi::Function::New(env, [rejectData](const Napi::CallbackInfo& info) {
                (*rejectData)(SAUCER_REQUEST_ERROR_FAILED);
                delete rejectData;
                return info.Env().Undefined();
              });

              promise.Then(onResolve, onReject);
            } else {
              resolveResponse(result);
            }
          } catch (const Napi::Error& err) {
            rejectResponse(SAUCER_REQUEST_ERROR_FAILED);
          } catch (...) {
            rejectResponse(SAUCER_REQUEST_ERROR_FAILED);
          }

          delete data;
        }
      );
    },
    policy
  );
}

void Webview::RemoveScheme(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() == 0 || !info[0].IsString()) {
    Napi::TypeError::New(env, "Usage: removeScheme(name)").ThrowAsJavaScriptException();
    return;
  }

  std::string name = info[0].As<Napi::String>().Utf8Value();

  {
    std::scoped_lock lock(scheme_mutex_);
    scheme_handlers_.erase(
      std::remove_if(scheme_handlers_.begin(), scheme_handlers_.end(),
        [&name](const std::shared_ptr<SchemeHandler>& h) {
          if (h->name == name) {
            if (h->tsfn) {
              h->tsfn->Release();
            }
            return true;
          }
          return false;
        }),
      scheme_handlers_.end()
    );
  }

  saucer_webview_remove_scheme(webview_, name.c_str());
}

void Webview::RegisterScheme(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() == 0 || !info[0].IsString()) {
    Napi::TypeError::New(env, "Usage: Webview.registerScheme(name)").ThrowAsJavaScriptException();
    return;
  }

  std::string name = info[0].As<Napi::String>().Utf8Value();
  saucer_register_scheme(name.c_str());
}



// Event handling - simplified implementation

void Webview::On(const Napi::CallbackInfo& info) {

  if (info.Length() < 2 || !info[0].IsString() || !info[1].IsFunction()) {

    Napi::TypeError::New(info.Env(), "Usage: on(event, callback)").ThrowAsJavaScriptException();

    return;

  }



  std::string event = info[0].As<Napi::String>().Utf8Value();

  Napi::Function cb = info[1].As<Napi::Function>();



  if (!RegisterEvent(event, cb, false)) {

    Napi::Error::New(info.Env(), "Unsupported event: " + event).ThrowAsJavaScriptException();

  }

}



void Webview::Once(const Napi::CallbackInfo& info) {

  if (info.Length() < 2 || !info[0].IsString() || !info[1].IsFunction()) {

    Napi::TypeError::New(info.Env(), "Usage: once(event, callback)").ThrowAsJavaScriptException();

    return;

  }



  std::string event = info[0].As<Napi::String>().Utf8Value();

  Napi::Function cb = info[1].As<Napi::Function>();



  if (!RegisterEvent(event, cb, true)) {

    Napi::Error::New(info.Env(), "Unsupported event: " + event).ThrowAsJavaScriptException();

  }

}



void Webview::Off(const Napi::CallbackInfo& info) {

  if (info.Length() == 0 || !info[0].IsString()) {

    Napi::TypeError::New(info.Env(), "Usage: off(event, callback?)").ThrowAsJavaScriptException();

    return;

  }



  std::string event = info[0].As<Napi::String>().Utf8Value();



  if (info.Length() > 1 && info[1].IsFunction()) {

    RemoveCallbackByFunction(event, info[1].As<Napi::Function>());

  } else {

    RemoveAllCallbacks(event);

  }

}



void Webview::OnMessage(const Napi::CallbackInfo& info) {

  Napi::Env env = info.Env();

  if (info.Length() == 0 || !info[0].IsFunction()) {

    Napi::TypeError::New(env, "Usage: onMessage(callback)").ThrowAsJavaScriptException();

    return;

  }



  if (message_tsfn_) {

    message_tsfn_.Release();

  }



  if (!message_handler_ref_.IsEmpty()) {

    message_handler_ref_.Reset();

  }



  Napi::Function cb = info[0].As<Napi::Function>();

  message_handler_ref_ = Napi::Persistent(cb);

  message_handler_ref_.SuppressDestruct();



  message_tsfn_ = Napi::ThreadSafeFunction::New(

    env,

    cb,

    "saucer.webview.onMessage",

    0,

    1

  );



  saucer_webview_on_message(webview_, &Webview::MessageCallback);

}



void Webview::Expose(const Napi::CallbackInfo& info) {

  Napi::Env env = info.Env();



  if (info.Length() < 2 || !info[0].IsString() || !info[1].IsFunction()) {

    Napi::TypeError::New(env, "Usage: expose(name, callback, options?)").ThrowAsJavaScriptException();

    return;

  }



  auto* handle = static_cast<saucer_handle*>(webview_);

  if (!handle) {

    Napi::Error::New(env, "Native webview handle unavailable").ThrowAsJavaScriptException();

    return;

  }



  std::string name = info[0].As<Napi::String>().Utf8Value();

  Napi::Function cb = info[1].As<Napi::Function>();



  auto exposed_entry = std::make_shared<ExposedCallback>();

  exposed_entry->name = name;

  exposed_entry->tsfn = std::make_shared<Napi::ThreadSafeFunction>(

    Napi::ThreadSafeFunction::New(

      env,

      cb,

      "saucer.webview.expose",

      0,

      1

    )

  );



  {

    std::scoped_lock lock(exposed_mutex_);

    exposed_callbacks_.push_back(exposed_entry);

  }



  using RpcExecutor = saucer::executor<glz::json_t>;

  handle->expose(

    std::move(name),

    [entry = exposed_entry](std::vector<glz::json_t> params, const RpcExecutor& exec) {

      auto executor = std::make_shared<RpcExecutor>(exec);

      auto params_json = glz::write_json(params).value_or("[]");

      auto* payload = new std::tuple<std::shared_ptr<Napi::ThreadSafeFunction>, std::shared_ptr<RpcExecutor>, std::string>{

        entry->tsfn,

        executor,

        std::move(params_json)

      };



      auto status = entry->tsfn->NonBlockingCall(

        payload,

        [](Napi::Env env, Napi::Function jsCallback, std::tuple<std::shared_ptr<Napi::ThreadSafeFunction>, std::shared_ptr<RpcExecutor>, std::string>* data) {

          auto [tsfn, executor, params] = *data;

          delete data;



          auto reject_with = [&](Napi::Value reason) {
            try {
              executor->reject(Webview::StringifyForRPC(env, reason));
            } catch (const Napi::Error& err) {
              executor->reject(Webview::StringifyForRPC(env, err.Value()));
            } catch (...) {
              executor->reject("Unhandled RPC rejection");
            }
          };



          auto resolve_with = [&](Napi::Value value) {
            try {
              executor->resolve(Webview::SerializeForRPC(env, value));
            } catch (const Napi::Error& err) {
              executor->reject(Webview::StringifyForRPC(env, err.Value()));
            } catch (...) {
              executor->reject("Failed to serialize RPC result");
            }
          };



          try {

            Napi::Value parsed = Webview::ParseJson(env, params);

            std::vector<napi_value> args;

            if (parsed.IsArray()) {

              Napi::Array arr = parsed.As<Napi::Array>();

              uint32_t len = arr.Length();

              args.reserve(len);

              for (uint32_t i = 0; i < len; ++i) {

                args.push_back(arr.Get(i));

              }

            } else if (!parsed.IsUndefined() && !parsed.IsNull()) {

              args.push_back(parsed);

            }



            Napi::Value result = jsCallback.Call(args);

            if (result.IsPromise()) {

              auto promise = result.As<Napi::Promise>();



              auto on_resolve = Napi::Function::New(env, [executor](const Napi::CallbackInfo& info) {

                Napi::Env env = info.Env();

                Napi::Value val = info.Length() > 0 ? info[0] : env.Undefined();

                executor->resolve(Webview::SerializeForRPC(env, val));

                return env.Undefined();

              });



              auto on_reject = Napi::Function::New(env, [executor](const Napi::CallbackInfo& info) {

                Napi::Env env = info.Env();

                Napi::Value reason = info.Length() > 0 ? info[0] : env.Null();

                executor->reject(Webview::StringifyForRPC(env, reason));

                return env.Undefined();

              });



              promise.Then(on_resolve, on_reject);

            } else {

              resolve_with(result);

            }

          } catch (const Napi::Error& err) {

            reject_with(err.Value());

          } catch (...) {

            executor->reject("Unexpected RPC error");

          }

        }

      );



      if (status != napi_ok) {

        executor->reject("Failed to dispatch RPC to JavaScript");

      }

    }

  );

}

void Webview::ClearExposed(const Napi::CallbackInfo& info) {

  Napi::Env env = info.Env();

  auto* handle = static_cast<saucer_handle*>(webview_);

  if (!handle) {

    Napi::Error::New(env, "Native webview handle unavailable").ThrowAsJavaScriptException();

    return;

  }

  if (info.Length() > 0 && info[0].IsString()) {

    // Clear specific exposed function by name

    std::string name = info[0].As<Napi::String>().Utf8Value();

    handle->clear_exposed(name);

    // Also remove from our tracking list

    std::scoped_lock lock(exposed_mutex_);

    exposed_callbacks_.erase(

      std::remove_if(exposed_callbacks_.begin(), exposed_callbacks_.end(),

        [&name](const auto& cb) { return cb->name == name; }),

      exposed_callbacks_.end());

  } else {

    // Clear all exposed functions

    handle->clear_exposed();

    // Clear our tracking list

    std::scoped_lock lock(exposed_mutex_);

    exposed_callbacks_.clear();

  }

}



Napi::Value Webview::Evaluate(const Napi::CallbackInfo& info) {

  Napi::Env env = info.Env();



  if (info.Length() == 0 || !info[0].IsString()) {

    Napi::TypeError::New(env, "Usage: evaluate(code, ...params)").ThrowAsJavaScriptException();

    return env.Undefined();

  }



  auto* handle = static_cast<saucer_handle*>(webview_);

  if (!handle) {

    Napi::Error::New(env, "Native webview handle unavailable").ThrowAsJavaScriptException();

    return env.Undefined();

  }



  std::string code = info[0].As<Napi::String>().Utf8Value();

  std::vector<glz::json_t> args;

  try {

    args = CollectJsonArgs(info, 1);

  } catch (const Napi::Error& err) {

    err.ThrowAsJavaScriptException();

    return env.Undefined();

  } catch (...) {

    Napi::Error::New(env, "Failed to serialize evaluation arguments").ThrowAsJavaScriptException();

    return env.Undefined();

  }



  auto deferred = Napi::Promise::Deferred::New(env);



  class EvaluateWorker : public Napi::AsyncWorker {

   public:

    EvaluateWorker(Napi::Env env, std::future<glz::json_t>&& fut, Napi::Promise::Deferred deferred)

      : Napi::AsyncWorker(env)

      , future_(std::move(fut))

      , deferred_(std::move(deferred)) {}



    void Execute() override {

      try {

        result_ = future_.get();

      } catch (const std::exception& err) {

        SetError(err.what());

      } catch (...) {

        SetError("Evaluation failed");

      }

    }



    void OnOK() override {

      std::string json = glz::write_json(result_).value_or("null");

      try {

        Napi::Value parsed = Webview::ParseJson(Env(), json);

        deferred_.Resolve(parsed);

      } catch (const Napi::Error& err) {

        deferred_.Reject(err.Value());

      } catch (...) {

        deferred_.Reject(Napi::Error::New(Env(), "Failed to parse evaluation result").Value());

      }

    }



    void OnError(const Napi::Error& err) override {

      deferred_.Reject(err.Value());

    }



   private:

    std::future<glz::json_t> future_;

    glz::json_t result_;

    Napi::Promise::Deferred deferred_;

  };



  std::future<glz::json_t> future;

  switch (args.size()) {

    case 0: future = handle->evaluate<glz::json_t>(code); break;

    case 1: future = handle->evaluate<glz::json_t>(code, args[0]); break;

    case 2: future = handle->evaluate<glz::json_t>(code, args[0], args[1]); break;

    case 3: future = handle->evaluate<glz::json_t>(code, args[0], args[1], args[2]); break;

    case 4: future = handle->evaluate<glz::json_t>(code, args[0], args[1], args[2], args[3]); break;

    case 5: future = handle->evaluate<glz::json_t>(code, args[0], args[1], args[2], args[3], args[4]); break;

    case 6: future = handle->evaluate<glz::json_t>(code, args[0], args[1], args[2], args[3], args[4], args[5]); break;

    case 7: future = handle->evaluate<glz::json_t>(code, args[0], args[1], args[2], args[3], args[4], args[5], args[6]); break;

    case 8: future = handle->evaluate<glz::json_t>(code, args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]); break;

    default:

      Napi::RangeError::New(env, "Too many arguments for evaluate").ThrowAsJavaScriptException();

      return env.Undefined();

  }

  auto* worker = new EvaluateWorker(env, std::move(future), deferred);

  worker->Queue();



  return deferred.Promise();

}



Webview* Webview::FromHandle(saucer_handle* handle) {

  std::scoped_lock lock(instance_mutex_);

  auto it = instances_.find(handle);

  if (it == instances_.end()) {

    return nullptr;

  }

  return it->second;

}



bool Webview::MessageCallback(saucer_handle* handle, const char* message) {

  Webview* self = FromHandle(handle);

  if (!self || !self->message_tsfn_) {

    return false;

  }



  auto* payload = new std::string(message ? std::string(message) : std::string{});



  napi_status status = self->message_tsfn_.NonBlockingCall(payload,

    [](Napi::Env env, Napi::Function jsCallback, std::string* data) {

      Napi::HandleScope scope(env);

      try {

        jsCallback.Call({ Napi::String::New(env, *data) });

      } catch (const Napi::Error& err) {

        err.ThrowAsJavaScriptException();

      }

      delete data;

    });



  if (status != napi_ok) {

    delete payload;

    return false;

  }



  return true;

}



std::string Webview::StringifyForRPC(Napi::Env env, Napi::Value value) {

  if (value.IsUndefined() || value.IsNull()) {

    return "null";

  }

  Napi::Object json = env.Global().Get("JSON").As<Napi::Object>();
  Napi::Function stringify = json.Get("stringify").As<Napi::Function>();



  if (value.IsObject()) {

    Napi::Object obj = value.As<Napi::Object>();

    if (obj.Has("message") && obj.Get("message").IsString()) {

      auto msg = obj.Get("message");

      Napi::Value jsonVal = stringify.Call(json, { msg });

      if (jsonVal.IsString()) {

        return jsonVal.As<Napi::String>().Utf8Value();

      }

    }

  }



  Napi::Value jsonVal = stringify.Call(json, { value });

  if (!jsonVal.IsString()) {

    throw Napi::Error::New(env, "Failed to stringify RPC value");

  }

  return jsonVal.As<Napi::String>().Utf8Value();

}



glz::json_t Webview::SerializeForRPC(Napi::Env env, Napi::Value value) {

  if (value.IsUndefined() || value.IsNull()) {

    auto nullValue = glz::read_json<glz::json_t>("null");
    return nullValue ? std::move(nullValue.value()) : glz::json_t{};

  }

  auto jsonStr = StringifyForRPC(env, value);
  auto parsed = glz::read_json<glz::json_t>(jsonStr);

  if (!parsed) {

    throw Napi::Error::New(env, "Failed to parse RPC payload");

  }

  return std::move(parsed.value());

}



Napi::Value Webview::ParseJson(Napi::Env env, const std::string& jsonStr) {

  Napi::Object json = env.Global().Get("JSON").As<Napi::Object>();

  Napi::Function parse = json.Get("parse").As<Napi::Function>();

  return parse.Call(json, { Napi::String::New(env, jsonStr) });

}



std::vector<glz::json_t> Webview::CollectJsonArgs(const Napi::CallbackInfo& info, size_t startIndex) {

  Napi::Env env = info.Env();

  std::vector<glz::json_t> args;



  if (info.Length() <= startIndex) {

    return args;

  }



  Napi::Object json = env.Global().Get("JSON").As<Napi::Object>();

  Napi::Function stringify = json.Get("stringify").As<Napi::Function>();



  for (size_t i = startIndex; i < info.Length(); ++i) {

    Napi::Value strVal = stringify.Call(json, { info[i] });

    if (!strVal.IsString()) {

      throw Napi::TypeError::New(env, "Failed to serialize argument to JSON");

    }



    std::string jsonStr = strVal.As<Napi::String>().Utf8Value();

    auto parsed = glz::read_json<glz::json_t>(jsonStr);

    if (!parsed) {

      throw Napi::Error::New(env, "Failed to parse serialized argument");

    }



    args.push_back(std::move(parsed).value());

  }



  return args;

}



bool Webview::RegisterEvent(const std::string& event, Napi::Function cb, bool once) {

  Napi::Env env = cb.Env();



  SAUCER_WINDOW_EVENT window_event;

  SAUCER_WEB_EVENT web_event;

  bool is_window = MapWindowEventName(event, window_event);

  bool is_web = !is_window && MapWebEventName(event, web_event);



  if (!is_window && !is_web) {

    return false;

  }



  auto tsfn = Napi::ThreadSafeFunction::New(

    env,

    cb,

    "saucer.webview.event",

    0,

    1

  );



  auto data = std::make_shared<CallbackData>();

  data->tsfn = tsfn;

  data->once = once;

  data->id = 0;

  data->callback_ref = Napi::Persistent(cb);

  data->callback_ref.SuppressDestruct();



  bool registered = false;



  if (is_window) {

    switch (window_event) {

      case SAUCER_WINDOW_EVENT_DECORATED:

        if (once) { saucer_window_once(webview_, window_event, reinterpret_cast<void*>(&Webview::OnWindowDecorated)); }

        else { data->id = saucer_window_on(webview_, window_event, reinterpret_cast<void*>(&Webview::OnWindowDecorated)); }

        registered = true;

        break;

      case SAUCER_WINDOW_EVENT_MAXIMIZE:

        if (once) { saucer_window_once(webview_, window_event, reinterpret_cast<void*>(&Webview::OnWindowMaximize)); }

        else { data->id = saucer_window_on(webview_, window_event, reinterpret_cast<void*>(&Webview::OnWindowMaximize)); }

        registered = true;

        break;

      case SAUCER_WINDOW_EVENT_MINIMIZE:

        if (once) { saucer_window_once(webview_, window_event, reinterpret_cast<void*>(&Webview::OnWindowMinimize)); }

        else { data->id = saucer_window_on(webview_, window_event, reinterpret_cast<void*>(&Webview::OnWindowMinimize)); }

        registered = true;

        break;

      case SAUCER_WINDOW_EVENT_CLOSED:

        if (once) { saucer_window_once(webview_, window_event, reinterpret_cast<void*>(&Webview::OnWindowClosed)); }

        else { data->id = saucer_window_on(webview_, window_event, reinterpret_cast<void*>(&Webview::OnWindowClosed)); }

        registered = true;

        break;

      case SAUCER_WINDOW_EVENT_RESIZE:

        if (once) { saucer_window_once(webview_, window_event, reinterpret_cast<void*>(&Webview::OnWindowResize)); }

        else { data->id = saucer_window_on(webview_, window_event, reinterpret_cast<void*>(&Webview::OnWindowResize)); }

        registered = true;

        break;

      case SAUCER_WINDOW_EVENT_FOCUS:

        if (once) { saucer_window_once(webview_, window_event, reinterpret_cast<void*>(&Webview::OnWindowFocus)); }

        else { data->id = saucer_window_on(webview_, window_event, reinterpret_cast<void*>(&Webview::OnWindowFocus)); }

        registered = true;

        break;

      case SAUCER_WINDOW_EVENT_CLOSE:

        if (once) { saucer_window_once(webview_, window_event, reinterpret_cast<void*>(&Webview::OnWindowClose)); }

        else { data->id = saucer_window_on(webview_, window_event, reinterpret_cast<void*>(&Webview::OnWindowClose)); }

        registered = true;

        break;

    }

  } else if (is_web) {

    switch (web_event) {

      case SAUCER_WEB_EVENT_DOM_READY:

        if (once) { saucer_webview_once(webview_, web_event, reinterpret_cast<void*>(&Webview::OnWebDomReady)); }

        else { data->id = saucer_webview_on(webview_, web_event, reinterpret_cast<void*>(&Webview::OnWebDomReady)); }

        registered = true;

        break;

      case SAUCER_WEB_EVENT_NAVIGATED:

        if (once) { saucer_webview_once(webview_, web_event, reinterpret_cast<void*>(&Webview::OnWebNavigated)); }

        else { data->id = saucer_webview_on(webview_, web_event, reinterpret_cast<void*>(&Webview::OnWebNavigated)); }

        registered = true;

        break;

      case SAUCER_WEB_EVENT_NAVIGATE:

        if (once) { saucer_webview_once(webview_, web_event, reinterpret_cast<void*>(&Webview::OnWebNavigate)); }

        else { data->id = saucer_webview_on(webview_, web_event, reinterpret_cast<void*>(&Webview::OnWebNavigate)); }

        registered = true;

        break;

      case SAUCER_WEB_EVENT_FAVICON:

        if (once) { saucer_webview_once(webview_, web_event, reinterpret_cast<void*>(&Webview::OnWebFavicon)); }

        else { data->id = saucer_webview_on(webview_, web_event, reinterpret_cast<void*>(&Webview::OnWebFavicon)); }

        registered = true;

        break;

      case SAUCER_WEB_EVENT_TITLE:

        if (once) { saucer_webview_once(webview_, web_event, reinterpret_cast<void*>(&Webview::OnWebTitle)); }

        else { data->id = saucer_webview_on(webview_, web_event, reinterpret_cast<void*>(&Webview::OnWebTitle)); }

        registered = true;

        break;

      case SAUCER_WEB_EVENT_LOAD:

        if (once) { saucer_webview_once(webview_, web_event, reinterpret_cast<void*>(&Webview::OnWebLoad)); }

        else { data->id = saucer_webview_on(webview_, web_event, reinterpret_cast<void*>(&Webview::OnWebLoad)); }

        registered = true;

        break;

    }

  }



  if (registered) {

    std::scoped_lock lock(callback_mutex_);

    event_callbacks_[event].push_back(data);

  } else {

    tsfn.Release();

    data->callback_ref.Reset();

  }



  return registered;

}



void Webview::EmitEvent(const std::string& event, std::function<void(Napi::Env, Napi::Function)> invoker) {

  std::vector<std::shared_ptr<CallbackData>> callbacks;

  {

    std::scoped_lock lock(callback_mutex_);

    auto it = event_callbacks_.find(event);

    if (it == event_callbacks_.end()) {

      return;

    }

    callbacks = it->second;

  }



  for (auto& cb : callbacks) {

    auto* fn = new std::function<void(Napi::Env, Napi::Function)>(invoker);



    cb->tsfn.BlockingCall(fn,

      [](Napi::Env env, Napi::Function jsCallback, std::function<void(Napi::Env, Napi::Function)>* data) {

        Napi::HandleScope scope(env);

        (*data)(env, jsCallback);

        delete data;

      });



    if (cb->once) {

      cb->tsfn.Release();

    }

  }



  RemoveOnceCallbacks(event);

}



bool Webview::EvaluatePolicy(const std::string& event, std::function<Napi::Value(Napi::Env, Napi::Function)> invoker) {
  std::vector<std::shared_ptr<CallbackData>> callbacks;
  {
    std::scoped_lock lock(callback_mutex_);
    auto it = event_callbacks_.find(event);
    if (it == event_callbacks_.end()) {
      return true;
    }
    callbacks = it->second;
  }

  struct PolicyPayload {
    std::shared_ptr<std::function<Napi::Value(Napi::Env, Napi::Function)>> invoker;
    std::shared_ptr<bool> allow;
  };

  auto allow_state = std::make_shared<bool>(true);
  auto invoker_ptr = std::make_shared<std::function<Napi::Value(Napi::Env, Napi::Function)>>(std::move(invoker));

  for (auto& cb : callbacks) {
    auto* payload = new PolicyPayload{ invoker_ptr, allow_state };

    cb->tsfn.BlockingCall(payload,
      [](Napi::Env env, Napi::Function jsCallback, PolicyPayload* data) {
        Napi::HandleScope scope(env);
        try {
          Napi::Value result = (*(data->invoker))(env, jsCallback);

          if (result.IsBoolean()) {
            if (!result.As<Napi::Boolean>().Value()) {
              *(data->allow) = false;
            }
          } else if (result.IsString()) {
            std::string val = result.As<Napi::String>().Utf8Value();
            if (val == "block") {
              *(data->allow) = false;
            }
          }
        } catch (const Napi::Error& err) {
          err.ThrowAsJavaScriptException();
          *(data->allow) = false;
        } catch (...) {
          *(data->allow) = false;
        }

        delete data;
      });

    if (cb->once) {
      cb->tsfn.Release();
    }
  }

  RemoveOnceCallbacks(event);
  return *allow_state;
}

void Webview::RemoveOnceCallbacks(const std::string& event) {

  std::scoped_lock lock(callback_mutex_);

  auto it = event_callbacks_.find(event);

  if (it == event_callbacks_.end()) return;



  auto& vec = it->second;

  vec.erase(std::remove_if(vec.begin(), vec.end(), [](std::shared_ptr<CallbackData>& cb) {

    if (cb->once) {

      cb->callback_ref.Reset();

      return true;

    }

    return false;

  }), vec.end());

}



void Webview::RemoveCallbackById(const std::string& event, uint64_t id) {

  std::scoped_lock lock(callback_mutex_);

  auto it = event_callbacks_.find(event);

  if (it == event_callbacks_.end()) return;



  auto& vec = it->second;

  vec.erase(std::remove_if(vec.begin(), vec.end(), [id](std::shared_ptr<CallbackData>& cb) {

    if (cb->id == id) {

      cb->tsfn.Release();

      cb->callback_ref.Reset();

      return true;

    }

    return false;

  }), vec.end());

}



void Webview::RemoveCallbackByFunction(const std::string& event, Napi::Function cb) {

  std::vector<uint64_t> ids;



  {

    std::scoped_lock lock(callback_mutex_);

    auto it = event_callbacks_.find(event);

    if (it == event_callbacks_.end()) return;



    auto& vec = it->second;

    vec.erase(std::remove_if(vec.begin(), vec.end(), [&](std::shared_ptr<CallbackData>& data) {

      bool match = data->callback_ref.Value().StrictEquals(cb);

      if (match) {

        if (data->id != 0) {

          ids.push_back(data->id);

        }

        data->tsfn.Release();

        data->callback_ref.Reset();

      }

      return match;

    }), vec.end());

  }



  SAUCER_WINDOW_EVENT window_event;

  SAUCER_WEB_EVENT web_event;



  if (MapWindowEventName(event, window_event)) {

    for (auto id : ids) {

      saucer_window_remove(webview_, window_event, id);

    }

  } else if (MapWebEventName(event, web_event)) {

    for (auto id : ids) {

      saucer_webview_remove(webview_, web_event, id);

    }

  }

}



void Webview::RemoveAllCallbacks(const std::string& event) {

  SAUCER_WINDOW_EVENT window_event;

  SAUCER_WEB_EVENT web_event;



  {

    std::scoped_lock lock(callback_mutex_);

    auto it = event_callbacks_.find(event);

    if (it == event_callbacks_.end()) {

      return;

    }



    for (auto& cb : it->second) {

      cb->tsfn.Release();

      cb->callback_ref.Reset();

    }



    event_callbacks_.erase(it);

  }



  if (MapWindowEventName(event, window_event)) {

    saucer_window_clear(webview_, window_event);

  } else if (MapWebEventName(event, web_event)) {

    saucer_webview_clear(webview_, web_event);

  }

}



bool Webview::HasCallbacks(const std::string& event) {

  std::scoped_lock lock(callback_mutex_);

  auto it = event_callbacks_.find(event);

  return it != event_callbacks_.end() && !it->second.empty();

}



void Webview::OnWindowDecorated(saucer_handle* handle, bool decorated) {

  if (Webview* self = FromHandle(handle)) {

    self->EmitEvent("decorated", [decorated](Napi::Env env, Napi::Function cb) {

      cb.Call({ Napi::Boolean::New(env, decorated) });

    });

  }

}



void Webview::OnWindowMaximize(saucer_handle* handle, bool maximized) {

  if (Webview* self = FromHandle(handle)) {

    self->EmitEvent("maximize", [maximized](Napi::Env env, Napi::Function cb) {

      cb.Call({ Napi::Boolean::New(env, maximized) });

    });

  }

}



void Webview::OnWindowMinimize(saucer_handle* handle, bool minimized) {

  if (Webview* self = FromHandle(handle)) {
    self->minimized_hint_valid_ = true;
    self->minimized_hint_ = minimized;

    self->EmitEvent("minimize", [minimized](Napi::Env env, Napi::Function cb) {

      cb.Call({ Napi::Boolean::New(env, minimized) });

    });

  }

}



void Webview::OnWindowClosed(saucer_handle* handle) {

  if (Webview* self = FromHandle(handle)) {

    self->EmitEvent("closed", [](Napi::Env env, Napi::Function cb) {

      cb.Call({});

    });

  }

}



void Webview::OnWindowResize(saucer_handle* handle, int width, int height) {

  if (Webview* self = FromHandle(handle)) {

    self->EmitEvent("resize", [width, height](Napi::Env env, Napi::Function cb) {

      cb.Call({ Napi::Number::New(env, width), Napi::Number::New(env, height) });

    });

  }

}



void Webview::OnWindowFocus(saucer_handle* handle, bool focused) {

  if (Webview* self = FromHandle(handle)) {

    self->EmitEvent("focus", [focused](Napi::Env env, Napi::Function cb) {

      cb.Call({ Napi::Boolean::New(env, focused) });

    });

  }

}



SAUCER_POLICY Webview::OnWindowClose(saucer_handle* handle) {

  if (Webview* self = FromHandle(handle)) {

    bool allow = self->EvaluatePolicy("close", [](Napi::Env env, Napi::Function cb) {

      return cb.Call({});

    });

    return allow ? SAUCER_POLICY_ALLOW : SAUCER_POLICY_BLOCK;

  }

  return SAUCER_POLICY_ALLOW;

}



void Webview::OnWebDomReady(saucer_handle* handle) {

  if (Webview* self = FromHandle(handle)) {

    self->EmitEvent("dom-ready", [](Napi::Env env, Napi::Function cb) {

      cb.Call({});

    });

  }

}



void Webview::OnWebNavigated(saucer_handle* handle, const char* url) {

  if (Webview* self = FromHandle(handle)) {

    std::string urlStr = url ? url : "";

    self->EmitEvent("navigated", [urlStr](Napi::Env env, Napi::Function cb) {

      cb.Call({ Napi::String::New(env, urlStr) });

    });

  }

}



SAUCER_POLICY Webview::OnWebNavigate(saucer_handle* handle, saucer_navigation* nav) {

  if (Webview* self = FromHandle(handle)) {

    char* url = saucer_navigation_url(nav);

    std::string urlStr = url ? url : "";

    saucer_memory_free(url);



    bool newWindow = saucer_navigation_new_window(nav);

    bool redirection = saucer_navigation_redirection(nav);

    bool userInitiated = saucer_navigation_user_initiated(nav);



    bool allow = self->EvaluatePolicy("navigate", [urlStr, newWindow, redirection, userInitiated](Napi::Env env, Napi::Function cb) {

      Napi::Object obj = Napi::Object::New(env);

      obj.Set("url", Napi::String::New(env, urlStr));

      obj.Set("newWindow", Napi::Boolean::New(env, newWindow));

      obj.Set("redirection", Napi::Boolean::New(env, redirection));

      obj.Set("userInitiated", Napi::Boolean::New(env, userInitiated));

      return cb.Call({ obj });

    });



    saucer_navigation_free(nav);

    return allow ? SAUCER_POLICY_ALLOW : SAUCER_POLICY_BLOCK;

  }



  saucer_navigation_free(nav);

  return SAUCER_POLICY_ALLOW;

}



void Webview::OnWebFavicon(saucer_handle* handle, saucer_icon* icon) {

  if (Webview* self = FromHandle(handle)) {

    if (!self->HasCallbacks("favicon")) {

      if (icon) {

        saucer_icon_free(icon);

      }

      return;

    }



    self->EmitEvent("favicon", [icon](Napi::Env env, Napi::Function cb) {

      Napi::Value payload = env.Null();



      if (icon && !saucer_icon_empty(icon)) {

        saucer_stash* stash = saucer_icon_data(icon);

        if (stash) {

          size_t size = saucer_stash_size(stash);

          const uint8_t* data = saucer_stash_data(stash);

          payload = Napi::Buffer<uint8_t>::Copy(env, data, size);

          saucer_stash_free(stash);

        }

      }



      cb.Call({ payload });

      saucer_icon_free(icon);

    });

  } else if (icon) {

    saucer_icon_free(icon);

  }

}



void Webview::OnWebTitle(saucer_handle* handle, const char* title) {

  if (Webview* self = FromHandle(handle)) {

    std::string titleStr = title ? title : "";

    self->EmitEvent("title", [titleStr](Napi::Env env, Napi::Function cb) {

      cb.Call({ Napi::String::New(env, titleStr) });

    });

  }

}



void Webview::OnWebLoad(saucer_handle* handle, SAUCER_STATE state) {

  if (Webview* self = FromHandle(handle)) {

    std::string status = state == SAUCER_STATE_STARTED ? "started" : "finished";

    self->EmitEvent("load", [status](Napi::Env env, Napi::Function cb) {

      cb.Call({ Napi::String::New(env, status) });

    });

  }

}



// ============================================================================
// Stash Class - Wraps saucer_stash for raw byte handling
// ============================================================================

class Stash : public Napi::ObjectWrap<Stash> {
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  static Napi::FunctionReference constructor;

  Stash(const Napi::CallbackInfo& info);
  ~Stash();

  // Create a Stash wrapper from an existing saucer_stash (takes ownership)
  static Napi::Object Wrap(Napi::Env env, saucer_stash* stash);

  saucer_stash* GetStash() { return stash_; }

private:
  saucer_stash* stash_ = nullptr;
  bool owns_stash_ = true;

  // Static methods
  static Napi::Value From(const Napi::CallbackInfo& info);
  static Napi::Value View(const Napi::CallbackInfo& info);

  // Instance methods
  Napi::Value GetSize(const Napi::CallbackInfo& info);
  Napi::Value GetData(const Napi::CallbackInfo& info);
};

Napi::FunctionReference Stash::constructor;

Napi::Object Stash::Init(Napi::Env env, Napi::Object exports) {
  Napi::Function func = DefineClass(env, "Stash", {
    StaticMethod("from", &Stash::From),
    StaticMethod("view", &Stash::View),
    InstanceAccessor("size", &Stash::GetSize, nullptr),
    InstanceMethod("getData", &Stash::GetData),
  });

  constructor = Napi::Persistent(func);
  constructor.SuppressDestruct();

  exports.Set("Stash", func);
  return exports;
}

Stash::Stash(const Napi::CallbackInfo& info) : Napi::ObjectWrap<Stash>(info) {
  Napi::Env env = info.Env();

  // Internal constructor - stash is set via Wrap or static methods
  if (info.Length() > 0 && info[0].IsExternal()) {
    stash_ = info[0].As<Napi::External<saucer_stash>>().Data();
    owns_stash_ = true;
  }
}

Stash::~Stash() {
  if (stash_ && owns_stash_) {
    saucer_stash_free(stash_);
    stash_ = nullptr;
  }
}

Napi::Object Stash::Wrap(Napi::Env env, saucer_stash* stash) {
  Napi::External<saucer_stash> ext = Napi::External<saucer_stash>::New(env, stash);
  return constructor.New({ ext });
}

Napi::Value Stash::From(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() == 0 || !info[0].IsBuffer()) {
    Napi::TypeError::New(env, "Stash.from requires a Buffer argument").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  Napi::Buffer<uint8_t> buffer = info[0].As<Napi::Buffer<uint8_t>>();
  saucer_stash* stash = saucer_stash_from(buffer.Data(), buffer.Length());

  if (!stash) {
    return env.Null();
  }

  return Wrap(env, stash);
}

Napi::Value Stash::View(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() == 0 || !info[0].IsBuffer()) {
    Napi::TypeError::New(env, "Stash.view requires a Buffer argument").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  Napi::Buffer<uint8_t> buffer = info[0].As<Napi::Buffer<uint8_t>>();
  saucer_stash* stash = saucer_stash_view(buffer.Data(), buffer.Length());

  if (!stash) {
    return env.Null();
  }

  return Wrap(env, stash);
}

Napi::Value Stash::GetSize(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (!stash_) {
    return Napi::Number::New(env, 0);
  }

  return Napi::Number::New(env, static_cast<double>(saucer_stash_size(stash_)));
}

Napi::Value Stash::GetData(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (!stash_) {
    return env.Null();
  }

  const uint8_t* data = saucer_stash_data(stash_);
  size_t size = saucer_stash_size(stash_);

  if (!data || size == 0) {
    return env.Null();
  }

  return Napi::Buffer<uint8_t>::Copy(env, data, size);
}

// ============================================================================
// Icon Class - Wraps saucer_icon for image handling
// ============================================================================

class Icon : public Napi::ObjectWrap<Icon> {
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  static Napi::FunctionReference constructor;

  Icon(const Napi::CallbackInfo& info);
  ~Icon();

  // Create an Icon wrapper from an existing saucer_icon (takes ownership)
  static Napi::Object Wrap(Napi::Env env, saucer_icon* icon);

  saucer_icon* GetIcon() { return icon_; }

private:
  saucer_icon* icon_ = nullptr;

  // Static methods
  static Napi::Value FromFile(const Napi::CallbackInfo& info);
  static Napi::Value FromData(const Napi::CallbackInfo& info);

  // Instance methods
  Napi::Value IsEmpty(const Napi::CallbackInfo& info);
  Napi::Value GetData(const Napi::CallbackInfo& info);
  void Save(const Napi::CallbackInfo& info);
};

Napi::FunctionReference Icon::constructor;

Napi::Object Icon::Init(Napi::Env env, Napi::Object exports) {
  Napi::Function func = DefineClass(env, "Icon", {
    StaticMethod("fromFile", &Icon::FromFile),
    StaticMethod("fromData", &Icon::FromData),
    InstanceMethod("isEmpty", &Icon::IsEmpty),
    InstanceMethod("getData", &Icon::GetData),
    InstanceMethod("save", &Icon::Save),
  });

  constructor = Napi::Persistent(func);
  constructor.SuppressDestruct();

  exports.Set("Icon", func);
  return exports;
}

Icon::Icon(const Napi::CallbackInfo& info) : Napi::ObjectWrap<Icon>(info) {
  Napi::Env env = info.Env();

  // Internal constructor - icon is set via Wrap or static methods
  if (info.Length() > 0 && info[0].IsExternal()) {
    icon_ = info[0].As<Napi::External<saucer_icon>>().Data();
  }
}

Icon::~Icon() {
  if (icon_) {
    saucer_icon_free(icon_);
    icon_ = nullptr;
  }
}

Napi::Object Icon::Wrap(Napi::Env env, saucer_icon* icon) {
  Napi::External<saucer_icon> ext = Napi::External<saucer_icon>::New(env, icon);
  return constructor.New({ ext });
}

Napi::Value Icon::FromFile(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() == 0 || !info[0].IsString()) {
    Napi::TypeError::New(env, "Icon.fromFile requires a file path string").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  std::string path = info[0].As<Napi::String>().Utf8Value();

  saucer_icon* icon = nullptr;
  saucer_icon_from_file(&icon, path.c_str());

  if (!icon) {
    return env.Null();
  }

  return Wrap(env, icon);
}

Napi::Value Icon::FromData(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() == 0 || !info[0].IsBuffer()) {
    Napi::TypeError::New(env, "Icon.fromData requires a Buffer argument").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  Napi::Buffer<uint8_t> buffer = info[0].As<Napi::Buffer<uint8_t>>();

  // Create a stash from the buffer data
  saucer_stash* stash = saucer_stash_from(buffer.Data(), buffer.Length());
  if (!stash) {
    return env.Null();
  }

  saucer_icon* icon = nullptr;
  saucer_icon_from_data(&icon, stash);
  saucer_stash_free(stash);

  if (!icon) {
    return env.Null();
  }

  return Wrap(env, icon);
}

Napi::Value Icon::IsEmpty(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (!icon_) {
    return Napi::Boolean::New(env, true);
  }

  return Napi::Boolean::New(env, saucer_icon_empty(icon_));
}

Napi::Value Icon::GetData(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (!icon_ || saucer_icon_empty(icon_)) {
    return env.Null();
  }

  saucer_stash* stash = saucer_icon_data(icon_);
  if (!stash) {
    return env.Null();
  }

  const uint8_t* data = saucer_stash_data(stash);
  size_t size = saucer_stash_size(stash);

  Napi::Buffer<uint8_t> buffer = Napi::Buffer<uint8_t>::Copy(env, data, size);
  saucer_stash_free(stash);

  return buffer;
}

void Icon::Save(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() == 0 || !info[0].IsString()) {
    Napi::TypeError::New(env, "Icon.save requires a file path string").ThrowAsJavaScriptException();
    return;
  }

  if (!icon_) {
    Napi::Error::New(env, "Icon is not initialized").ThrowAsJavaScriptException();
    return;
  }

  std::string path = info[0].As<Napi::String>().Utf8Value();
  saucer_icon_save(icon_, path.c_str());
}

// ============================================================================
// Desktop Class - Native file dialogs and system integration
// ============================================================================

class Desktop : public Napi::ObjectWrap<Desktop> {
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  static Napi::FunctionReference constructor;

  Desktop(const Napi::CallbackInfo& info);
  ~Desktop();

private:
  saucer_desktop* desktop_ = nullptr;

  // Methods
  void Open(const Napi::CallbackInfo& info);
  Napi::Value PickFile(const Napi::CallbackInfo& info);
  Napi::Value PickFolder(const Napi::CallbackInfo& info);
  Napi::Value PickFiles(const Napi::CallbackInfo& info);
  Napi::Value PickFolders(const Napi::CallbackInfo& info);
};

Napi::FunctionReference Desktop::constructor;

Napi::Object Desktop::Init(Napi::Env env, Napi::Object exports) {
  Napi::Function func = DefineClass(env, "Desktop", {
    InstanceMethod("open", &Desktop::Open),
    InstanceMethod("pickFile", &Desktop::PickFile),
    InstanceMethod("pickFolder", &Desktop::PickFolder),
    InstanceMethod("pickFiles", &Desktop::PickFiles),
    InstanceMethod("pickFolders", &Desktop::PickFolders),
  });

  constructor = Napi::Persistent(func);
  constructor.SuppressDestruct();

  exports.Set("Desktop", func);
  return exports;
}

Desktop::Desktop(const Napi::CallbackInfo& info) : Napi::ObjectWrap<Desktop>(info) {
  Napi::Env env = info.Env();

  if (info.Length() == 0) {
    Napi::TypeError::New(env, "Desktop requires an Application instance").ThrowAsJavaScriptException();
    return;
  }

  // Get the native Application instance
  Napi::Object appObj = info[0].As<Napi::Object>();
  if (!appObj.Has("_native")) {
    Napi::TypeError::New(env, "Invalid Application instance").ThrowAsJavaScriptException();
    return;
  }

  // We need to access the native saucer_application
  // This is tricky because we need to get the internal Application wrapper
  // For simplicity, we'll store the app reference and access it later
  saucer_application* app = nullptr;

  // Try to get the app from the native binding
  Napi::Value nativeVal = appObj.Get("_native");
  if (nativeVal.IsObject()) {
    Napi::Object nativeObj = nativeVal.As<Napi::Object>();
    // The native Application should have a method to get the raw pointer
    if (nativeObj.Has("nativeHandle") && nativeObj.Get("nativeHandle").IsFunction()) {
      Napi::Value handle = nativeObj.Get("nativeHandle").As<Napi::Function>().Call(nativeObj, {});
      if (handle.IsBigInt()) {
        bool lossless;
        app = reinterpret_cast<saucer_application*>(handle.As<Napi::BigInt>().Uint64Value(&lossless));
      }
    }
  }

  if (!app) {
    Napi::Error::New(env, "Could not get native Application handle").ThrowAsJavaScriptException();
    return;
  }

  desktop_ = saucer_desktop_new(app);
}

Desktop::~Desktop() {
  if (desktop_) {
    saucer_desktop_free(desktop_);
    desktop_ = nullptr;
  }
}

void Desktop::Open(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (!desktop_) {
    Napi::Error::New(env, "Desktop not initialized").ThrowAsJavaScriptException();
    return;
  }

  if (info.Length() == 0 || !info[0].IsString()) {
    Napi::TypeError::New(env, "open() requires a path string").ThrowAsJavaScriptException();
    return;
  }

  std::string path = info[0].As<Napi::String>().Utf8Value();
  saucer_desktop_open(desktop_, path.c_str());
}

Napi::Value Desktop::PickFile(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (!desktop_) {
    Napi::Error::New(env, "Desktop not initialized").ThrowAsJavaScriptException();
    return env.Null();
  }

  saucer_picker_options* options = saucer_picker_options_new();

  // Parse options if provided
  if (info.Length() > 0 && info[0].IsObject()) {
    Napi::Object opts = info[0].As<Napi::Object>();

    if (opts.Has("initial") && opts.Get("initial").IsString()) {
      std::string initial = opts.Get("initial").As<Napi::String>().Utf8Value();
      saucer_picker_options_set_initial(options, initial.c_str());
    }

    if (opts.Has("filters") && opts.Get("filters").IsArray()) {
      Napi::Array filters = opts.Get("filters").As<Napi::Array>();
      for (uint32_t i = 0; i < filters.Length(); ++i) {
        if (filters.Get(i).IsString()) {
          std::string filter = filters.Get(i).As<Napi::String>().Utf8Value();
          saucer_picker_options_add_filter(options, filter.c_str());
        }
      }
    }
  }

  char* result = saucer_desktop_pick_file(desktop_, options);
  saucer_picker_options_free(options);

  if (!result) {
    return env.Null();
  }

  Napi::String path = Napi::String::New(env, result);
  saucer_memory_free(result);
  return path;
}

Napi::Value Desktop::PickFolder(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (!desktop_) {
    Napi::Error::New(env, "Desktop not initialized").ThrowAsJavaScriptException();
    return env.Null();
  }

  saucer_picker_options* options = saucer_picker_options_new();

  if (info.Length() > 0 && info[0].IsObject()) {
    Napi::Object opts = info[0].As<Napi::Object>();

    if (opts.Has("initial") && opts.Get("initial").IsString()) {
      std::string initial = opts.Get("initial").As<Napi::String>().Utf8Value();
      saucer_picker_options_set_initial(options, initial.c_str());
    }
  }

  char* result = saucer_desktop_pick_folder(desktop_, options);
  saucer_picker_options_free(options);

  if (!result) {
    return env.Null();
  }

  Napi::String path = Napi::String::New(env, result);
  saucer_memory_free(result);
  return path;
}

Napi::Value Desktop::PickFiles(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (!desktop_) {
    Napi::Error::New(env, "Desktop not initialized").ThrowAsJavaScriptException();
    return env.Null();
  }

  saucer_picker_options* options = saucer_picker_options_new();

  if (info.Length() > 0 && info[0].IsObject()) {
    Napi::Object opts = info[0].As<Napi::Object>();

    if (opts.Has("initial") && opts.Get("initial").IsString()) {
      std::string initial = opts.Get("initial").As<Napi::String>().Utf8Value();
      saucer_picker_options_set_initial(options, initial.c_str());
    }

    if (opts.Has("filters") && opts.Get("filters").IsArray()) {
      Napi::Array filters = opts.Get("filters").As<Napi::Array>();
      for (uint32_t i = 0; i < filters.Length(); ++i) {
        if (filters.Get(i).IsString()) {
          std::string filter = filters.Get(i).As<Napi::String>().Utf8Value();
          saucer_picker_options_add_filter(options, filter.c_str());
        }
      }
    }
  }

  char** results = saucer_desktop_pick_files(desktop_, options);
  saucer_picker_options_free(options);

  if (!results) {
    return env.Null();
  }

  Napi::Array arr = Napi::Array::New(env);
  uint32_t idx = 0;
  for (char** p = results; *p != nullptr; ++p) {
    arr.Set(idx++, Napi::String::New(env, *p));
    saucer_memory_free(*p);
  }
  saucer_memory_free(results);

  return arr;
}

Napi::Value Desktop::PickFolders(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (!desktop_) {
    Napi::Error::New(env, "Desktop not initialized").ThrowAsJavaScriptException();
    return env.Null();
  }

  saucer_picker_options* options = saucer_picker_options_new();

  if (info.Length() > 0 && info[0].IsObject()) {
    Napi::Object opts = info[0].As<Napi::Object>();

    if (opts.Has("initial") && opts.Get("initial").IsString()) {
      std::string initial = opts.Get("initial").As<Napi::String>().Utf8Value();
      saucer_picker_options_set_initial(options, initial.c_str());
    }
  }

  char** results = saucer_desktop_pick_folders(desktop_, options);
  saucer_picker_options_free(options);

  if (!results) {
    return env.Null();
  }

  Napi::Array arr = Napi::Array::New(env);
  uint32_t idx = 0;
  for (char** p = results; *p != nullptr; ++p) {
    arr.Set(idx++, Napi::String::New(env, *p));
    saucer_memory_free(*p);
  }
  saucer_memory_free(results);

  return arr;
}

// ============================================================================
// PDF Class - Export webview content to PDF
// ============================================================================

class PDF : public Napi::ObjectWrap<PDF> {
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  static Napi::FunctionReference constructor;

  PDF(const Napi::CallbackInfo& info);
  ~PDF();

private:
  saucer_pdf* pdf_ = nullptr;

  void Save(const Napi::CallbackInfo& info);
};

Napi::FunctionReference PDF::constructor;

Napi::Object PDF::Init(Napi::Env env, Napi::Object exports) {
  Napi::Function func = DefineClass(env, "PDF", {
    InstanceMethod("save", &PDF::Save),
  });

  constructor = Napi::Persistent(func);
  constructor.SuppressDestruct();

  exports.Set("PDF", func);
  return exports;
}

PDF::PDF(const Napi::CallbackInfo& info) : Napi::ObjectWrap<PDF>(info) {
  Napi::Env env = info.Env();

  if (info.Length() == 0) {
    Napi::TypeError::New(env, "PDF requires a Webview instance").ThrowAsJavaScriptException();
    return;
  }

  // Get the native Webview instance
  Napi::Object webviewObj = info[0].As<Napi::Object>();
  if (!webviewObj.Has("_native")) {
    Napi::TypeError::New(env, "Invalid Webview instance").ThrowAsJavaScriptException();
    return;
  }

  // Get the webview handle
  saucer_handle* handle = nullptr;
  Napi::Value nativeVal = webviewObj.Get("_native");
  if (nativeVal.IsObject()) {
    // The native Webview object wraps the saucer_handle
    // We need to get this from the Webview's internal state
    // For now, we'll use a workaround by accessing through the ObjectWrap
    Napi::Object nativeObj = nativeVal.As<Napi::Object>();
    Webview* webview = Napi::ObjectWrap<Webview>::Unwrap(nativeObj);
    if (webview) {
      handle = webview->GetWebview();
    }
  }

  if (!handle) {
    Napi::Error::New(env, "Could not get native Webview handle").ThrowAsJavaScriptException();
    return;
  }

  pdf_ = saucer_pdf_new(handle);
}

PDF::~PDF() {
  if (pdf_) {
    saucer_pdf_free(pdf_);
    pdf_ = nullptr;
  }
}

void PDF::Save(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (!pdf_) {
    Napi::Error::New(env, "PDF not initialized").ThrowAsJavaScriptException();
    return;
  }

  saucer_print_settings* settings = saucer_print_settings_new();

  if (info.Length() > 0 && info[0].IsObject()) {
    Napi::Object opts = info[0].As<Napi::Object>();

    if (opts.Has("file") && opts.Get("file").IsString()) {
      std::string file = opts.Get("file").As<Napi::String>().Utf8Value();
      saucer_print_settings_set_file(settings, file.c_str());
    }

    if (opts.Has("orientation") && opts.Get("orientation").IsString()) {
      std::string orientation = opts.Get("orientation").As<Napi::String>().Utf8Value();
      if (orientation == "landscape") {
        saucer_print_settings_set_orientation(settings, SAUCER_LAYOUT_LANDSCAPE);
      } else {
        saucer_print_settings_set_orientation(settings, SAUCER_LAYOUT_PORTRAIT);
      }
    }

    if (opts.Has("width") && opts.Get("width").IsNumber()) {
      double width = opts.Get("width").As<Napi::Number>().DoubleValue();
      saucer_print_settings_set_width(settings, width);
    }

    if (opts.Has("height") && opts.Get("height").IsNumber()) {
      double height = opts.Get("height").As<Napi::Number>().DoubleValue();
      saucer_print_settings_set_height(settings, height);
    }
  }

  saucer_pdf_save(pdf_, settings);
  saucer_print_settings_free(settings);
}

// ============================================================================

// Module initialization

// ============================================================================



Napi::Object Init(Napi::Env env, Napi::Object exports) {

  Application::Init(env, exports);

  Webview::Init(env, exports);

  Stash::Init(env, exports);

  Icon::Init(env, exports);

  Desktop::Init(env, exports);

  PDF::Init(env, exports);

  // Premium features (platform-specific)
  Clipboard::Init(env, exports);
  Notification::Init(env, exports);
  SystemTray::Init(env, exports);

  return exports;

}



NODE_API_MODULE(saucer_nodejs, Init)



} // namespace saucer_nodejs
