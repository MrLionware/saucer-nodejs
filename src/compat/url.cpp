#include "url.h"

#include "url.hpp"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

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

    void write_optional(const std::optional<std::string> &value, char *result, size_t *size)
    {
        if (!size)
        {
            return;
        }

        if (!value.has_value())
        {
            *size = 0;
            return;
        }

        write_range(*value, result, size);
    }
}

extern "C"
{
    void saucer_url_free(saucer_url *url)
    {
        delete url;
    }

    saucer_url *saucer_url_copy(saucer_url *url)
    {
        if (!url)
        {
            return nullptr;
        }

        return saucer_url::make(url->value());
    }

    saucer_url *saucer_url_new_parse(const char *value, int *error)
    {
        auto parsed = saucer::url::parse(value ? value : "");
        if (!parsed.has_value())
        {
            if (error)
            {
                *error = parsed.error().code();
            }

            return nullptr;
        }

        return saucer_url::from(std::move(parsed).value());
    }

    saucer_url *saucer_url_new_from(const char *value, int *error)
    {
        auto parsed = saucer::url::from(std::filesystem::path{value ? value : ""});
        if (!parsed.has_value())
        {
            if (error)
            {
                *error = parsed.error().code();
            }

            return nullptr;
        }

        return saucer_url::from(std::move(parsed).value());
    }

    saucer_url *saucer_url_new_opts(const char *scheme, const char *host, size_t *port, const char *path)
    {
        saucer::url::options opts{
            .scheme = scheme ? scheme : "",
            .path = std::filesystem::path{path ? path : ""},
        };

        if (host)
        {
            opts.host = host;
        }

        if (port)
        {
            opts.port = *port;
        }

        return saucer_url::from(saucer::url::make(opts));
    }

    void saucer_url_string(saucer_url *url, char *string, size_t *size)
    {
        if (!url)
        {
            if (size)
            {
                *size = 0;
            }
            return;
        }

        write_range(url->value().string(), string, size);
    }

    void saucer_url_path(saucer_url *url, char *path, size_t *size)
    {
        if (!url)
        {
            if (size)
            {
                *size = 0;
            }
            return;
        }

        write_range(url->value().path().string(), path, size);
    }

    void saucer_url_scheme(saucer_url *url, char *scheme, size_t *size)
    {
        if (!url)
        {
            if (size)
            {
                *size = 0;
            }
            return;
        }

        write_range(url->value().scheme(), scheme, size);
    }

    void saucer_url_host(saucer_url *url, char *host, size_t *size)
    {
        if (!url)
        {
            if (size)
            {
                *size = 0;
            }
            return;
        }

        write_optional(url->value().host(), host, size);
    }

    bool saucer_url_port(saucer_url *url, size_t *port)
    {
        if (!url)
        {
            return false;
        }

        auto value = url->value().port();
        if (value.has_value() && port)
        {
            *port = *value;
        }

        return value.has_value();
    }

    void saucer_url_user(saucer_url *url, char *user, size_t *size)
    {
        if (!url)
        {
            if (size)
            {
                *size = 0;
            }
            return;
        }

        write_optional(url->value().user(), user, size);
    }

    void saucer_url_password(saucer_url *url, char *password, size_t *size)
    {
        if (!url)
        {
            if (size)
            {
                *size = 0;
            }
            return;
        }

        write_optional(url->value().password(), password, size);
    }

    void saucer_url_native(saucer_url *url, size_t idx, void *result, size_t *size)
    {
        if (!url || idx != 0)
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

        *reinterpret_cast<void **>(result) = reinterpret_cast<void *>(url->value().native<false>());
    }
}
