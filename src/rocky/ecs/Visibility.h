/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <rocky/Rendering.h>
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
        //! TODO: remove this??
        Visibility* parent = nullptr;

        //! Activate the ability to control visibility based on visitation frame
        inline void enableFrameAgeVisibility(bool on) {
            frame.fill(on ? 0 : static_cast<std::int64_t>(UINT_MAX));
        }
    };

    //! Whether the Visibility component is visible in the given view
    //! @param vis The visibility component
    //! @param view_index Index of view to check visibility
    //! @return True if visible in that view
    inline bool visible(const Visibility& vis, int view_index)
    {
        return vis.parent != nullptr ? visible(*vis.parent, view_index) : vis.visible[view_index];
    }

    inline bool visible(const Visibility& vis, detail::RenderingState& rs)
    {
        if (vis.parent != nullptr)
            return visible(*vis.parent, rs);

        auto delta = (std::int64_t)rs.frame - vis.frame[rs.viewID];
        return vis.visible[rs.viewID] && delta <= 1;
    }

    //! Toggle the visibility of an entity in the given view
    //! @param r Entity registry
    //! @param e Entity id
    //! @param value New visibility state
    //! @param view_index Index of view to set visibility
    inline void setVisible(entt::registry& registry, entt::entity e, bool value, int view_index = -1)
    {
        ROCKY_SOFT_ASSERT_AND_RETURN(e != entt::null, void());

        auto& visibility = registry.get<Visibility>(e);

        if (visibility.parent == nullptr)
        {
            if (view_index >= 0)
                visibility.visible[view_index] = value;
            else
                visibility.visible.fill(value);
        }
    }

    template<typename ITER>
    inline void setVisible(entt::registry& registry, ITER begin, ITER end, bool value, int view_index = -1)
    {
        for (auto it = begin; it != end; ++it)
        {
            setVisible(registry, *it, value, view_index);
        }
    }

    //! Whether an entity is visible in the given view
    //! @param r Entity registry
    //! @param e Entity id
    //! @param view_index Index of view to check visibility
    //! @return True if visible in that view
    inline bool visible(entt::registry& registry, entt::entity e, int view_index = 0)
    {
        // assume a readlock on the registry
        ROCKY_SOFT_ASSERT_AND_RETURN(e != entt::null, false);

        return visible(registry.get<Visibility>(e), view_index);
    }
}
