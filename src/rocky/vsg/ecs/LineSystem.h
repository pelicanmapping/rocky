/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/ecs/Line.h>
#include <rocky/vsg/ecs/ECSNode.h>

namespace ROCKY_NAMESPACE
{
    /**
    * Renders a line or linestring geometry.
    */
    class ROCKY_EXPORT LineGeometryNode : public vsg::Inherit<vsg::Geometry, LineGeometryNode>
    {
    public:
        //! Construct a new line string geometry node
        LineGeometryNode();

        //! Populate the geometry arrays
        template<typename VEC3_T>
        inline void set(const std::vector<VEC3_T>& verts, LineTopology topology, std::size_t capacity);

        //! The first vertex in the line string to render
        void setFirst(unsigned value);

        //! Last vertex in the line string to render
        void setCount(unsigned value);

        //protected:
        std::size_t _capacity = 0u;
        vsg::ref_ptr<vsg::DrawIndexed> _drawCommand;
        vsg::ref_ptr<vsg::vec3Array> _current;
        vsg::ref_ptr<vsg::vec3Array> _previous;
        vsg::ref_ptr<vsg::vec3Array> _next;
        vsg::ref_ptr<vsg::vec4Array> _colors;
        vsg::ref_ptr<vsg::uintArray> _indices;

        void calcBound(vsg::dsphere& out, const vsg::dmat4& matrix) const;

        void record(vsg::CommandBuffer& commandBuffer) const override
        {
            if (_drawCommand->indexCount > 0)
                vsg::Geometry::record(commandBuffer);
        }
    };

    namespace detail
    {
        // "line.style" in the shader
        struct LineStyleRecord
        {
            Color color;
            float width;
            std::int32_t stipplePattern;
            std::int32_t stippleFactor;
            float resolution;
            float depthOffset;
            std::uint32_t padding[3]; // pad to 16 bytes

            inline void populate(const LineStyle& in) {
                color = in.color;
                width = in.width;
                stipplePattern = in.stipplePattern;
                stippleFactor = in.stippleFactor;
                resolution = in.resolution;
                depthOffset = in.depthOffset;
            }
        };
        static_assert(sizeof(LineStyleRecord) % 16 == 0, "LineStyleRecord must be 16-byte aligned");


        // "line" in the shader
        struct LineStyleUniform
        {
            LineStyleRecord style; // actual style data
        };
        static_assert(sizeof(LineStyleUniform) % 16 == 0, "LineStyleUniform must be 16-byte aligned");


        // render leaf for collecting and drawing meshes
        struct LineDrawable
        {
            vsg::Node* node = nullptr;
            TransformDetail* xformDetail = nullptr;
        };

        using LineDrawList = std::vector<LineDrawable>;

        struct LineStyleDetail
        {
            LineDrawList drawList;

            vsg::ref_ptr<vsg::BindDescriptorSet> bind;
            vsg::ref_ptr<vsg::Data> styleData;
            vsg::ref_ptr<vsg::DescriptorBuffer> styleUBO;
        };

        struct LineGeometryDetail
        {
            vsg::ref_ptr<vsg::Node> node;
            vsg::ref_ptr<LineGeometryNode> geomNode;
            std::size_t capacity = 0;
        };

        struct LineDetail
        {
            vsg::ref_ptr<vsg::Node> node;
        };
    }


    /**
     * ECS system that handles LineString components
     */
    class ROCKY_EXPORT LineSystemNode : public vsg::Inherit<detail::SimpleSystemNodeBase, LineSystemNode>
    {
    public:
        //! Construct the system
        LineSystemNode(Registry& registry);

        // (not hooked up for multiple pipelines - reevaluate and see
        // if we can just use dynamic state instead)
        enum Features
        {
            DEFAULT = 0x0,
            WRITE_DEPTH = 1 << 0,
            NUM_PIPELINES = 1
        };
        //! Returns a mask of supported features for the given mesh
        //int featureMask(const Line&) const override;

        //! One-time initialization of the system    
        void initialize(VSGContext&) override;

        //! Periodic update to check for style changes
        void update(VSGContext&) override;

        //! Record/render traversal
        void traverse(vsg::RecordTraversal&) const override;

        void traverse(vsg::ConstVisitor&) const override;

    private:
        mutable detail::LineStyleDetail _defaultStyleDetail;
        mutable vsg::ref_ptr<vsg::MatrixTransform> _tempMT;

        inline vsg::PipelineLayout* getPipelineLayout(const Line& line) {
            return _pipelines[0].config->layout;
        }

        // Called when a Line is marked dirty (i.e., upon first creation or when either the
        // style of the geometry entity is reassigned).
        void createOrUpdateComponent(const Line&, detail::LineDetail&, detail::LineGeometryDetail*);

        // Called when a line geometry component is found in the dirty list
        void createOrUpdateGeometry(const LineGeometry& geom, detail::LineGeometryDetail&, VSGContext& context);

        // Called when a line style is found in the dirty list
        void createOrUpdateStyle(const LineStyle& style, detail::LineStyleDetail& styleDetail);
    };




    template<typename VEC3_T>
    void LineGeometryNode::set(const std::vector<VEC3_T>& t_verts, LineTopology topology, std::size_t capacity)
    {
        const vsg::vec4 defaultColor = { 1.0f, 1.0f, 1.0f, 1.0f };

        auto& verts = reinterpret_cast<const std::vector<vsg::dvec3>&>(t_verts);

        capacity = std::max(capacity, verts.size());
        capacity = std::max(capacity, (std::size_t)4);

        ROCKY_HARD_ASSERT(capacity > 0);

        std::size_t indices_to_allocate =
            topology == LineTopology::Strip ? (capacity - 1) * 6 :
            (capacity / 2) * 6; // Segments

        ROCKY_SOFT_ASSERT_AND_RETURN(_current == nullptr || capacity <= _capacity, void(),
            "LineGeometry state corruption - capacity overflow should always result in a new LineGeometry");

        if (!_current) // capacity exceeded, new object
        {
            // this should only happen on a new LineGeometry
            _current = vsg::vec3Array::create(capacity * 4);
            _previous = vsg::vec3Array::create(capacity * 4);
            _next = vsg::vec3Array::create(capacity * 4);
            _colors = vsg::vec4Array::create(capacity * 4);
            assignArrays({ _current, _previous, _next, _colors });

            _indices = vsg::uintArray::create(indices_to_allocate * 4);
            assignIndices(_indices);
        }
        else
        {
            ROCKY_SOFT_ASSERT_AND_RETURN(capacity * 4 <= _current->size(), void(), "LineGeometry overflow");
        }

        _capacity = capacity;

        auto* current = (_current->data());
        auto* prev = (_previous->data());
        auto* next = (_next->data());
        auto* color = (_colors->data());
        auto* indicies = (_indices->data());
        int i_ptr = 0;

        if (topology == LineTopology::Strip)
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
        else // topology == LineTopology::Segments
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

                if (even)
                {
                    auto e = (i) * 4 + 2;
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

        // not strictly necessary since we are using the upload() technique,
        // but keep for good measure
        _current->dirty();
        _previous->dirty();
        _next->dirty();
        _colors->dirty();
        _indices->dirty();
    }
}
