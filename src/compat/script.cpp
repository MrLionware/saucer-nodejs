#include "script.h"
#include "script.hpp"

namespace
{
    saucer::script::time to_time(const SAUCER_LOAD_TIME time)
    {
        return time == SAUCER_LOAD_TIME_CREATION ? saucer::script::time::creation : saucer::script::time::ready;
    }
}

extern "C"
{
    saucer_script *saucer_script_new(const char *code, SAUCER_LOAD_TIME time)
    {
        return saucer_script::from(saucer::script{
            .code = code ? code : "",
            .run_at = to_time(time),
        });
    }

    void saucer_script_free(saucer_script *handle)
    {
        delete handle;
    }

    void saucer_script_set_frame(saucer_script *handle, SAUCER_WEB_FRAME frame)
    {
        handle->value().no_frames = frame != SAUCER_WEB_FRAME_ALL;
    }

    void saucer_script_set_time(saucer_script *handle, SAUCER_LOAD_TIME time)
    {
        handle->value().run_at = to_time(time);
    }

    void saucer_script_set_permanent(saucer_script *handle, bool permanent)
    {
        handle->value().clearable = !permanent;
    }

    void saucer_script_set_code(saucer_script *handle, const char *code)
    {
        handle->value().code = code ? code : "";
    }
}
