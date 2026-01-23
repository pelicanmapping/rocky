/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <array>

namespace ROCKY_NAMESPACE
{
    //! State information at the time of rendering a view.
    struct RenderingState
    {
        std::uint32_t viewID;
        std::uint64_t frame;
    };

    //! ViewLocal is a container that holds data on a per-view basis.
    template<typename T>
    struct ViewLocal : public std::array<T, ROCKY_MAX_NUMBER_OF_VIEWS>
    {
        //! default construct
        ViewLocal() = default;

        //! Construct with uniform value
        explicit ViewLocal(T v) { fill(v); };

        //! Set all views to a value (same as fill)
        ViewLocal<T>& operator = (T v) {
            this->fill(v);
            return *this;
        }
    };
}
