#pragma once

#include "utils/handle.hpp"

#include <string>

struct saucer_navigation_data
{
    std::string url;
    bool new_window{false};
    bool redirection{false};
    bool user_initiated{false};
};

struct saucer_navigation : bindings::handle<saucer_navigation, saucer_navigation_data>
{
};
