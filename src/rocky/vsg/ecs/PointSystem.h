/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/ecs/Point.h>
#include <rocky/vsg/ecs/ECSNode.h>

namespace ROCKY_NAMESPACE
{
    /**
    * Renders a point(s) geometry.
    */
    class ROCKY_EXPORT PointGeometryNode : public vsg::Inherit<vsg::VertexDraw, PointGeometryNode>
    {
    public:
        //! Construct a new line string geometry node
        PointGeometryNode() = default;

        //! Populate the geometry arrays
        template<typename VEC3_T, typename VEC4_T>
        inline void set(const std::vector<VEC3_T>& verts, const std::vector<VEC4_T>& colors,
            const std::vector<float>& widths);

        std::size_t allocatedCapacity = 0u;

        vsg::ref_ptr<vsg::VertexDraw> _drawCommand;
        vsg::ref_ptr<vsg::vec3Array> _verts;
        vsg::ref_ptr<vsg::vec4Array> _colors;
        vsg::ref_ptr<vsg::floatArray> _widths;

        void calcBound(vsg::dsphere& out, const vsg::dmat4& matrix) const;
    };

    namespace detail
    {
        // "point.style" in the shader
        struct PointStyleRecord
        {
            Color color;
            float width;
            float antialias;
            float depthOffset;
            std::uint32_t perVertexMask = 0; // bit 0 = color, bit 1 = width
            float devicePixelRatio = 1.0f;
            std::uint32_t padding[3];

            inline void populate(const PointStyle& in) {
                color = in.color;
                width = in.width;
                antialias = in.antialias;
                depthOffset = in.depthOffset;
                perVertexMask =
                    (in.useGeometryColors ? 0x1 : 0x0) |
                    (in.useGeometryWidths ? 0x2 : 0x0);
            }
        };
        static_assert(sizeof(PointStyleRecord) % 16 == 0, "PointStyleRecord must be 16-byte aligned");


        // "point" in the shader
        struct PointStyleUniform
        {
            PointStyleRecord style; // actual style data
        };
        static_assert(sizeof(PointStyleUniform) % 16 == 0, "PointStyleUniform must be 16-byte aligned");


        struct PointStyleDetail : public StyleDetail<PointStyleDetail>
        {
            vsg::ref_ptr<vsg::BindDescriptorSet> bind;
            vsg::ref_ptr<vsg::Data> styleData;
            vsg::ref_ptr<vsg::DescriptorBuffer> styleUBO;

            inline void recycle() {
                bind = nullptr;
                styleData = nullptr;
                styleUBO = nullptr;
            }
        };

        struct PointGeometryDetail
        {
            struct View
            {
                vsg::ref_ptr<vsg::Node> root;
                vsg::ref_ptr<PointGeometryNode> geomNode;
                std::size_t capacity = 0;

                inline void recycle() {
                    root = nullptr;
                    geomNode = nullptr;
                    capacity = 0;
                }
            };

            ViewLocal<View> views;
        };
    }


    /**
     * ECS system that handles Point components
     */
    class ROCKY_EXPORT PointSystemNode : public vsg::Inherit<detail::SimpleSystemNodeBase, PointSystemNode>
    {
    public:
        //! Construct the system
        PointSystemNode(Registry& registry);

    public: // SimpleSystemNodeBase
        void initialize(VSGContext) override;
        void update(VSGContext) override;

    public: // vsg::Object
        void traverse(vsg::RecordTraversal&) const override;
        void traverse(vsg::ConstVisitor& v) const override;
        void traverse(vsg::Visitor& v) override;

    public: // vsg::Compilable
        void compile(vsg::Context& cc) override;

    private:
        mutable detail::PointStyleDetail _defaultStyleDetail;
        mutable std::vector<detail::PointStyleDetail*> _styleDetailBins;
        mutable float _devicePixelRatio = -1.0;

        inline vsg::PipelineLayout* getPipelineLayout(const Point&) {
            return _pipelines[0].config->layout;
        }

        // Called when a point geometry component is found in the dirty list
        void createOrUpdateGeometry(const PointGeometry& geom, detail::PointGeometryDetail&);

        // Called when a point style is found in the dirty list
        void createOrUpdateStyle(const PointStyle& style, detail::PointStyleDetail& styleDetail);

        // Called when a specific view's properties change (e.g. srs switch)
        void createOrUpdateGeometryForView(ViewIDType, const PointGeometry&, detail::PointGeometryDetail&);
    };



    template<typename VEC3_T, typename VEC4_T>
    void PointGeometryNode::set(const std::vector<VEC3_T>& t_verts, 
        const std::vector<VEC4_T>& t_colors, const std::vector<float>& widths)
    {
        const vsg::vec4 useStyleColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        const float useStyleWidth = 2.0f;

        auto& verts = reinterpret_cast<const std::vector<vsg::dvec3>&>(t_verts);
        auto& colors = reinterpret_cast<const std::vector<vsg::vec4>&>(t_colors);

        // always allocate space for a minimum of 4 verts.
        std::size_t requiredCapacity = std::max((std::size_t)4, verts.capacity());

        if (!_verts) // capacity exceeded, new object
        {
            // this should only happen on a new PointGeometry
            _verts = vsg::vec3Array::create(requiredCapacity);

            _colors = vsg::vec4Array::create(requiredCapacity);
            std::fill(_colors->begin(), _colors->end(), useStyleColor);

            _widths = vsg::floatArray::create(requiredCapacity);
            std::fill(_widths->begin(), _widths->end(), useStyleWidth);

            assignArrays({ _verts, _colors, _widths });

            allocatedCapacity = requiredCapacity;
        }
        else
        {
            ROCKY_SOFT_ASSERT_AND_RETURN(requiredCapacity <= _verts->size(), void(), "PointGeometry overflow");
        }

        std::copy(verts.begin(), verts.end(), _verts->begin());
        std::copy(colors.begin(), colors.end(), _colors->begin());
        std::copy(widths.begin(), widths.end(), _widths->begin());

        vertexCount = (std::uint32_t)verts.size();
        instanceCount = 1;

        // not strictly necessary since we are using the upload() technique,
        // but keep for good measure
        _verts->dirty();
        _colors->dirty();
        _widths->dirty();
    }
}
