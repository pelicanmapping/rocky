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

    using Logger = std::shared_ptr<spdlog::logger>;
    extern ROCKY_EXPORT Logger Log();
}
