/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Image.h>
#include <rocky_vsg/ECS.h>
#include <optional>

namespace ROCKY_NAMESPACE
{
    /**
     * Dynamic render settings for an icon.
     */
    struct IconStyle
    {
        float size_pixels = 256.0f;
        float rotation_radians = 0.0f;
        float padding[2];
    };

    /**
    * VSG command to apply an IconStyle
    */
    class ROCKY_VSG_EXPORT BindIconStyle : public vsg::Inherit<vsg::BindDescriptorSet, BindIconStyle>
    {
    public:
        //! Construct a default styling command
        BindIconStyle();

        //! Initialize this command with the associated layout
        void init(vsg::ref_ptr<vsg::PipelineLayout> layout);

        //! Refresh the data buffer contents on the GPU
        void updateStyle(const IconStyle&);

        std::shared_ptr<Image> _image;
        vsg::ref_ptr<vsg::ubyteArray> _styleData;
        vsg::ref_ptr<vsg::Data> _imageData;
    };

    /**
    * Command to render Icon geometry
    */
    class ROCKY_VSG_EXPORT IconGeometry : public vsg::Inherit<vsg::Geometry, IconGeometry>
    {
    public:
        //! Construct a new line string geometry node
        IconGeometry();

        //! Recompile the geometry after making changes.
        //! TODO: just make it dynamic instead
        void compile(vsg::Context&) override;

    protected:
        vsg::ref_ptr<vsg::Draw> _drawCommand;
    };

    /**
    * Icon Component - an icon is a 2D billboard with a texture
    * at a geolocation.
    */
    class ROCKY_VSG_EXPORT Icon : public ECS::NodeComponent
    {
    public:
        //! Construct the component
        Icon();

        //! Dynamic styling for the icon
        IconStyle style;

        //! Image to use for the icon texture
        std::shared_ptr<Image> image;

        //! serialize as JSON string
        JSON to_json() const override;

        //! Call after changing the style or image
        void dirty();

        void dirtyImage();

    public: // NodeComponent

        void initializeNode(const ECS::NodeComponent::Params&) override;

        int featureMask() const override;

    private:
        vsg::ref_ptr<BindIconStyle> bindCommand;
        vsg::ref_ptr<IconGeometry> geometry;
        friend class IconSystem;
    };
}
