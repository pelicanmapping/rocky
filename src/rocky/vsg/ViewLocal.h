/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <array>

namespace ROCKY_NAMESPACE
{
    namespace detail
    {
        template<typename T>
        using ViewLocal = std::array<T, ROCKY_MAX_NUMBER_OF_VIEWS>;
    }
}
