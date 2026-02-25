#include "scheme.h"
#include "scheme.hpp"

#include "stash.hpp"

#include "memory.h"
#include "utils/string.hpp"

extern "C"
{
    saucer_scheme_response *saucer_scheme_response_new(saucer_stash *data, const char *mime)
    {
        return saucer_scheme_response::from(saucer::scheme::response{
            .data = data->value(),
            .mime = mime ? mime : "application/octet-stream",
        });
    }

    void saucer_scheme_response_free(saucer_scheme_response *handle)
    {
        delete handle;
    }

    void saucer_scheme_response_set_status(saucer_scheme_response *handle, int status)
    {
        handle->value().status = status;
    }

    void saucer_scheme_response_add_header(saucer_scheme_response *handle, const char *header, const char *value)
    {
        handle->value().headers.emplace(header, value);
    }

    void saucer_scheme_response_append_header(saucer_scheme_response *handle, const char *header, const char *value)
    {
        saucer_scheme_response_add_header(handle, header, value);
    }

    void saucer_scheme_request_free(saucer_scheme_request *handle)
    {
        delete handle;
    }

    saucer_scheme_request *saucer_scheme_request_copy(saucer_scheme_request *handle)
    {
        if (!handle)
        {
            return nullptr;
        }

        return saucer_scheme_request::make(handle->value());
    }

    char *saucer_scheme_request_url(saucer_scheme_request *handle)
    {
        return bindings::alloc(handle->value().url().string());
    }

    char *saucer_scheme_request_method(saucer_scheme_request *handle)
    {
        return bindings::alloc(handle->value().method());
    }

    saucer_stash *saucer_scheme_request_content(saucer_scheme_request *handle)
    {
        return saucer_stash::from(handle->value().content());
    }

    void saucer_scheme_request_headers(saucer_scheme_request *handle, char ***headers, char ***values, size_t *count)
    {
        auto data = handle->value().headers();
        *count    = data.size();

        *headers = static_cast<char **>(saucer_memory_alloc(*count * sizeof(char *)));
        *values  = static_cast<char **>(saucer_memory_alloc(*count * sizeof(char *)));

        for (auto it = data.begin(); it != data.end(); it++)
        {
            const auto &[header, value] = *it;
            const auto index            = std::distance(data.begin(), it);

            (*headers)[index] = bindings::alloc(header);
            (*values)[index]  = bindings::alloc(value);
        }
    }

    void saucer_scheme_executor_free(saucer_scheme_executor *handle)
    {
        delete handle;
    }

    saucer_scheme_executor *saucer_scheme_executor_copy(saucer_scheme_executor *handle)
    {
        if (!handle)
        {
            return nullptr;
        }

        return saucer_scheme_executor::make(handle->value());
    }

    void saucer_scheme_executor_resolve(saucer_scheme_executor *handle, saucer_scheme_response *response)
    {
        handle->value().resolve(response->value());
    }

    void saucer_scheme_executor_accept(saucer_scheme_executor *handle, saucer_scheme_response *response)
    {
        saucer_scheme_executor_resolve(handle, response);
    }

    void saucer_scheme_executor_reject(saucer_scheme_executor *handle, SAUCER_SCHEME_ERROR error)
    {
        switch (error)
        {
        case SAUCER_REQUEST_ERROR_NOT_FOUND:
            handle->value().reject(saucer::scheme::error::not_found);
            return;
        case SAUCER_REQUEST_ERROR_INVALID:
            handle->value().reject(saucer::scheme::error::invalid);
            return;
        case SAUCER_REQUEST_ERROR_DENIED:
            handle->value().reject(saucer::scheme::error::denied);
            return;
        case SAUCER_REQUEST_ERROR_ABORTED:
        case SAUCER_REQUEST_ERROR_FAILED:
        default:
            handle->value().reject(saucer::scheme::error::failed);
            return;
        }
    }
}
