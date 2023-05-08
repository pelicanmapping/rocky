/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky_vsg/MapObject.h>
#include <rocky_vsg/engine/IconState.h>
#include <rocky/Image.h>

namespace ROCKY_NAMESPACE
{
    /**
     * Render settings for an icon.
     */
    struct IconStyle
    {
        float size_pixels;
        float padding[3];
    };

    /**
    * Icon attachment
    */
    class ROCKY_VSG_EXPORT Icon : public rocky::Inherit<Attachment, Icon>
    {
    public:
        //! Construct a mesh attachment
        Icon();

        //! Set to overall style for this mesh
        void setStyle(const IconStyle& value);

        //! Overall style for the mesh
        const IconStyle& style() const;

        //! Set the image to render
        void setImage(std::shared_ptr<Image> image);

        //! Image to render
        std::shared_ptr<Image> image() const;

        //! serialize as JSON string
        JSON to_json() const override;


    protected:
        void createNode(Runtime& runtime) override;

    private:
        vsg::ref_ptr<BindIconStyle> _bindStyle;
        vsg::ref_ptr<IconGeometry> _geometry;
    };
}
