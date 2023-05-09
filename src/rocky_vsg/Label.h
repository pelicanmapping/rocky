/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky_vsg/MapObject.h>
#include <vsg/text/Text.h>
#include <vsg/text/Font.h>
#include <vsg/text/StandardLayout.h>

namespace ROCKY_NAMESPACE
{
    /**
    * Text label MapObject attachment
    */
    class ROCKY_VSG_EXPORT Label : public rocky::Inherit<Attachment, Label>
    {
    public:
        Label();

        //! Set the text of the label
        void setText(const std::string& value);

        //! Label text
        const std::string& text() const;

        //! serialize as JSON string
        JSON to_json() const override;

    public:
        vsg::ref_ptr<vsg::Text> textNode;
        vsg::ref_ptr<vsg::stringValue> valueBuffer;
        vsg::ref_ptr<vsg::StandardLayout> layout;

    protected:
        void createNode(Runtime&) override;

    private:
        std::string _text;
    };
}
