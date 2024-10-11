/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <spdlog/spdlog.h>

namespace ROCKY_NAMESPACE
{
    namespace log = spdlog;

    extern ROCKY_EXPORT std::shared_ptr<spdlog::logger> Log();
}
