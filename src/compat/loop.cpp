#include "loop.h"

#include "app.hpp"
#include "utils/handle.hpp"

#include <saucer/modules/loop.hpp>
#include <memory>

struct saucer_loop : bindings::handle<saucer_loop, std::shared_ptr<saucer::modules::loop>>
{
};

extern "C"
{
    void saucer_loop_free(saucer_loop *loop)
    {
        delete loop;
    }

    saucer_loop *saucer_loop_new(saucer_application *app)
    {
        if (!app || !app->value() || !app->value()->app)
        {
            return nullptr;
        }

        return saucer_loop::from(std::make_shared<saucer::modules::loop>(*app->value()->app));
    }

    void saucer_loop_run(saucer_loop *loop)
    {
        if (loop && loop->value())
        {
            loop->value()->run();
        }
    }

    void saucer_loop_iteration(saucer_loop *loop)
    {
        if (loop && loop->value())
        {
            loop->value()->iteration();
        }
    }

    void saucer_loop_quit(saucer_loop *loop)
    {
        if (loop && loop->value())
        {
            loop->value()->quit();
        }
    }
}
