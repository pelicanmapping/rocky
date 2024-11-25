/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/ecs/Component.h>
#include <rocky/vsg/ViewLocal.h>

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
    struct Visibility : public util::ViewLocal<bool>
    {
        Visibility() {
            fill(true);
        }

        //! setting this ties this component to another, ignoring the internal settings
        Visibility* parent = nullptr;
    };
}
