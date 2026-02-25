#pragma once

#include "utils/handle.hpp"

#include <saucer/url.hpp>

struct saucer_url : bindings::handle<saucer_url, saucer::url>
{
};
