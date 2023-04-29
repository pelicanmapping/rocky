/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky_vsg/MapObject.h>
#include <vsg/text/Text.h>

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

    protected:
        void createNode(Runtime&) override;

    private:
        std::string _text;
        vsg::ref_ptr<vsg::Group> _culler;
        vsg::ref_ptr<vsg::Text> _textNode;
        vsg::ref_ptr<vsg::stringValue> _value;
    };
}
