#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include "export.h"

#include "options.h"
#include "window.h"

#include <stddef.h>
#include <stdbool.h>

    /**
     * @brief A handle to a saucer::application
     * @note The application will live as long as there are handles to it. So make sure to properly free all handles you
     * obtain to a saucer::application like through e.g. `saucer_application_active`!
     */
    struct saucer_application;
    struct saucer_screen;
    typedef saucer_options saucer_application_options;

    enum saucer_application_event
    {
        SAUCER_APPLICATION_EVENT_QUIT,
    };

    SAUCER_EXPORT saucer_application *saucer_application_init(saucer_options *options);
    SAUCER_EXPORT saucer_application *saucer_application_new(saucer_application_options *options, int *error);
    SAUCER_EXPORT void saucer_application_free(saucer_application *);

    SAUCER_EXPORT saucer_application *saucer_application_active();

    SAUCER_EXPORT bool saucer_application_thread_safe(saucer_application *);
    SAUCER_EXPORT void saucer_application_screens(saucer_application *, saucer_screen **, size_t *size);

    SAUCER_EXPORT void saucer_screen_free(saucer_screen *);
    SAUCER_EXPORT const char *saucer_screen_name(saucer_screen *);
    SAUCER_EXPORT void saucer_screen_size(saucer_screen *, int *w, int *h);
    SAUCER_EXPORT void saucer_screen_position(saucer_screen *, int *x, int *y);

    typedef void (*saucer_pool_callback)();

    /**
     * @brief Submits (blocking) the given @param callback to the thread-pool
     */
    SAUCER_EXPORT void saucer_application_pool_submit(saucer_application *, saucer_pool_callback callback);

    /**
     * @brief Emplaces (non blocking) the given @param callback to the thread-pool
     */
    SAUCER_EXPORT void saucer_application_pool_emplace(saucer_application *, saucer_pool_callback callback);

    typedef void (*saucer_post_callback)();
    SAUCER_EXPORT void saucer_application_post(saucer_application *, saucer_post_callback callback);

    SAUCER_EXPORT void saucer_application_quit(saucer_application *);

    SAUCER_EXPORT void saucer_application_run(saucer_application *);
    SAUCER_EXPORT void saucer_application_run_once(saucer_application *);

    SAUCER_EXPORT saucer_application_options *saucer_application_options_new(const char *id);
    SAUCER_EXPORT void saucer_application_options_free(saucer_application_options *);
    SAUCER_EXPORT void saucer_application_options_set_argc(saucer_application_options *, int argc);
    SAUCER_EXPORT void saucer_application_options_set_argv(saucer_application_options *, char **argv);
    SAUCER_EXPORT void saucer_application_options_set_quit_on_last_window_closed(saucer_application_options *, bool value);

    typedef SAUCER_POLICY (*saucer_application_event_quit)(saucer_application *, void *);

    SAUCER_EXPORT size_t saucer_application_on(saucer_application *, saucer_application_event, void *callback,
                                               bool clearable, void *userdata);
    SAUCER_EXPORT void saucer_application_once(saucer_application *, saucer_application_event, void *callback,
                                               void *userdata);
    SAUCER_EXPORT void saucer_application_off(saucer_application *, saucer_application_event, size_t id);
    SAUCER_EXPORT void saucer_application_off_all(saucer_application *, saucer_application_event);

    SAUCER_EXPORT void saucer_application_native(saucer_application *, size_t idx, void *result, size_t *size);
    SAUCER_EXPORT const char *saucer_version();

#ifdef __cplusplus
}
#endif
