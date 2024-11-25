/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/ecs/Line.h>
#include <rocky/vsg/ecs/ECSNode.h>

#include <vsg/state/BindDescriptorSet.h>
#include <vsg/nodes/Geometry.h>
#include <vsg/commands/DrawIndexed.h>

namespace ROCKY_NAMESPACE
{
    /**
     * ECS system that handles LineString components
     */
    class ROCKY_EXPORT LineSystemNode : public vsg::Inherit<ecs::SystemNode<Line>, LineSystemNode>
    {
    public:
        //! Construct the system
        LineSystemNode(ecs::Registry& registry);

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

        void createOrUpdateNode(const Line&, ecs::BuildInfo&, Runtime&) const override;
    };

    /**
    * Renders a line or linestring geometry.
    */
    class ROCKY_EXPORT LineGeometry : public vsg::Inherit<vsg::Geometry, LineGeometry>
    {
    public:
        //! Construct a new line string geometry node
        LineGeometry();

        template<typename VEC3_T>
        inline void set(const std::vector<VEC3_T>& verts, Line::Topology topology, std::size_t staticStorage = 0);

        //! The first vertex in the line string to render
        void setFirst(unsigned value);

        //! Last vertex in the line string to render
        void setCount(unsigned value);

        //protected:
        vsg::ref_ptr<vsg::DrawIndexed> _drawCommand;
        vsg::ref_ptr<vsg::vec3Array> _current;
        vsg::ref_ptr<vsg::vec3Array> _previous;
        vsg::ref_ptr<vsg::vec3Array> _next;
        vsg::ref_ptr<vsg::vec4Array> _colors;
        vsg::ref_ptr<vsg::uintArray> _indices;

        void calcBound(vsg::dsphere& out, const vsg::dmat4& matrix) const;
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




    template<typename VEC3_T>
    void LineGeometry::set(const std::vector<VEC3_T>& verts, Line::Topology topology, std::size_t staticStorage)
    {
        const vsg::vec4 defaultColor = { 1.0f, 1.0f, 1.0f, 1.0f };

        std::size_t verts_to_allocate = (staticStorage > 0 ? staticStorage : verts.size());
        std::size_t indices_to_allocate = 
            topology == Line::Topology::Strip ? (verts_to_allocate - 1) * 6 :
            (verts_to_allocate/2) * 6; // Segments

        if (!_current)
        {
            _current = vsg::vec3Array::create(verts_to_allocate * 4);
            _current->properties.dataVariance = vsg::DYNAMIC_DATA;

            _previous = vsg::vec3Array::create(verts_to_allocate * 4);
            _previous->properties.dataVariance = vsg::DYNAMIC_DATA;
            
            _next = vsg::vec3Array::create(verts_to_allocate * 4);
            _next->properties.dataVariance = vsg::DYNAMIC_DATA;

            _colors = vsg::vec4Array::create(verts_to_allocate * 4);
            _colors->properties.dataVariance = vsg::DYNAMIC_DATA;

            assignArrays({ _current, _previous, _next, _colors });

            _indices = vsg::uintArray::create((verts_to_allocate - 1) * 6);
            _indices->properties.dataVariance = vsg::DYNAMIC_DATA;
            assignIndices(_indices);
        }

        auto* current = (_current->data());
        auto* prev = (_previous->data());
        auto* next = (_next->data());
        auto* color = (_colors->data());
        auto* indicies = (_indices->data());
        int i_ptr = 0;

        if (topology == Line::Topology::Strip)
        {
            for (unsigned i = 0; i < verts.size(); ++i)
            {
                bool first = i == 0;
                bool last = (i == verts.size() - 1);

                for (int n = 0; n < 4; ++n)
                {
                    (_previous->data())[i * 4 + n] = first ? verts[i] : verts[i - 1];
                    (_next->data())[i * 4 + n] = last ? verts[i] : verts[i + 1];
                    (_current->data())[i * 4 + n] = verts[i];
                    (_colors->data())[i * 4 + n] = defaultColor;
                }

                if (!first)
                {
                    auto e = (i - 1) * 4 + 2;
                    (_indices->data())[i_ptr++] = e + 3;
                    (_indices->data())[i_ptr++] = e + 1;
                    (_indices->data())[i_ptr++] = e + 0; // provoking vertex
                    (_indices->data())[i_ptr++] = e + 2;
                    (_indices->data())[i_ptr++] = e + 3;
                    (_indices->data())[i_ptr++] = e + 0; // provoking vertex
                }
            }
        }
        else // topology == Topology::Segments
        {
            ROCKY_SOFT_ASSERT_AND_RETURN((verts.size() & 0x1) == 0, void(), "Lines with 'Segment' topology must have an even number of vertices");

            for (unsigned i = 0; i < verts.size(); ++i)
            {
                bool even = (i & 0x1) == 0;

                for (int n = 0; n < 4; ++n)
                {
                    if (even) // beginning of segment
                    {
                        prev[i * 4 + n] = verts[i];
                        next[i * 4 + n] = verts[i + 1];
                    }
                    else // end of segment
                    {
                        prev[i * 4 + n] = verts[i - 1];
                        next[i * 4 + n] = verts[i];
                    }

                    current[i * 4 + n] = verts[i];
                    color[i * 4 + n] = defaultColor;
                }

                if (!even)
                {
                    auto e = (i - 1) * 4 + 2;
                    indicies[i_ptr++] = e + 3;
                    indicies[i_ptr++] = e + 1;
                    indicies[i_ptr++] = e + 0; // provoking vertex
                    indicies[i_ptr++] = e + 2;
                    indicies[i_ptr++] = e + 3;
                    indicies[i_ptr++] = e + 0; // provoking vertex                
                }
            }
        }

        _drawCommand->firstIndex = 0;
        _drawCommand->indexCount = i_ptr;

        _current->dirty();
        _previous->dirty();
        _next->dirty();
        _colors->dirty();
        _indices->dirty();
    }
}
