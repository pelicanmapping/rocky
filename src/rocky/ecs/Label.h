/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <rocky/Math.h>
#include <rocky/ecs/Component.h>

namespace ROCKY_NAMESPACE
{
    struct LabelStyle
    {
        enum class Align {
            Left, Center, Right
        };

        std::string font;

        Align horizontalAlignment = Align::Center;
        Align verticalAlignment = Align::Center;

        float pointSize = 14.0f;
        float outlineSize = 0.0f;

        glm::fvec3 pixelOffset = { 0.0f, 0.0f, 0.0f };
    };

    /**
    * Text label component
    */
    struct Label : public RevisionedComponent
    {
        std::string text;
        LabelStyle style;
    };
}
