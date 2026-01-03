#pragma once

#include <saucer/webview.h>

#include <saucer/smartview.hpp>

struct saucer_handle : saucer::smartview<>
{
    saucer_on_message m_on_message{};

  public:
    using saucer::smartview<>::smartview;

  public:
    bool on_message(const std::string &message) override
    {
        if (saucer::smartview<>::on_message(message))
        {
            return true;
        }

        if (!m_on_message)
        {
            return false;
        }

        return m_on_message(this, message.c_str());
    }
};
