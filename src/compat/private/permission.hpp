#pragma once

#include "utils/handle.hpp"

#include <saucer/permission.hpp>
#include <memory>

struct saucer_permission_request
    : bindings::handle<saucer_permission_request, std::shared_ptr<saucer::permission::request>>
{
};
