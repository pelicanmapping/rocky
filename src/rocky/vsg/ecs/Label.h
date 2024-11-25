/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/ecs/Component.h>
#include <vsg/text/Font.h>
#include <vsg/text/StandardLayout.h>

namespace ROCKY_NAMESPACE
{
    struct LabelStyle
    {
        vsg::ref_ptr<vsg::Font> font;
        vsg::StandardLayout::Alignment horizontalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT;
        vsg::StandardLayout::Alignment verticalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT;
        float pointSize = 14.0f;
        float outlineSize = 0.0f;
        vsg::vec3 pixelOffset = { 0.0f, 0.0f, 0.0f };
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
