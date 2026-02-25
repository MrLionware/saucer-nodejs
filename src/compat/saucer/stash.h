#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include "export.h"

#include <stdint.h>
#include <stddef.h>

    struct saucer_stash;

    SAUCER_EXPORT void saucer_stash_free(saucer_stash *);

    SAUCER_EXPORT size_t saucer_stash_size(saucer_stash *);
    SAUCER_EXPORT const uint8_t *saucer_stash_data(saucer_stash *);

    SAUCER_EXPORT saucer_stash *saucer_stash_from(const uint8_t *data, size_t size);
    SAUCER_EXPORT saucer_stash *saucer_stash_view(const uint8_t *data, size_t size);

    SAUCER_EXPORT saucer_stash *saucer_stash_copy(saucer_stash *);
    SAUCER_EXPORT saucer_stash *saucer_stash_new_from(uint8_t *data, size_t size);
    SAUCER_EXPORT saucer_stash *saucer_stash_new_view(const uint8_t *data, size_t size);

    typedef saucer_stash *(*saucer_stash_lazy_callback)();

    /**
     * @note The stash returned from within the @param callback is automatically deleted. However, the stash returned
     * from this function must still be free'd accordingly.
     */
    SAUCER_EXPORT saucer_stash *saucer_stash_lazy(saucer_stash_lazy_callback callback);

    typedef saucer_stash *(*saucer_stash_lazy_callback_v8)(void *userdata);
    SAUCER_EXPORT saucer_stash *saucer_stash_new_lazy(saucer_stash_lazy_callback_v8 callback, void *userdata);

    SAUCER_EXPORT saucer_stash *saucer_stash_new_from_str(const char *str);
    SAUCER_EXPORT saucer_stash *saucer_stash_new_view_str(const char *str);
    SAUCER_EXPORT saucer_stash *saucer_stash_new_empty();

#ifdef __cplusplus
}
#endif
