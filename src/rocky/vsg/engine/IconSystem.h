/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/Icon.h>
#include <rocky/vsg/ECS.h>

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

    struct ROCKY_EXPORT IconRenderable : public ECS::NodeComponent
    {
        IconRenderable();

        entt::entity icon_id = entt::null;
        vsg::ref_ptr<vsg::Data> imageData;
        vsg::ref_ptr<class BindIconStyle> bindCommand;
        vsg::ref_ptr<class IconGeometry> geometry;

        //! Call after changing the style or image
        void dirty(Icon&);

        void dirtyImage();
    };

    /**
     * Creates commands for rendering icon primitives.
     */
    class ROCKY_EXPORT IconSystemNode :
        public vsg::Inherit<ECS::SystemNode<Icon, IconRenderable>, IconSystemNode>
    {
    public:
        //! Construct the mesh renderer
        IconSystemNode(entt::registry& r);

        virtual ~IconSystemNode();

        //! Features supported by this renderer
        enum Features
        {
            NONE = 0x0,
            NUM_PIPELINES = 1
        };

        //! Get the feature mask for a given icon
        int featureMask(const Icon& icon) const override { return 0; }

        //! Initialize the system (once)
        void initializeSystem(Runtime&) override;


    public: // entt signals

        //! Called by ENTT when the user creates a new Icon.
        void on_construct(entt::registry& registry, entt::entity);

    private:

        //! Called by the helper to initialize a new node component.
        bool update(entt::entity entity, Runtime& runtime) override;

        // cache of image descriptors so we can re-use textures & samplers
        std::unordered_map<std::shared_ptr<Image>, vsg::ref_ptr<vsg::DescriptorImage>> descriptorImage_cache;
    };
}
