#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include "export.h"

#include <saucer/app.h>
#include <stddef.h>

    struct saucer_desktop;

    SAUCER_DESKTOP_EXPORT saucer_desktop *saucer_desktop_new(saucer_application *app);
    SAUCER_DESKTOP_EXPORT void saucer_desktop_free(saucer_desktop *);

    SAUCER_DESKTOP_EXPORT void saucer_desktop_open(saucer_desktop *, const char *path);

    struct saucer_picker_options;

    SAUCER_DESKTOP_EXPORT saucer_picker_options *saucer_picker_options_new();
    SAUCER_DESKTOP_EXPORT void saucer_picker_options_free(saucer_picker_options *);

    SAUCER_DESKTOP_EXPORT void saucer_picker_options_set_initial(saucer_picker_options *, const char *path);
    SAUCER_DESKTOP_EXPORT void saucer_picker_options_add_filter(saucer_picker_options *, const char *filter);
    SAUCER_DESKTOP_EXPORT void saucer_picker_options_set_filters(saucer_picker_options *, const char *filters, size_t size);

    /**
     * @note The returned array will be populated with strings which are themselves dynamically allocated.
     *
     * To properly free the returned array you should:
     * - Free all strings within the array
     * - Free the array itself
     */
    /*[[sc::requires_free]]*/ SAUCER_DESKTOP_EXPORT char *saucer_desktop_pick_file(saucer_desktop *,
                                                                                   saucer_picker_options *options);

    /*[[sc::requires_free]]*/ SAUCER_DESKTOP_EXPORT char *saucer_desktop_pick_folder(saucer_desktop *,
                                                                                     saucer_picker_options *options);

    /*[[sc::requires_free]]*/ SAUCER_DESKTOP_EXPORT char **saucer_desktop_pick_files(saucer_desktop *,
                                                                                     saucer_picker_options *options);

    /*[[sc::requires_free]]*/ SAUCER_DESKTOP_EXPORT char **saucer_desktop_pick_folders(saucer_desktop *,
                                                                                       saucer_picker_options *options);

    SAUCER_DESKTOP_EXPORT void saucer_desktop_mouse_position(saucer_desktop *, int *x, int *y);
    SAUCER_DESKTOP_EXPORT void saucer_picker_pick_file(saucer_desktop *, saucer_picker_options *, char *, size_t *,
                                                       int *error);
    SAUCER_DESKTOP_EXPORT void saucer_picker_pick_folder(saucer_desktop *, saucer_picker_options *, char *, size_t *,
                                                         int *error);
    SAUCER_DESKTOP_EXPORT void saucer_picker_pick_files(saucer_desktop *, saucer_picker_options *, char *, size_t *,
                                                        int *error);
    SAUCER_DESKTOP_EXPORT void saucer_picker_save(saucer_desktop *, saucer_picker_options *, char *, size_t *,
                                                  int *error);

#ifdef __cplusplus
}
#endif
