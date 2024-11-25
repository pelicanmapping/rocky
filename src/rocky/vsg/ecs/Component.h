/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/Common.h>
#include <entt/entt.hpp>

namespace ROCKY_NAMESPACE
{
    /**
    * Superclass for ECS components meant to be revisioned and/or with an attach point.
    */
    struct RevisionedComponent
    {
        //! Revision, for synchronizing this component with another
        int revision = 0;
        void dirty() { revision++; }

        //! Attach point for additional components, as needed
        entt::entity attach_point = entt::null;
    };
}
