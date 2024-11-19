/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/ECS.h>

namespace ROCKY_NAMESPACE
{
    /**
    * Component representing an entity's visibility state across mulitple views.
    */
    struct Visibility : public ecs::PerView<bool, 4, true>
    {
        //! overall active state
        bool active = true;

        //! setting this ties this component to another, ignoring the internal settings
        Visibility* parent = nullptr;
    };
}
