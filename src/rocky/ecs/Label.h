/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <rocky/Color.h>
#include <rocky/ecs/Component.h>

namespace ROCKY_NAMESPACE
{
    /**
    * Label Style component.
    * Multiple Label components can share one style.
    */
    struct LabelStyle : public Component<LabelStyle>
    {
        //! Filename of the font to use
        std::string fontName;

        //! Color or the text
        Color textColor = Color::White;

        //! Size of the text, in points
        float textSize = 24.0f;

        //! Width of the text outline, in pixels
        float outlineSize = 1.0f;

        //! Text outline color, when outlineSize > 0
        Color outlineColor = Color("#0f0f0f");

        //! Width of the border, in pixels
        float borderSize = 0.0f;

        //! Border color, when borderSize > 0
        Color borderColor = Color::Lime;

        //! Background color
        Color backgroundColor = Color(0, 0, 0, 0);

        //! Padding between the text and the border (pixels)
        glm::fvec2 padding = { 2.0f, 2.0f };

        //! Unit location of the pivot point; for alignment.
        //! Each dimension is [0..1] where 0 is upper-left, 1 is lower-right.
        glm::fvec2 pivot = { 0.5f, 0.5f };

        //! Screen offset of the label from its transformed position, in pixels
        glm::ivec2 offset = { 0, 0 };
    };


    /**
    * Label ECS component.
    */
    struct Label : public Component<Label>
    {
        //! Text to display
        std::string text;

        //! Entity of LabelStyle to use; if null a default style will apply
        entt::entity style = entt::null;

        //! Create a new label with text
        Label(std::string_view t) :
            text(t) { }

        //! Create a new label with text and style
        Label(std::string_view t, const LabelStyle& style_in) :
            text(t), style(style_in.owner) { }
    };
}
