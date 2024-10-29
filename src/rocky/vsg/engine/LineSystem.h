/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/Line.h>
#include <rocky/vsg/ECS.h>
#include <vsg/state/BindDescriptorSet.h>
#include <vsg/nodes/Geometry.h>

namespace ROCKY_NAMESPACE
{   
    /**
    * Renders a line or linestring geometry.
    */
    class ROCKY_EXPORT LineGeometry : public vsg::Inherit<vsg::Geometry, LineGeometry>
    {
    public:
        //! Construct a new line string geometry node
        LineGeometry();

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
        std::vector<vsg::vec3> _current;
        std::vector<vsg::vec3> _previous;
        std::vector<vsg::vec3> _next;
        std::vector<vsg::vec4> _colors;
        vsg::ref_ptr<vsg::DrawIndexed> _drawCommand;
    };

    /**
    * Applies a line style.
    */
    class ROCKY_EXPORT BindLineDescriptors : public vsg::Inherit<vsg::BindDescriptorSet, BindLineDescriptors>
    {
    public:
        //! Construct a line style node
        BindLineDescriptors();

        //! Initialize this command with the associated layout
        void init(vsg::ref_ptr<vsg::PipelineLayout> layout);

        //! Refresh the data buffer contents on the GPU
        void updateStyle(const LineStyle&);

        vsg::ref_ptr<vsg::ubyteArray> _styleData;
    };

    /**
     * ECS system that handles LineString components
     */
    class ROCKY_EXPORT LineSystemNode : public vsg::Inherit<ECS::SystemNode<Line>, LineSystemNode>
    {
    public:
        //! Construct the system
        LineSystemNode(entt::registry& registry);

        enum Features
        {
            DEFAULT = 0x0,
            WRITE_DEPTH = 1 << 0,
            NUM_PIPELINES = 2
        };

        //! Returns a mask of supported features for the given mesh
        int featureMask(const Line&) const override;

        //! One-time initialization of the system    
        void initializeSystem(Runtime&) override;

    private:
        bool update(entt::entity, Runtime& runtime) override;
    };
}
