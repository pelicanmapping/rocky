/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky_vsg/MapObject.h>
#include <rocky/Image.h>
#include <rocky/Status.h>
#include <vsg/io/Options.h>
#include <vsg/utils/GraphicsPipelineConfigurator.h>
#include <vsg/nodes/StateGroup.h>
#include <vsg/nodes/Geometry.h>
#include <vsg/commands/DrawIndexed.h>

namespace ROCKY_NAMESPACE
{
    struct IconStyle;
    class Runtime;

    /**
     * Creates commands for rendering icon primitives.
     */
    class ROCKY_VSG_EXPORT IconState
    {
    public:
        //! Destructs the state objects
        ~IconState();

        //! Create the state commands necessary for rendering objects.
        static void initialize(Runtime&);

        // Status; check before using.
        static Status status;

        //! Singleton pipeline config object created when the object is first constructed,
        //! for access to pipeline and desriptor set layouts.
        static vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> pipelineConfig;

        //! Singleton state commands for establishing the pipeline.
        static vsg::StateGroup::StateCommands pipelineStateCommands;
    };

    /**
    * Applies a style.
    */
    class ROCKY_VSG_EXPORT BindIconStyle : public vsg::Inherit<vsg::BindDescriptorSet, BindIconStyle>
    {
    public:
        //! Construct a default mesh styling command
        BindIconStyle();

        //! Style for any linestrings that are children of this node
        void setStyle(const IconStyle&);
        const IconStyle& style() const;

        //! Image to render to the icon
        void setImage(std::shared_ptr<Image> image);
        std::shared_ptr<Image> image() const;

        vsg::ref_ptr<vsg::ubyteArray> styleData;
        vsg::ref_ptr<vsg::Data> imageData;

    private:
        void rebuildDescriptorSet();
        std::shared_ptr<Image> my_image;
    };

    /**
    * Renders an icon geometry.
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
}
