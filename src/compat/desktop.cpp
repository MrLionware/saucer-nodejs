#include "desktop.h"

#include "app.hpp"

#include "utils/string.hpp"
#include "utils/handle.hpp"

#include <saucer/memory.h>
#include <saucer/modules/desktop.hpp>

#include <algorithm>
#include <cstring>
#include <set>
#include <string>
#include <vector>

struct saucer_desktop : bindings::handle<saucer_desktop, saucer::modules::desktop>
{
};

struct saucer_picker_options : bindings::handle<saucer_picker_options, saucer::modules::picker::options>
{
};

using saucer::modules::picker::type;

namespace
{
    template <typename Range>
    void write_range(const Range &value, char *result, size_t *size)
    {
        if (!size)
        {
            return;
        }

        if (!result)
        {
            *size = value.size();
            return;
        }

        const auto count = std::min(*size, value.size());
        for (auto i = 0u; i < count; ++i)
        {
            result[i] = value[i];
        }

        *size = count;
    }
}

extern "C"
{
    saucer_desktop *saucer_desktop_new(saucer_application *app)
    {
        return saucer_desktop::make(app->value()->app.get());
    }

    void saucer_desktop_free(saucer_desktop *handle)
    {
        delete handle;
    }

    void saucer_desktop_open(saucer_desktop *handle, const char *path)
    {
        handle->value().open(path);
    }

    saucer_picker_options *saucer_picker_options_new()
    {
        return saucer_picker_options::make();
    }

    void saucer_picker_options_free(saucer_picker_options *handle)
    {
        delete handle;
    }

    void saucer_picker_options_set_initial(saucer_picker_options *handle, const char *path)
    {
        handle->value().initial = path;
    }

    void saucer_picker_options_add_filter(saucer_picker_options *handle, const char *filter)
    {
        handle->value().filters.emplace(filter);
    }

    void saucer_picker_options_set_filters(saucer_picker_options *handle, const char *filters, size_t size)
    {
        if (!handle)
        {
            return;
        }

        auto parsed = std::set<std::string>{};
        if (filters)
        {
            for (const char *current = filters; current - filters < static_cast<std::ptrdiff_t>(size);)
            {
                parsed.emplace(current);
                current += std::char_traits<char>::length(current) + 1;
            }
        }

        handle->value().filters = std::move(parsed);
    }

    char *saucer_desktop_pick_file(saucer_desktop *handle, saucer_picker_options *options)
    {
        auto result = handle->value().pick<type::file>(options->value());

        if (!result)
        {
            return nullptr;
        }

        return bindings::alloc(result->string());
    }

    char *saucer_desktop_pick_folder(saucer_desktop *handle, saucer_picker_options *options)
    {
        auto result = handle->value().pick<type::folder>(options->value());

        if (!result)
        {
            return nullptr;
        }

        return bindings::alloc(result->string());
    }

    char **saucer_desktop_pick_files(saucer_desktop *handle, saucer_picker_options *options)
    {
        auto result = handle->value().pick<type::files>(options->value());

        if (!result)
        {
            return nullptr;
        }

        const auto count = result->size();
        auto **rtn       = static_cast<char **>(saucer_memory_alloc(count * sizeof(char *)));

        for (auto i = 0u; result->size() > i; i++)
        {
            rtn[i] = bindings::alloc(result->at(i).string());
        }

        return rtn;
    }

    char **saucer_desktop_pick_folders(saucer_desktop *handle, saucer_picker_options *options)
    {
        auto result = handle->value().pick<type::files>(options->value());

        if (!result)
        {
            return nullptr;
        }

        const auto count = result->size();
        auto **rtn       = static_cast<char **>(saucer_memory_alloc(count * sizeof(char *)));

        for (auto i = 0u; result->size() > i; i++)
        {
            rtn[i] = bindings::alloc(result->at(i).string());
        }

        return rtn;
    }

    void saucer_desktop_mouse_position(saucer_desktop *handle, int *x, int *y)
    {
        if (!handle || !x || !y)
        {
            return;
        }

        const auto pos = handle->value().mouse_position();
        *x = pos.x;
        *y = pos.y;
    }

    void saucer_picker_pick_file(saucer_desktop *handle, saucer_picker_options *options, char *file, size_t *size, int *error)
    {
        auto result = handle->value().pick<type::file>(options ? options->value() : saucer::modules::picker::options{});
        if (!result)
        {
            if (error)
            {
                *error = result.error().code();
            }
            if (size)
            {
                *size = 0;
            }
            return;
        }

        write_range(result->string(), file, size);
    }

    void saucer_picker_pick_folder(saucer_desktop *handle, saucer_picker_options *options, char *folder, size_t *size,
                                   int *error)
    {
        auto result = handle->value().pick<type::folder>(options ? options->value() : saucer::modules::picker::options{});
        if (!result)
        {
            if (error)
            {
                *error = result.error().code();
            }
            if (size)
            {
                *size = 0;
            }
            return;
        }

        write_range(result->string(), folder, size);
    }

    void saucer_picker_pick_files(saucer_desktop *handle, saucer_picker_options *options, char *files, size_t *size, int *error)
    {
        auto result = handle->value().pick<type::files>(options ? options->value() : saucer::modules::picker::options{});
        if (!result)
        {
            if (error)
            {
                *error = result.error().code();
            }
            if (size)
            {
                *size = 0;
            }
            return;
        }

        auto packed = std::vector<char>{};
        for (const auto &path : *result)
        {
            const auto str = path.string();
            packed.insert(packed.end(), str.begin(), str.end());
            packed.emplace_back('\0');
        }

        if (!packed.empty())
        {
            packed.pop_back();
        }

        write_range(packed, files, size);
    }

    void saucer_picker_save(saucer_desktop *handle, saucer_picker_options *options, char *location, size_t *size, int *error)
    {
        auto result = handle->value().pick<type::save>(options ? options->value() : saucer::modules::picker::options{});
        if (!result)
        {
            if (error)
            {
                *error = result.error().code();
            }
            if (size)
            {
                *size = 0;
            }
            return;
        }

        write_range(result->string(), location, size);
    }
}
