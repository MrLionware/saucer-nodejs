#include "icon.h"
#include "icon.hpp"

#include "stash.hpp"

extern "C"
{
    void saucer_icon_free(saucer_icon *handle)
    {
        delete handle;
    }

    bool saucer_icon_empty(saucer_icon *handle)
    {
        return handle->value().empty();
    }

    saucer_stash *saucer_icon_data(saucer_icon *handle)
    {
        return saucer_stash::from(handle->value().data());
    }

    void saucer_icon_save(saucer_icon *handle, const char *path)
    {
        handle->value().save(path);
    }

    saucer_icon *saucer_icon_copy(saucer_icon *handle)
    {
        if (!handle)
        {
            return nullptr;
        }

        return saucer_icon::make(handle->value());
    }

    void saucer_icon_from_file(saucer_icon **result, const char *file)
    {
        auto icon = saucer::icon::from(file);

        if (!icon)
        {
            return;
        }

        *result = saucer_icon::from(std::move(icon.value()));
    }

    void saucer_icon_from_data(saucer_icon **result, saucer_stash *stash)
    {
        auto icon = saucer::icon::from(stash->value());

        if (!icon)
        {
            return;
        }

        *result = saucer_icon::from(std::move(icon.value()));
    }

    saucer_icon *saucer_icon_new_from_file(const char *file, int *error)
    {
        auto icon = saucer::icon::from(file ? file : "");
        if (!icon.has_value())
        {
            if (error)
            {
                *error = icon.error().code();
            }

            return nullptr;
        }

        return saucer_icon::from(std::move(icon.value()));
    }

    saucer_icon *saucer_icon_new_from_stash(saucer_stash *stash, int *error)
    {
        if (!stash)
        {
            if (error)
            {
                *error = -1;
            }

            return nullptr;
        }

        auto icon = saucer::icon::from(stash->value());
        if (!icon.has_value())
        {
            if (error)
            {
                *error = icon.error().code();
            }

            return nullptr;
        }

        return saucer_icon::from(std::move(icon.value()));
    }

    void saucer_icon_native(saucer_icon *icon, size_t idx, void *result, size_t *size)
    {
        if (!icon || idx != 0)
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

        *reinterpret_cast<void **>(result) = reinterpret_cast<void *>(icon->value().native<false>());
    }
}
