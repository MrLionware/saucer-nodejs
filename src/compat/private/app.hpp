#pragma once

#include "utils/handle.hpp"
#include <saucer/app.hpp>
#include <saucer/modules/loop.hpp>
#include <memory>

struct saucer_application_state
{
    std::shared_ptr<saucer::application> app;
    std::shared_ptr<saucer::modules::loop> loop;
};

struct saucer_screen : bindings::handle<saucer_screen, saucer::screen>
{
};

struct saucer_application : bindings::handle<saucer_application, std::shared_ptr<saucer_application_state>>
{
};
