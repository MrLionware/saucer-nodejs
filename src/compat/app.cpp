#include "app.h"
#include "app.hpp"

#include "options.hpp"
#include "utils/handle.hpp"

#include <algorithm>
#include <future>
#include <thread>
#include <mutex>

namespace
{
    std::mutex g_active_mutex;
    std::weak_ptr<saucer_application_state> g_active_state;

    saucer_application *make_application(saucer::application &&app_value)
    {
        auto state = std::make_shared<saucer_application_state>();
        state->app = std::make_shared<saucer::application>(std::move(app_value));
        state->loop = std::make_shared<saucer::modules::loop>(*state->app);

        {
            std::scoped_lock lock(g_active_mutex);
            g_active_state = state;
        }

        return saucer_application::from(std::move(state));
    }
}

extern "C"
{
    saucer_application *saucer_application_init(saucer_options *options)
    {
        if (!options)
        {
            return nullptr;
        }

        saucer::application::options create_opts{.id = options->value().id};

        if (options->value().argc.has_value())
        {
            create_opts.argc = options->value().argc.value();
        }

        if (options->value().argv.has_value())
        {
            create_opts.argv = options->value().argv.value();
        }

        auto app_result = saucer::application::create(create_opts);
        if (!app_result.has_value())
        {
            return nullptr;
        }

        return make_application(std::move(app_result).value());
    }

    saucer_application *saucer_application_new(saucer_application_options *options, int *error)
    {
        if (!options)
        {
            if (error)
            {
                *error = -1;
            }

            return nullptr;
        }

        auto *app = saucer_application_init(reinterpret_cast<saucer_options *>(options));
        if (!app && error)
        {
            *error = -1;
        }

        return app;
    }

    void saucer_application_free(saucer_application *handle)
    {
        delete handle;
    }

    saucer_application *saucer_application_active()
    {
        std::shared_ptr<saucer_application_state> state;
        {
            std::scoped_lock lock(g_active_mutex);
            state = g_active_state.lock();
        }

        if (!state)
        {
            return nullptr;
        }

        return saucer_application::from(std::move(state));
    }

    bool saucer_application_thread_safe(saucer_application *handle)
    {
        return handle && handle->value() && handle->value()->app && handle->value()->app->thread_safe();
    }

    void saucer_application_screens(saucer_application *handle, saucer_screen **screens, size_t *size)
    {
        if (!handle || !handle->value() || !handle->value()->app || !size)
        {
            if (size)
            {
                *size = 0;
            }

            return;
        }

        auto values = handle->value()->app->screens();

        if (!screens)
        {
            *size = values.size();
            return;
        }

        const auto count = std::min(*size, values.size());
        for (auto i = 0u; i < count; ++i)
        {
            screens[i] = saucer_screen::from(std::move(values[i]));
        }

        *size = count;
    }

    void saucer_screen_free(saucer_screen *screen)
    {
        delete screen;
    }

    const char *saucer_screen_name(saucer_screen *screen)
    {
        return screen ? screen->value().name.c_str() : "";
    }

    void saucer_screen_size(saucer_screen *screen, int *w, int *h)
    {
        if (!screen || !w || !h)
        {
            return;
        }

        *w = screen->value().size.w;
        *h = screen->value().size.h;
    }

    void saucer_screen_position(saucer_screen *screen, int *x, int *y)
    {
        if (!screen || !x || !y)
        {
            return;
        }

        *x = screen->value().position.x;
        *y = screen->value().position.y;
    }

    void saucer_application_pool_submit(saucer_application *, saucer_pool_callback callback)
    {
        if (!callback)
        {
            return;
        }

        std::async(std::launch::async, callback).wait();
    }

    void saucer_application_pool_emplace(saucer_application *, saucer_pool_callback callback)
    {
        if (!callback)
        {
            return;
        }

        std::thread(callback).detach();
    }

    void saucer_application_post(saucer_application *handle, saucer_post_callback callback)
    {
        if (!handle || !handle->value() || !handle->value()->app || !callback)
        {
            return;
        }

        handle->value()->app->post([callback] { callback(); });
    }

    void saucer_application_quit(saucer_application *handle)
    {
        if (handle && handle->value() && handle->value()->app)
        {
            handle->value()->app->quit();
        }
    }

    void saucer_application_run(saucer_application *handle)
    {
        if (handle && handle->value() && handle->value()->loop)
        {
            handle->value()->loop->run();
        }
    }

    void saucer_application_run_once(saucer_application *handle)
    {
        if (handle && handle->value() && handle->value()->loop)
        {
            handle->value()->loop->iteration();
        }
    }

    saucer_application_options *saucer_application_options_new(const char *id)
    {
        return reinterpret_cast<saucer_application_options *>(saucer_options_new(id));
    }

    void saucer_application_options_free(saucer_application_options *options)
    {
        saucer_options_free(reinterpret_cast<saucer_options *>(options));
    }

    void saucer_application_options_set_argc(saucer_application_options *options, int argc)
    {
        saucer_options_set_argc(reinterpret_cast<saucer_options *>(options), argc);
    }

    void saucer_application_options_set_argv(saucer_application_options *options, char **argv)
    {
        saucer_options_set_argv(reinterpret_cast<saucer_options *>(options), argv);
    }

    void saucer_application_options_set_quit_on_last_window_closed(saucer_application_options *, bool)
    {
        // Legacy options do not expose this toggle.
    }

    size_t saucer_application_on(saucer_application *handle, saucer_application_event event, void *callback,
                                 bool clearable, void *userdata)
    {
        if (!handle || !handle->value() || !handle->value()->app || !callback)
        {
            return 0;
        }

        switch (event)
        {
        case SAUCER_APPLICATION_EVENT_QUIT:
        {
            auto converted = reinterpret_cast<saucer_application_event_quit>(callback);
            return handle->value()->app->on<saucer::application::event::quit>({{
                .func = [handle, converted, userdata]()
                {
                    return static_cast<saucer::policy>(converted(handle, userdata));
                },
                .clearable = clearable,
            }});
        }
        }

        return 0;
    }

    void saucer_application_once(saucer_application *handle, saucer_application_event event, void *callback,
                                 void *userdata)
    {
        if (!handle || !handle->value() || !handle->value()->app || !callback)
        {
            return;
        }

        switch (event)
        {
        case SAUCER_APPLICATION_EVENT_QUIT:
        {
            auto converted = reinterpret_cast<saucer_application_event_quit>(callback);
            handle->value()->app->once<saucer::application::event::quit>(
                [handle, converted, userdata]()
                {
                    return static_cast<saucer::policy>(converted(handle, userdata));
                });
            return;
        }
        }
    }

    void saucer_application_off(saucer_application *handle, saucer_application_event event, size_t id)
    {
        if (!handle || !handle->value() || !handle->value()->app)
        {
            return;
        }

        handle->value()->app->off(static_cast<saucer::application::event>(event), id);
    }

    void saucer_application_off_all(saucer_application *handle, saucer_application_event event)
    {
        if (!handle || !handle->value() || !handle->value()->app)
        {
            return;
        }

        handle->value()->app->off(static_cast<saucer::application::event>(event));
    }

    void saucer_application_native(saucer_application *handle, size_t idx, void *result, size_t *size)
    {
        if (!handle || !handle->value() || !handle->value()->app || idx != 0)
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

        *reinterpret_cast<void **>(result) = reinterpret_cast<void *>(handle->value()->app->native<false>());
    }

    const char *saucer_version()
    {
        return "8.0.0";
    }
}
