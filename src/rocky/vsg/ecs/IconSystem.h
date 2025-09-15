/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/ecs/Icon.h>
#include <rocky/vsg/ecs/ECSNode.h>

namespace ROCKY_NAMESPACE
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
        vsg::ref_ptr<vsg::DescriptorBuffer> _ubo;
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

    /**
     * Creates commands for rendering icon primitives.
     */
    class ROCKY_EXPORT IconSystemNode : public vsg::Inherit<detail::SystemNode<Icon>, IconSystemNode>
    {
    public:
        //! Construct the mesh renderer
        IconSystemNode(Registry& r);

        //! Features supported by this renderer
        enum Features
        {
            NONE = 0x0,
            NUM_PIPELINES = 1
        };

        //! Get the feature mask for a given icon
        int featureMask(const Icon& icon) const override { return 0; }

        //! Initialize the system (once)
        void initialize(VSGContext&) override;

    private:

        //! Called by the helper to initialize a new node component.
        void createOrUpdateNode(const Icon&, detail::BuildInfo&, VSGContext&) const override;

        // cache of image descriptors so we can re-use textures & samplers
        mutable std::unordered_map<std::shared_ptr<Image>, vsg::ref_ptr<vsg::DescriptorImage>> descriptorImage_cache;
        mutable std::mutex mutex;
    };
}
