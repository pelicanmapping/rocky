/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Log.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

using namespace ROCKY_NAMESPACE;

LogSettings _log_settings_instance;

LogSettings::LogSettings()
{
    try
    {
        auto logger = spdlog::stdout_color_mt("rocky");
        logger->set_pattern("%^[%n %l]%$ %v");
    }
    catch (spdlog::spdlog_ex ex)
    {
        std::cout << "SPDLOG EXCEPTION: " << std::string(ex.what()) << std::endl;
        std::exit(-1);
    }
}

std::shared_ptr<spdlog::logger> rocky::Log()
{
    return spdlog::get("rocky");
}
