#pragma once

#ifdef nil
#define SAUCER_NODEJS_RESTORE_NIL_MACRO 1
#pragma push_macro("nil")
#undef nil
#endif

#include <saucer/webview.h>
#include <saucer/smartview.hpp>

#ifdef SAUCER_NODEJS_RESTORE_NIL_MACRO
#pragma pop_macro("nil")
#undef SAUCER_NODEJS_RESTORE_NIL_MACRO
#endif

#include <glaze/json/write.hpp>

#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <stdexcept>
#include <utility>

struct saucer_handle
{
    saucer::smartview view;
    std::shared_ptr<saucer::window> window;
    saucer_on_message m_on_message{};
    std::optional<std::size_t> m_message_listener{};

  private:
    template <typename T>
    static std::string serialize(T &&value)
    {
        return glz::write_json(std::forward<T>(value)).value_or("null");
    }

    template <typename... Ts>
    static std::string format_runtime(std::string code, Ts &&...params)
    {
        std::vector<std::string> args;
        args.reserve(sizeof...(Ts));
        (args.emplace_back(serialize(std::forward<Ts>(params))), ...);

        std::string out;
        out.reserve(code.size() + args.size() * 8);

        std::size_t arg_idx = 0;
        for (std::size_t i = 0; i < code.size(); ++i)
        {
            if (code[i] == '{' && i + 1 < code.size() && code[i + 1] == '}' && arg_idx < args.size())
            {
                out += args[arg_idx++];
                ++i;
                continue;
            }

            out.push_back(code[i]);
        }

        return out;
    }

  public:
    template <typename Function>
    void expose(std::string name, Function &&func)
    {
        view.expose(std::move(name), std::forward<Function>(func));
    }

    template <typename Function, typename Policy>
    void expose(std::string name, Function &&func, Policy &&)
    {
        view.expose(std::move(name), std::forward<Function>(func));
    }

    void clear_exposed()
    {
        view.unexpose();
    }

    void clear_exposed(const std::string &name)
    {
        view.unexpose(name);
    }

    template <typename... Ts>
    void execute(const std::string &code, Ts &&...params)
    {
        view.saucer::webview::execute(format_runtime(code, std::forward<Ts>(params)...));
    }

    template <typename R, typename... Ts>
    auto evaluate(const std::string &code, Ts &&...params)
    {
        auto formatted = format_runtime(code, std::forward<Ts>(params)...);
        auto future = view.template evaluate<R>("eval({})", formatted);

        return std::async(std::launch::async, [future = std::move(future)]() mutable -> R {
            auto result = future.get();
            if (!result.has_value())
            {
                throw std::runtime_error(result.error());
            }

            return std::move(result.value());
        });
    }

    template <bool Stable = true>
    auto native() const
    {
        return view.template native<Stable>();
    }
};
