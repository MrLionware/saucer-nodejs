#include "webview.hpp"

#include "icon.hpp"
#include "stash.hpp"
#include "script.hpp"
#include "scheme.hpp"
#include "preferences.hpp"
#include "navigation.hpp"
#include "url.hpp"

#include "utils/string.hpp"
#include "utils/handle.hpp"

#include <filesystem>
#include <optional>
#include <utility>

struct saucer_embedded_file : bindings::handle<saucer_embedded_file, saucer::embedded_file>
{
};

struct saucer_webview_options : bindings::handle<saucer_webview_options, saucer::smartview::options>
{
};

namespace
{
    saucer_navigation *make_navigation(const saucer::navigation &navigation)
    {
        return saucer_navigation::from({
            .url = navigation.url().string(),
            .new_window = navigation.new_window(),
            .redirection = navigation.redirection(),
            .user_initiated = navigation.user_initiated(),
        });
    }

    SAUCER_STATE to_legacy_state(const saucer::state &state)
    {
        return state == saucer::state::started ? SAUCER_STATE_STARTED : SAUCER_STATE_FINISHED;
    }
}

extern "C"
{
    saucer_embedded_file *saucer_embed(saucer_stash *content, const char *mime)
    {
        return saucer_embedded_file::make(content->value(), mime ? mime : "application/octet-stream");
    }

    void saucer_embed_free(saucer_embedded_file *handle)
    {
        delete handle;
    }

    void saucer_webview_options_free(saucer_webview_options *options)
    {
        delete options;
    }

    saucer_webview_options *saucer_webview_options_new(saucer_window *window)
    {
        auto *handle = reinterpret_cast<saucer_handle *>(window);
        if (!handle || !handle->window)
        {
            return nullptr;
        }

        return saucer_webview_options::from({.window = handle->window});
    }

    void saucer_webview_options_set_attributes(saucer_webview_options *options, bool value)
    {
        options->value().attributes = value;
    }

    void saucer_webview_options_set_persistent_cookies(saucer_webview_options *options, bool value)
    {
        options->value().persistent_cookies = value;
    }

    void saucer_webview_options_set_hardware_acceleration(saucer_webview_options *options, bool value)
    {
        options->value().hardware_acceleration = value;
    }

    void saucer_webview_options_set_storage_path(saucer_webview_options *options, const char *value)
    {
        options->value().storage_path = value ? std::filesystem::path{value} : std::filesystem::path{};
    }

    void saucer_webview_options_set_user_agent(saucer_webview_options *options, const char *value)
    {
        options->value().user_agent = value ? std::optional<std::string>{value} : std::nullopt;
    }

    void saucer_webview_options_append_browser_flag(saucer_webview_options *options, const char *value)
    {
        if (value)
        {
            options->value().browser_flags.emplace(value);
        }
    }

    saucer_handle *saucer_new(saucer_preferences *prefs)
    {
        if (!prefs || !prefs->value().application)
        {
            return nullptr;
        }

        auto window = saucer::window::create(prefs->value().application.get());
        if (!window.has_value())
        {
            return nullptr;
        }

        saucer::smartview::options options{
            .window = window.value(),
        };
        options.persistent_cookies = prefs->value().persistent_cookies;
        options.hardware_acceleration = prefs->value().hardware_acceleration;
        options.storage_path = prefs->value().storage_path;
        options.user_agent = prefs->value().user_agent;
        options.browser_flags = prefs->value().browser_flags;

        auto view = saucer::smartview::create(options);
        if (!view.has_value())
        {
            return nullptr;
        }

        auto *handle = new saucer_handle{.view = std::move(view.value())};
        handle->window = window.value();
        return handle;
    }

    saucer_webview *saucer_webview_new(saucer_webview_options *options, int *error)
    {
        if (!options)
        {
            if (error)
            {
                *error = -1;
            }

            return nullptr;
        }

        auto created = saucer::smartview::create(options->value());
        if (!created.has_value())
        {
            if (error)
            {
                *error = created.error().code();
            }

            return nullptr;
        }

        auto *handle = new saucer_handle{.view = std::move(created).value()};
        handle->window = options->value().window.value();
        return reinterpret_cast<saucer_webview *>(handle);
    }

    void saucer_webview_free(saucer_webview *webview)
    {
        delete reinterpret_cast<saucer_handle *>(webview);
    }

    void saucer_free(saucer_handle *handle)
    {
        delete handle;
    }

    void saucer_webview_on_message(saucer_handle *handle, saucer_on_message callback)
    {
        handle->m_on_message = callback;

        if (handle->m_message_listener.has_value())
        {
            handle->view.off(saucer::webview::event::message, handle->m_message_listener.value());
            handle->m_message_listener.reset();
        }

        if (!callback)
        {
            return;
        }

        handle->m_message_listener = handle->view.on<saucer::webview::event::message>(
            [handle](std::string_view message)
            {
                if (!handle->m_on_message)
                {
                    return saucer::status::unhandled;
                }

                const std::string copy(message);
                return handle->m_on_message(handle, copy.c_str()) ? saucer::status::handled : saucer::status::unhandled;
            });
    }

    saucer_icon *saucer_webview_favicon(saucer_handle *handle)
    {
        return saucer_icon::from(handle->view.favicon());
    }

    char *saucer_webview_page_title(saucer_handle *handle)
    {
        return bindings::alloc(handle->view.page_title());
    }

    bool saucer_webview_dev_tools(saucer_handle *handle)
    {
        return handle->view.dev_tools();
    }

    char *saucer_webview_url(saucer_handle *handle)
    {
        const auto url = handle->view.url();
        if (!url.has_value())
        {
            return nullptr;
        }

        return bindings::alloc(url->string());
    }

    bool saucer_webview_context_menu(saucer_handle *handle)
    {
        return handle->view.context_menu();
    }

    void saucer_webview_background(saucer_handle *handle, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a)
    {
        const auto color = handle->view.background();
        *r = color.r;
        *g = color.g;
        *b = color.b;
        *a = color.a;
    }

    bool saucer_webview_force_dark_mode(saucer_handle *handle)
    {
        return handle->view.force_dark();
    }

    bool saucer_webview_force_dark(saucer_webview *webview)
    {
        auto *handle = reinterpret_cast<saucer_handle *>(webview);
        return saucer_webview_force_dark_mode(handle);
    }

    void saucer_webview_bounds(saucer_webview *webview, int *x, int *y, int *w, int *h)
    {
        auto *handle = reinterpret_cast<saucer_handle *>(webview);
        const auto bounds = handle->view.bounds();
        *x = bounds.x;
        *y = bounds.y;
        *w = bounds.w;
        *h = bounds.h;
    }

    void saucer_webview_set_dev_tools(saucer_handle *handle, bool enabled)
    {
        handle->view.set_dev_tools(enabled);
    }

    void saucer_webview_set_context_menu(saucer_handle *handle, bool enabled)
    {
        handle->view.set_context_menu(enabled);
    }

    void saucer_webview_set_force_dark_mode(saucer_handle *handle, bool enabled)
    {
        handle->view.set_force_dark(enabled);
    }

    void saucer_webview_set_force_dark(saucer_webview *webview, bool enabled)
    {
        auto *handle = reinterpret_cast<saucer_handle *>(webview);
        saucer_webview_set_force_dark_mode(handle, enabled);
    }

    void saucer_webview_set_background(saucer_handle *handle, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    {
        handle->view.set_background({.r = r, .g = g, .b = b, .a = a});
    }

    void saucer_webview_reset_bounds(saucer_webview *webview)
    {
        auto *handle = reinterpret_cast<saucer_handle *>(webview);
        handle->view.reset_bounds();
    }

    void saucer_webview_set_bounds(saucer_webview *webview, int x, int y, int w, int h)
    {
        auto *handle = reinterpret_cast<saucer_handle *>(webview);
        handle->view.set_bounds({.x = x, .y = y, .w = w, .h = h});
    }

    void saucer_webview_set_file(saucer_handle *handle, const char *file)
    {
        const auto parsed = saucer::url::from(file ? file : "");
        if (parsed.has_value())
        {
            handle->view.set_url(parsed.value());
        }
    }

    void saucer_webview_set_url(saucer_handle *handle, const char *url)
    {
        handle->view.set_url(url ? url : "");
    }

    void saucer_webview_set_url_str(saucer_webview *webview, const char *url)
    {
        auto *handle = reinterpret_cast<saucer_handle *>(webview);
        saucer_webview_set_url(handle, url);
    }

    void saucer_webview_set_html(saucer_webview *webview, const char *html)
    {
        auto *handle = reinterpret_cast<saucer_handle *>(webview);
        handle->view.set_html(html ? html : "");
    }

    void saucer_webview_back(saucer_handle *handle)
    {
        handle->view.back();
    }

    void saucer_webview_forward(saucer_handle *handle)
    {
        handle->view.forward();
    }

    void saucer_webview_reload(saucer_handle *handle)
    {
        handle->view.reload();
    }

    void saucer_webview_embed_file(saucer_handle *handle, const char *name, saucer_embedded_file *file, SAUCER_LAUNCH)
    {
        handle->view.embed({{name ? name : "", file->value()}});
    }

    void saucer_webview_embed(saucer_webview *webview, const char *path, saucer_stash *content, const char *mime)
    {
        auto *handle = reinterpret_cast<saucer_handle *>(webview);
        handle->view.embed({{
            path ? path : "",
            saucer::embedded_file{
                .content = content ? content->value() : saucer::stash::empty(),
                .mime = mime ? mime : "application/octet-stream",
            },
        }});
    }

    void saucer_webview_serve(saucer_handle *handle, const char *file)
    {
        handle->view.serve(file ? file : "");
    }

    void saucer_webview_clear_scripts(saucer_handle *handle)
    {
        handle->view.uninject();
    }

    void saucer_webview_clear_embedded(saucer_handle *handle)
    {
        handle->view.unembed();
    }

    void saucer_webview_clear_embedded_file(saucer_handle *handle, const char *file)
    {
        handle->view.unembed(file ? file : "");
    }

    void saucer_webview_unembed_all(saucer_webview *webview)
    {
        auto *handle = reinterpret_cast<saucer_handle *>(webview);
        handle->view.unembed();
    }

    void saucer_webview_unembed(saucer_webview *webview, const char *path)
    {
        auto *handle = reinterpret_cast<saucer_handle *>(webview);
        handle->view.unembed(path ? path : "");
    }

    void saucer_webview_inject(saucer_handle *handle, saucer_script *script)
    {
        handle->view.inject(script->value());
    }

    void saucer_webview_execute(saucer_handle *handle, const char *code)
    {
        handle->execute(code ? code : "");
    }

    void saucer_webview_uninject_all(saucer_webview *webview)
    {
        auto *handle = reinterpret_cast<saucer_handle *>(webview);
        handle->view.uninject();
    }

    void saucer_webview_uninject(saucer_webview *webview, size_t id)
    {
        auto *handle = reinterpret_cast<saucer_handle *>(webview);
        handle->view.uninject(id);
    }

    void saucer_webview_handle_scheme(saucer_handle *handle, const char *name, saucer_scheme_handler handler, SAUCER_LAUNCH)
    {
        handle->view.handle_scheme(
            name ? name : "",
            [handle, handler](saucer::scheme::request req, saucer::scheme::executor exec)
            {
                auto *request = saucer_scheme_request::from(std::move(req));
                auto *executor = saucer_scheme_executor::from(std::move(exec));
                std::invoke(handler, handle, request, executor);
            });
    }

    void saucer_webview_remove_scheme(saucer_handle *handle, const char *name)
    {
        handle->view.remove_scheme(name ? name : "");
    }

    void saucer_webview_clear(saucer_handle *handle, SAUCER_WEB_EVENT event)
    {
        handle->view.off(static_cast<saucer::webview::event>(event));
    }

    void saucer_webview_remove(saucer_handle *handle, SAUCER_WEB_EVENT event, uint64_t id)
    {
        handle->view.off(static_cast<saucer::webview::event>(event), id);
    }

    void saucer_webview_off(saucer_webview *webview, SAUCER_WEB_EVENT event, size_t id)
    {
        auto *handle = reinterpret_cast<saucer_handle *>(webview);
        handle->view.off(static_cast<saucer::webview::event>(event), id);
    }

    void saucer_webview_off_all(saucer_webview *webview, SAUCER_WEB_EVENT event)
    {
        auto *handle = reinterpret_cast<saucer_handle *>(webview);
        handle->view.off(static_cast<saucer::webview::event>(event));
    }

    void saucer_webview_once(saucer_handle *handle, SAUCER_WEB_EVENT event, void *callback)
    {
        switch (event)
        {
        case SAUCER_WEB_EVENT_DOM_READY:
        {
            auto converted = reinterpret_cast<void (*)(saucer_handle *)>(callback);
            handle->view.once<saucer::webview::event::dom_ready>([handle, converted]() { converted(handle); });
            return;
        }
        case SAUCER_WEB_EVENT_NAVIGATED:
        {
            auto converted = reinterpret_cast<void (*)(saucer_handle *, const char *)>(callback);
            handle->view.once<saucer::webview::event::navigated>(
                [handle, converted](const saucer::url &url)
                {
                    const auto url_string = url.string();
                    converted(handle, url_string.c_str());
                });
            return;
        }
        case SAUCER_WEB_EVENT_NAVIGATE:
        {
            auto converted = reinterpret_cast<SAUCER_POLICY (*)(saucer_handle *, saucer_navigation *)>(callback);
            handle->view.once<saucer::webview::event::navigate>(
                [handle, converted](const saucer::navigation &navigation)
                {
                    auto *wrapped = make_navigation(navigation);
                    const auto policy = converted(handle, wrapped);
                    delete wrapped;
                    return static_cast<saucer::policy>(policy);
                });
            return;
        }
        case SAUCER_WEB_EVENT_FAVICON:
        {
            auto converted = reinterpret_cast<void (*)(saucer_handle *, saucer_icon *)>(callback);
            handle->view.once<saucer::webview::event::favicon>(
                [handle, converted](const saucer::icon &icon) { converted(handle, saucer_icon::from(saucer::icon(icon))); });
            return;
        }
        case SAUCER_WEB_EVENT_TITLE:
        {
            auto converted = reinterpret_cast<void (*)(saucer_handle *, const char *)>(callback);
            handle->view.once<saucer::webview::event::title>(
                [handle, converted](std::string_view title)
                {
                    const std::string copy(title);
                    converted(handle, copy.c_str());
                });
            return;
        }
        case SAUCER_WEB_EVENT_LOAD:
        {
            auto converted = reinterpret_cast<void (*)(saucer_handle *, SAUCER_STATE)>(callback);
            handle->view.once<saucer::webview::event::load>(
                [handle, converted](const saucer::state &state) { converted(handle, to_legacy_state(state)); });
            return;
        }
        }

        std::unreachable();
    }

    uint64_t saucer_webview_on(saucer_handle *handle, SAUCER_WEB_EVENT event, void *callback)
    {
        switch (event)
        {
        case SAUCER_WEB_EVENT_DOM_READY:
        {
            auto converted = reinterpret_cast<void (*)(saucer_handle *)>(callback);
            return handle->view.on<saucer::webview::event::dom_ready>([handle, converted]() { converted(handle); });
        }
        case SAUCER_WEB_EVENT_NAVIGATED:
        {
            auto converted = reinterpret_cast<void (*)(saucer_handle *, const char *)>(callback);
            return handle->view.on<saucer::webview::event::navigated>(
                [handle, converted](const saucer::url &url)
                {
                    const auto url_string = url.string();
                    converted(handle, url_string.c_str());
                });
        }
        case SAUCER_WEB_EVENT_NAVIGATE:
        {
            auto converted = reinterpret_cast<SAUCER_POLICY (*)(saucer_handle *, saucer_navigation *)>(callback);
            return handle->view.on<saucer::webview::event::navigate>(
                [handle, converted](const saucer::navigation &navigation)
                {
                    auto *wrapped = make_navigation(navigation);
                    const auto policy = converted(handle, wrapped);
                    delete wrapped;
                    return static_cast<saucer::policy>(policy);
                });
        }
        case SAUCER_WEB_EVENT_FAVICON:
        {
            auto converted = reinterpret_cast<void (*)(saucer_handle *, saucer_icon *)>(callback);
            return handle->view.on<saucer::webview::event::favicon>(
                [handle, converted](const saucer::icon &icon) { converted(handle, saucer_icon::from(saucer::icon(icon))); });
        }
        case SAUCER_WEB_EVENT_TITLE:
        {
            auto converted = reinterpret_cast<void (*)(saucer_handle *, const char *)>(callback);
            return handle->view.on<saucer::webview::event::title>(
                [handle, converted](std::string_view title)
                {
                    const std::string copy(title);
                    converted(handle, copy.c_str());
                });
        }
        case SAUCER_WEB_EVENT_LOAD:
        {
            auto converted = reinterpret_cast<void (*)(saucer_handle *, SAUCER_STATE)>(callback);
            return handle->view.on<saucer::webview::event::load>(
                [handle, converted](const saucer::state &state) { converted(handle, to_legacy_state(state)); });
        }
        }

        std::unreachable();
    }

    void saucer_register_scheme(const char *name)
    {
        saucer::webview::register_scheme(name ? name : "");
    }

    void saucer_webview_register_scheme(const char *name)
    {
        saucer_register_scheme(name);
    }

    void saucer_webview_native(saucer_webview *webview, size_t idx, void *result, size_t *size)
    {
        auto *handle = reinterpret_cast<saucer_handle *>(webview);
        if (!handle || idx != 0)
        {
            if (size)
            {
                *size = 0;
            }
            return;
        }

        if (!result)
        {
            if (size)
            {
                *size = sizeof(void *);
            }
            return;
        }

        if (!size || *size < sizeof(void *))
        {
            return;
        }

        *reinterpret_cast<void **>(result) = reinterpret_cast<void *>(handle->view.native<false>());
    }
}
