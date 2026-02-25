#pragma once

#include "utils/handle.hpp"

#include <saucer/app.hpp>

#include <set>
#include <string>
#include <memory>
#include <optional>
#include <filesystem>

struct saucer_preferences_data
{
    std::shared_ptr<saucer::application> application;

  public:
    bool persistent_cookies{true};
    bool hardware_acceleration{true};

  public:
    std::optional<std::filesystem::path> storage_path;
    std::optional<std::string> user_agent;

  public:
    std::set<std::string> browser_flags;
};

struct saucer_preferences : bindings::handle<saucer_preferences, saucer_preferences_data>
{
};
