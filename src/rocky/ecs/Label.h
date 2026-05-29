/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <rocky/Color.h>
#include <rocky/Image.h>
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
        Color textColor = StockColor::White;

        //! Size of the text, in points
        float textSize = 24.0f;

        //! Width of the text outline, in pixels
        float textOutlineSize = 1.0f;

        //! Text outline color, when textOutlineSize > 0
        Color textOutlineColor = Color("#0f0f0f");

        //! Unit location of the pivot point; for alignment.
        //! Each dimension is [0..1] where 0 is upper-left, 1 is lower-right.
        glm::fvec2 textPivot = { 0.5f, 0.5f };

        //! Screen offset of the label from its transformed position, in pixels
        glm::ivec2 textOffset = { 0, 0 };


        //! Image to use for an (optional) icon
        Image::Ptr icon;

        //! Image size in pixels
        float iconSizePixels = 32.0f;

        //! Image rotation in degrees
        float iconRotationDegrees = 0.0f;

        //! Pivot point for the icon , in unit coordinates [0..1] where 0 is upper-left, 1 is lower-right
        glm::fvec2 iconPivot = { 0.5f, 0.5f };


        //! Width of the border, in pixels
        float borderSize = 0.0f;

        //! Border color, when borderSize > 0
        Color borderColor = StockColor::Lime;

        //! Background color
        Color backgroundColor = StockColor::Transparent;

        //! Padding between the contents and the border (pixels)
        glm::fvec2 padding = { 2.0f, 2.0f };
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

        //! Create a new empty label
        Label() {}

        //! Create a new label with text
        Label(std::string_view t) :
            text(t) { }

        //! Create a new label with text and style
        Label(std::string_view t, const LabelStyle& style_in) :
            text(t), style(style_in.owner) { }
    };
}
