/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Log.h"
#include "Context.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <mutex>

ROCKY_ABOUT(spdlog, std::to_string(SPDLOG_VER_MAJOR) + "." + std::to_string(SPDLOG_VER_MINOR) + "." + std::to_string(SPDLOG_VER_PATCH));

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::detail;

Logger rocky::Log()
{
    static std::once_flag s_once;

    std::call_once(s_once, []()
        {
            const spdlog::level::level_enum default_level = spdlog::level::info;
            try
            {
                auto logger = spdlog::stdout_color_mt("rocky");
                logger->set_pattern("%^[%n %l]%$ %v");

                auto log_level = getEnvVar("ROCKY_LOG_LEVEL");
                if (!log_level.has_value()) log_level = getEnvVar("ROCKY_NOTIFY_LEVEL");
                if (!log_level.has_value()) logger->set_level(default_level);
                else if (ciEquals(log_level.value(), "trace")) logger->set_level(spdlog::level::trace);
                else if (ciEquals(log_level.value(), "info")) logger->set_level(spdlog::level::info);
                else if (ciEquals(log_level.value(), "debug")) logger->set_level(spdlog::level::debug);
                else if (ciEquals(log_level.value(), "warn")) logger->set_level(spdlog::level::warn);
                else if (ciEquals(log_level.value(), "error")) logger->set_level(spdlog::level::err);
                else if (ciEquals(log_level.value(), "critical")) logger->set_level(spdlog::level::critical);
                else if (ciEquals(log_level.value(), "off")) logger->set_level(spdlog::level::off);
                else logger->set_level(default_level);
            }
            catch (spdlog::spdlog_ex ex)
            {
                std::cout << "SPDLOG EXCEPTION: " << std::string(ex.what()) << std::endl;
                std::exit(-1);
            }
        });

    auto logger = spdlog::get("rocky");

    if (!logger)
    {
        // catch for calling Log() after static destruction of the spdlog internal instance manager..
        // at least we'll get stderr output at the WARN level
        auto sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
        logger = std::make_shared<spdlog::logger>("rocky", sink);
        logger->set_level(spdlog::level::warn);
        logger->set_pattern("%^[%n %l]%$ %v");
    }

    return logger;
}
