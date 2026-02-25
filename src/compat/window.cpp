#include "window.h"

#include "app.hpp"
#include "icon.hpp"
#include "webview.hpp"

#include "utils/string.hpp"

extern "C"
{
    saucer_window *saucer_window_new(saucer_application *application, int *error)
    {
        if (!application || !application->value() || !application->value()->app)
        {
            if (error)
            {
                *error = -1;
            }

            return nullptr;
        }

        auto window_result = saucer::window::create(application->value()->app.get());
        if (!window_result.has_value())
        {
            if (error)
            {
                *error = window_result.error().code();
            }

            return nullptr;
        }

        saucer::smartview::options options{
            .window = window_result.value(),
        };

        auto view_result = saucer::smartview::create(options);
        if (!view_result.has_value())
        {
            if (error)
            {
                *error = view_result.error().code();
            }

            return nullptr;
        }

        auto *handle = new saucer_handle{.view = std::move(view_result).value()};
        handle->window = window_result.value();
        return reinterpret_cast<saucer_window *>(handle);
    }

    void saucer_window_free(saucer_window *window)
    {
        delete reinterpret_cast<saucer_handle *>(window);
    }

    bool saucer_window_visible(saucer_handle *handle)
    {
        return handle->view.parent().visible();
    }

    bool saucer_window_focused(saucer_handle *handle)
    {
        return handle->view.parent().focused();
    }

    bool saucer_window_minimized(saucer_handle *handle)
    {
        return handle->view.parent().minimized();
    }

    bool saucer_window_maximized(saucer_handle *handle)
    {
        return handle->view.parent().maximized();
    }

    bool saucer_window_resizable(saucer_handle *handle)
    {
        return handle->view.parent().resizable();
    }

    bool saucer_window_fullscreen(saucer_window *window)
    {
        auto *handle = reinterpret_cast<saucer_handle *>(window);
        return handle->view.parent().fullscreen();
    }

    bool saucer_window_decorations(saucer_handle *handle)
    {
        return handle->view.parent().decorations() != saucer::window::decoration::none;
    }

    bool saucer_window_always_on_top(saucer_handle *handle)
    {
        return handle->view.parent().always_on_top();
    }

    bool saucer_window_click_through(saucer_handle *handle)
    {
        return handle->view.parent().click_through();
    }

    char *saucer_window_title(saucer_handle *handle)
    {
        return bindings::alloc(handle->view.parent().title());
    }

    void saucer_window_size(saucer_handle *handle, int *width, int *height)
    {
        const auto size = handle->view.parent().size();
        *width = size.w;
        *height = size.h;
    }

    void saucer_window_max_size(saucer_handle *handle, int *width, int *height)
    {
        const auto size = handle->view.parent().max_size();
        *width = size.w;
        *height = size.h;
    }

    void saucer_window_min_size(saucer_handle *handle, int *width, int *height)
    {
        const auto size = handle->view.parent().min_size();
        *width = size.w;
        *height = size.h;
    }

    void saucer_window_background(saucer_window *window, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a)
    {
        auto *handle = reinterpret_cast<saucer_handle *>(window);
        const auto color = handle->view.parent().background();
        *r = color.r;
        *g = color.g;
        *b = color.b;
        *a = color.a;
    }

    void saucer_window_position(saucer_window *window, int *x, int *y)
    {
        auto *handle = reinterpret_cast<saucer_handle *>(window);
        const auto position = handle->view.parent().position();
        *x = position.x;
        *y = position.y;
    }

    saucer_screen *saucer_window_screen(saucer_window *window)
    {
        auto *handle = reinterpret_cast<saucer_handle *>(window);
        const auto screen = handle->view.parent().screen();
        if (!screen.has_value())
        {
            return nullptr;
        }

        return saucer_screen::make(*screen);
    }

    void saucer_window_hide(saucer_handle *handle)
    {
        handle->view.parent().hide();
    }

    void saucer_window_show(saucer_handle *handle)
    {
        handle->view.parent().show();
    }

    void saucer_window_close(saucer_handle *handle)
    {
        handle->view.parent().close();
    }

    void saucer_window_focus(saucer_handle *handle)
    {
        handle->view.parent().focus();
    }

    void saucer_window_start_drag(saucer_handle *handle)
    {
        handle->view.parent().start_drag();
    }

    void saucer_window_start_resize(saucer_handle *handle, SAUCER_WINDOW_EDGE edge)
    {
        handle->view.parent().start_resize(static_cast<saucer::window::edge>(edge));
    }

    void saucer_window_set_minimized(saucer_handle *handle, bool enabled)
    {
        handle->view.parent().set_minimized(enabled);
    }

    void saucer_window_set_maximized(saucer_handle *handle, bool enabled)
    {
        handle->view.parent().set_maximized(enabled);
    }

    void saucer_window_set_resizable(saucer_handle *handle, bool enabled)
    {
        handle->view.parent().set_resizable(enabled);
    }

    void saucer_window_set_decorations(saucer_handle *handle, bool enabled)
    {
        handle->view.parent().set_decorations(enabled ? saucer::window::decoration::full : saucer::window::decoration::none);
    }

    void saucer_window_set_fullscreen(saucer_window *window, bool enabled)
    {
        auto *handle = reinterpret_cast<saucer_handle *>(window);
        handle->view.parent().set_fullscreen(enabled);
    }

    void saucer_window_set_always_on_top(saucer_handle *handle, bool enabled)
    {
        handle->view.parent().set_always_on_top(enabled);
    }

    void saucer_window_set_click_through(saucer_handle *handle, bool enabled)
    {
        handle->view.parent().set_click_through(enabled);
    }

    void saucer_window_set_icon(saucer_handle *handle, saucer_icon *icon)
    {
        handle->view.parent().set_icon(icon->value());
    }

    void saucer_window_set_title(saucer_handle *handle, const char *title)
    {
        handle->view.parent().set_title(title ? title : "");
    }

    void saucer_window_set_size(saucer_handle *handle, int width, int height)
    {
        handle->view.parent().set_size({.w = width, .h = height});
    }

    void saucer_window_set_max_size(saucer_handle *handle, int width, int height)
    {
        handle->view.parent().set_max_size({.w = width, .h = height});
    }

    void saucer_window_set_min_size(saucer_handle *handle, int width, int height)
    {
        handle->view.parent().set_min_size({.w = width, .h = height});
    }

    void saucer_window_set_background(saucer_window *window, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    {
        auto *handle = reinterpret_cast<saucer_handle *>(window);
        handle->view.parent().set_background({.r = r, .g = g, .b = b, .a = a});
    }

    void saucer_window_set_position(saucer_window *window, int x, int y)
    {
        auto *handle = reinterpret_cast<saucer_handle *>(window);
        handle->view.parent().set_position({.x = x, .y = y});
    }

    void saucer_window_clear(saucer_handle *handle, SAUCER_WINDOW_EVENT event)
    {
        handle->view.parent().off(static_cast<saucer::window::event>(event));
    }

    void saucer_window_remove(saucer_handle *handle, SAUCER_WINDOW_EVENT event, uint64_t id)
    {
        handle->view.parent().off(static_cast<saucer::window::event>(event), id);
    }

    void saucer_window_off(saucer_window *window, SAUCER_WINDOW_EVENT event, size_t id)
    {
        auto *handle = reinterpret_cast<saucer_handle *>(window);
        handle->view.parent().off(static_cast<saucer::window::event>(event), id);
    }

    void saucer_window_off_all(saucer_window *window, SAUCER_WINDOW_EVENT event)
    {
        auto *handle = reinterpret_cast<saucer_handle *>(window);
        handle->view.parent().off(static_cast<saucer::window::event>(event));
    }

    void saucer_window_once(saucer_handle *handle, SAUCER_WINDOW_EVENT event, void *callback)
    {
        switch (event)
        {
        case SAUCER_WINDOW_EVENT_DECORATED:
        {
            auto converted = reinterpret_cast<void (*)(saucer_handle *, bool)>(callback);
            handle->view.parent().once<saucer::window::event::decorated>(
                [handle, converted](saucer::window::decoration decoration) { converted(handle, decoration != saucer::window::decoration::none); });
            return;
        }
        case SAUCER_WINDOW_EVENT_MAXIMIZE:
        {
            auto converted = reinterpret_cast<void (*)(saucer_handle *, bool)>(callback);
            handle->view.parent().once<saucer::window::event::maximize>(
                [handle, converted](bool value) { converted(handle, value); });
            return;
        }
        case SAUCER_WINDOW_EVENT_MINIMIZE:
        {
            auto converted = reinterpret_cast<void (*)(saucer_handle *, bool)>(callback);
            handle->view.parent().once<saucer::window::event::minimize>(
                [handle, converted](bool value) { converted(handle, value); });
            return;
        }
        case SAUCER_WINDOW_EVENT_CLOSED:
        {
            auto converted = reinterpret_cast<void (*)(saucer_handle *)>(callback);
            handle->view.parent().once<saucer::window::event::closed>(
                [handle, converted]() { converted(handle); });
            return;
        }
        case SAUCER_WINDOW_EVENT_RESIZE:
        {
            auto converted = reinterpret_cast<void (*)(saucer_handle *, int, int)>(callback);
            handle->view.parent().once<saucer::window::event::resize>(
                [handle, converted](int w, int h) { converted(handle, w, h); });
            return;
        }
        case SAUCER_WINDOW_EVENT_FOCUS:
        {
            auto converted = reinterpret_cast<void (*)(saucer_handle *, bool)>(callback);
            handle->view.parent().once<saucer::window::event::focus>(
                [handle, converted](bool value) { converted(handle, value); });
            return;
        }
        case SAUCER_WINDOW_EVENT_CLOSE:
        {
            auto converted = reinterpret_cast<SAUCER_POLICY (*)(saucer_handle *)>(callback);
            handle->view.parent().once<saucer::window::event::close>(
                [handle, converted]() { return static_cast<saucer::policy>(converted(handle)); });
            return;
        }
        }

        std::unreachable();
    }

    uint64_t saucer_window_on(saucer_handle *handle, SAUCER_WINDOW_EVENT event, void *callback)
    {
        switch (event)
        {
        case SAUCER_WINDOW_EVENT_DECORATED:
        {
            auto converted = reinterpret_cast<void (*)(saucer_handle *, bool)>(callback);
            return handle->view.parent().on<saucer::window::event::decorated>(
                [handle, converted](saucer::window::decoration decoration) { converted(handle, decoration != saucer::window::decoration::none); });
        }
        case SAUCER_WINDOW_EVENT_MAXIMIZE:
        {
            auto converted = reinterpret_cast<void (*)(saucer_handle *, bool)>(callback);
            return handle->view.parent().on<saucer::window::event::maximize>(
                [handle, converted](bool value) { converted(handle, value); });
        }
        case SAUCER_WINDOW_EVENT_MINIMIZE:
        {
            auto converted = reinterpret_cast<void (*)(saucer_handle *, bool)>(callback);
            return handle->view.parent().on<saucer::window::event::minimize>(
                [handle, converted](bool value) { converted(handle, value); });
        }
        case SAUCER_WINDOW_EVENT_CLOSED:
        {
            auto converted = reinterpret_cast<void (*)(saucer_handle *)>(callback);
            return handle->view.parent().on<saucer::window::event::closed>(
                [handle, converted]() { converted(handle); });
        }
        case SAUCER_WINDOW_EVENT_RESIZE:
        {
            auto converted = reinterpret_cast<void (*)(saucer_handle *, int, int)>(callback);
            return handle->view.parent().on<saucer::window::event::resize>(
                [handle, converted](int w, int h) { converted(handle, w, h); });
        }
        case SAUCER_WINDOW_EVENT_FOCUS:
        {
            auto converted = reinterpret_cast<void (*)(saucer_handle *, bool)>(callback);
            return handle->view.parent().on<saucer::window::event::focus>(
                [handle, converted](bool value) { converted(handle, value); });
        }
        case SAUCER_WINDOW_EVENT_CLOSE:
        {
            auto converted = reinterpret_cast<SAUCER_POLICY (*)(saucer_handle *)>(callback);
            return handle->view.parent().on<saucer::window::event::close>(
                [handle, converted]() { return static_cast<saucer::policy>(converted(handle)); });
        }
        }

        std::unreachable();
    }

    void saucer_window_native(saucer_window *window, size_t idx, void *result, size_t *size)
    {
        auto *handle = reinterpret_cast<saucer_handle *>(window);
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

        *reinterpret_cast<void **>(result) = reinterpret_cast<void *>(handle->view.parent().native<false>());
    }
}
