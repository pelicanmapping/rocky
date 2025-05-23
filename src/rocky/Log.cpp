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

                std::string log_level = util::getEnvVar("ROCKY_LOG_LEVEL");
                if (log_level.empty()) log_level = util::getEnvVar("ROCKY_NOTIFY_LEVEL");
                if (util::ciEquals(log_level, "trace")) logger->set_level(spdlog::level::trace);
                else if (util::ciEquals(log_level, "info")) logger->set_level(spdlog::level::info);
                else if (util::ciEquals(log_level, "debug")) logger->set_level(spdlog::level::debug);
                else if (util::ciEquals(log_level, "warn")) logger->set_level(spdlog::level::warn);
                else if (util::ciEquals(log_level, "error")) logger->set_level(spdlog::level::err);
                else if (util::ciEquals(log_level, "critical")) logger->set_level(spdlog::level::critical);
                else if (util::ciEquals(log_level, "off")) logger->set_level(spdlog::level::off);
                else logger->set_level(default_level);
            }
            catch (spdlog::spdlog_ex ex)
            {
                std::cout << "SPDLOG EXCEPTION: " << std::string(ex.what()) << std::endl;
                std::exit(-1);
            }
        });

    return spdlog::get("rocky");
}
