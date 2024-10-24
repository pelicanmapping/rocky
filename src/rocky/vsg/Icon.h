/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Image.h>
#include <rocky/vsg/ECS.h>
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

    namespace detail
    {
        /**
        * VSG command to apply an IconStyle
        */
        class ROCKY_EXPORT BindIconStyle : public vsg::Inherit<vsg::BindDescriptorSet, BindIconStyle>
        {
        public:
            //! Construct a default styling command
            BindIconStyle();

            //! Refresh the data buffer contents on the GPU
            void updateStyle(const IconStyle&);

            vsg::ref_ptr<vsg::Data> _image;
            vsg::ref_ptr<vsg::ubyteArray> _styleData;
            vsg::ref_ptr<vsg::Data> _imageData;
        };

        /**
        * Command to render Icon geometry
        */
        class ROCKY_EXPORT IconGeometry : public vsg::Inherit<vsg::Geometry, IconGeometry>
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
    }

    /**
    * Icon Component - an icon is a 2D billboard with a texture
    * at a geolocation.
    */
    class ROCKY_EXPORT Icon : public ECS::NodeComponent
    {
    public:
        //! Construct the component
        Icon();

        //! Dynamic styling for the icon
        IconStyle style;

        //! Image to use for the icon texture
        std::shared_ptr<Image> image;
        vsg::ref_ptr<vsg::Data> imageData;

        //! serialize as JSON string
        std::string to_json() const override;

        //! Call after changing the style or image
        void dirty();

        void dirtyImage();

    public: // NodeComponent

        int featureMask() const override;

    private:
        vsg::ref_ptr<detail::BindIconStyle> bindCommand;
        vsg::ref_ptr<detail::IconGeometry> geometry;
        friend class IconSystemNode;
    };
}
