/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/ViewLocal.h>
#include <cstdint>
#include <climits>

namespace ROCKY_NAMESPACE
{
    /**
    * Component whose precense indicates that an entity is active.
    */
    struct ActiveState
    {
        bool active = true;
    };

    /**
    * Component representing an entity's visibility state across mulitple views.
    */
    struct Visibility
    {
        Visibility() {
            visible.fill(true);
            frame.fill(static_cast<std::int64_t>(UINT_MAX));
        }

        //! Whether this entity is visible in each view
        detail::ViewLocal<bool> visible;

        //! Frame number when this entity was last visible in each view,
        //! or ~0 if not in use.
        detail::ViewLocal<std::int64_t> frame;

        //! setting this ties this component to another, ignoring the internal settings
        Visibility* parent = nullptr;

        //! Activate the ability to control visibility based on visitation frame
        inline void enableFrameAgeVisibility(bool on) {
            frame.fill(on ? 0 : static_cast<std::int64_t>(UINT_MAX));
        }
    };
}
