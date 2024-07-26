/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/ECS.h>
#include <vsg/text/Text.h>
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
    class ROCKY_EXPORT Label : public ECS::NodeComponent
    {
    public:
        //! Construct a new label component
        Label();

        //! Label content; call dirty() to apply
        std::string text;

        //! Label style; call dirty() to apply
        LabelStyle style;

        //! Apply changes
        void dirty() override;

        //! serialize as JSON string
        JSON to_json() const override;

    public: // NodeComponent interface

        void initializeNode(const ECS::NodeComponent::Params&) override;

    protected:
        vsg::ref_ptr<vsg::Text> textNode;
        vsg::ref_ptr<vsg::stringValue> valueBuffer;
        vsg::ref_ptr<vsg::StandardLayout> layout;
        vsg::ref_ptr<vsg::Options> options;
        LabelStyle appliedStyle;
        std::string appliedText;
    };
}
