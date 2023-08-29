/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky_vsg/ECS.h>
#include <vsg/text/Text.h>
#include <vsg/text/Font.h>
#include <vsg/text/StandardLayout.h>

namespace ROCKY_NAMESPACE
{
    /**
    * Text label component
    */
    class ROCKY_VSG_EXPORT Label : public ECS::NodeComponent
    {
    public:
        //! Construct a new label component
        Label();

        //! Label content; call dirty() to apply
        std::string text;

        //! Font (required); call dirty() to apply
        vsg::ref_ptr<vsg::Font> font;

        //! Apply property changes
        void dirty();

        //! serialize as JSON string
        JSON to_json() const override;

    public: // NodeComponent interface

        void initializeNode(const ECS::NodeComponent::Params&) override;

    protected:
        vsg::ref_ptr<vsg::Text> textNode;
        vsg::ref_ptr<vsg::stringValue> valueBuffer;
        vsg::ref_ptr<vsg::StandardLayout> layout;
        vsg::ref_ptr<vsg::Options> options;
    };
}
