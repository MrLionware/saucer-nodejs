#pragma once

#include "utils/handle.hpp"
#include <cstddef>
#include <optional>
#include <string>

struct saucer_options_data
{
    std::string id;

  public:
    std::optional<int> argc;
    std::optional<char **> argv;

  public:
    std::size_t threads{0};
};

struct saucer_options : bindings::handle<saucer_options, saucer_options_data>
{
};
