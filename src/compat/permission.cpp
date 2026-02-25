#include "permission.h"

#include "permission.hpp"
#include "url.hpp"

extern "C"
{
    void saucer_permission_request_free(saucer_permission_request *request)
    {
        delete request;
    }

    saucer_permission_request *saucer_permission_request_copy(saucer_permission_request *request)
    {
        if (!request)
        {
            return nullptr;
        }

        return saucer_permission_request::make(request->value());
    }

    saucer_url *saucer_permission_request_url(saucer_permission_request *request)
    {
        if (!request || !request->value())
        {
            return nullptr;
        }

        return saucer_url::from(request->value()->url());
    }

    saucer_permission_type saucer_permission_request_type(saucer_permission_request *request)
    {
        if (!request || !request->value())
        {
            return SAUCER_PERMISSION_TYPE_UNKNOWN;
        }

        return static_cast<saucer_permission_type>(request->value()->type());
    }

    void saucer_permission_request_accept(saucer_permission_request *request, bool value)
    {
        if (request && request->value())
        {
            request->value()->accept(value);
        }
    }

    void saucer_permission_request_native(saucer_permission_request *request, size_t idx, void *result, size_t *size)
    {
        if (!request || !request->value() || idx != 0)
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

        *reinterpret_cast<void **>(result) = reinterpret_cast<void *>(request->value()->native<false>());
    }
}
