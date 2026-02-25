#include "stash.h"
#include "stash.hpp"

#include <span>
#include <vector>

extern "C"
{
    void saucer_stash_free(saucer_stash *handle)
    {
        delete handle;
    }

    size_t saucer_stash_size(saucer_stash *handle)
    {
        return handle->value().size();
    }

    const uint8_t *saucer_stash_data(saucer_stash *handle)
    {
        return handle->value().data();
    }

    saucer_stash *saucer_stash_from(const uint8_t *data, size_t size)
    {
        if (!data || size == 0)
        {
            return saucer_stash::from(saucer::stash::empty());
        }

        std::vector<uint8_t> owning(data, data + size);
        return saucer_stash::from(saucer::stash::from(std::move(owning)));
    }

    saucer_stash *saucer_stash_view(const uint8_t *data, size_t size)
    {
        if (!data || size == 0)
        {
            return saucer_stash::from(saucer::stash::empty());
        }

        return saucer_stash::from(saucer::stash::view(std::span<const uint8_t>{data, size}));
    }

    saucer_stash *saucer_stash_lazy(saucer_stash_lazy_callback callback)
    {
        return saucer_stash::from(saucer::stash::lazy(
            [callback]()
            {
                auto *handle = std::invoke(callback);
                auto rtn = std::move(handle->value());
                delete handle;
                return rtn;
            }));
    }

    saucer_stash *saucer_stash_copy(saucer_stash *handle)
    {
        if (!handle)
        {
            return nullptr;
        }

        return saucer_stash::make(handle->value());
    }

    saucer_stash *saucer_stash_new_from(uint8_t *data, size_t size)
    {
        return saucer_stash_from(data, size);
    }

    saucer_stash *saucer_stash_new_view(const uint8_t *data, size_t size)
    {
        return saucer_stash_view(data, size);
    }

    saucer_stash *saucer_stash_new_lazy(saucer_stash_lazy_callback_v8 callback, void *userdata)
    {
        return saucer_stash::from(saucer::stash::lazy(
            [callback, userdata]()
            {
                auto *handle = callback(userdata);
                auto rtn = std::move(handle->value());
                delete handle;
                return rtn;
            }));
    }

    saucer_stash *saucer_stash_new_from_str(const char *str)
    {
        return saucer_stash::from(saucer::stash::from_str(str ? str : ""));
    }

    saucer_stash *saucer_stash_new_view_str(const char *str)
    {
        return saucer_stash::from(saucer::stash::view_str(str ? str : ""));
    }

    saucer_stash *saucer_stash_new_empty()
    {
        return saucer_stash::from(saucer::stash::empty());
    }
}
