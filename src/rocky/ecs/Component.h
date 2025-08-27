/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <entt/entt.hpp>

namespace ROCKY_NAMESPACE
{
    /**
    * Superclass for ECS components that support revisionings and an attach point.
    * The attach point is for internal usage.
    */
    struct BaseComponent
    {
        //! Revision, for synchronizing this component with another
        int revision = 0;

        //! Attach point for additional components, as needed
        entt::entity attach_point = entt::null;

        //! bump the revision.
        virtual void dirty()
        {
            ++revision;
        }

        BaseComponent() = default;

        BaseComponent(const BaseComponent&) = default;

        BaseComponent& operator = (const BaseComponent&) = default;

        BaseComponent(BaseComponent&& rhs) noexcept
        {
            *this = rhs;
        }

        BaseComponent& operator = (BaseComponent&& rhs) noexcept
        {
            if (this != &rhs)
            {
                attach_point = rhs.attach_point;
                rhs.attach_point = entt::null;

                revision = rhs.revision;
                rhs.revision = 0;
            }
            return *this;
        }
    };
}
