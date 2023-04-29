/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky_vsg/Common.h>
#include <rocky/Status.h>

#include <vsg/io/Options.h>
#include <vsg/utils/GraphicsPipelineConfigurator.h>
#include <vsg/nodes/StateGroup.h>
#include <vsg/nodes/Geometry.h>
#include <vsg/commands/DrawIndexed.h>

namespace ROCKY_NAMESPACE
{
    struct LineStyle;
    class Runtime;

    /**
     * Creates commands for rendering line primitives and holds the singleton pipeline
     * configurator for line drawing state.
     */
    class ROCKY_VSG_EXPORT LineState
    {
    public:
        //! Destructs the mesh state objects
        ~LineState();

        //! Create the state commands necessary for rendering lines.
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
    * Applies a line style.
    */
    class ROCKY_VSG_EXPORT BindLineStyle : public vsg::Inherit<vsg::BindDescriptorSet, BindLineStyle>
    {
    public:
        //! Construct a line style node
        BindLineStyle();

        //! Style for any linestrings that are children of this node
        void setStyle(const LineStyle&);
        const LineStyle& style() const;

        vsg::ref_ptr<vsg::ubyteArray> _styleData;
    };

    /**
    * Renders a line or linestring geometry.
    */
    class ROCKY_VSG_EXPORT LineStringGeometry : public vsg::Inherit<vsg::Geometry, LineStringGeometry>
    {
    public:
        //! Construct a new line string geometry node
        LineStringGeometry();

        //! Adds a vertex to the end of the line string
        void push_back(const vsg::vec3& vert);

        //! Number of verts comprising this line string
        unsigned numVerts() const;

        //! The first vertex in the line string to render
        void setFirst(unsigned value);

        //! Number of vertices in the line string to render
        void setCount(unsigned value);

        //! Recompile the geometry after making changes.
        //! TODO: just make it dynamic instead
        void compile(vsg::Context&) override;

    protected:
        vsg::vec4 _defaultColor = { 1,1,1,1 };
        std::uint32_t _stippleFactor;
        std::uint16_t _stipplePattern;
        float _width;
        std::vector<vsg::vec3> _current;
        std::vector<vsg::vec3> _previous;
        std::vector<vsg::vec3> _next;
        std::vector<vsg::vec4> _colors;
        vsg::ref_ptr<vsg::DrawIndexed> _drawCommand;

        //unsigned actualVertsPerVirtualVert(unsigned) const;
        //unsigned numVirtualVerts(const vsg::Array*) const;
        //unsigned getRealIndex(unsigned) const;
        //void updateFirstCount();
    };
}
