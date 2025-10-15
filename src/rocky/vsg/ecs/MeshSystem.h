/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/ecs/Mesh.h>
#include <rocky/vsg/VSGContext.h>
#include <rocky/vsg/ecs/ECSNode.h>
#include <rocky/vsg/ecs/ECSTypes.h>

namespace ROCKY_NAMESPACE
{
    /**
    * Command to render a Mesh's triangles.
    */
    class ROCKY_EXPORT MeshGeometryNode : public vsg::Inherit<vsg::Geometry, MeshGeometryNode>
    {
    public:
        //! Construct a new line string geometry node
        MeshGeometryNode();

        void reserve(size_t numVerts);

        inline void addTriangle(const vsg::dvec3* verts, const vsg::vec2* uvs, const vsg::vec4* colors);

        void addTriangle(const vsg::vec3* verts, const vsg::vec2* uvs, const vsg::vec4* colors);

        void compile(vsg::Context&) override;

        vsg::vec4 _defaultColor = { 1,1,1,1 };
        std::vector<vsg::vec3> _verts;
        std::vector<vsg::vec3> _normals;
        std::vector<vsg::vec4> _colors;
        std::vector<vsg::vec2> _uvs;
        vsg::ref_ptr<vsg::DrawIndexed> _drawCommand;
        using index_type = std::uint32_t;
        using key = std::tuple<vsg::vec3, vsg::vec4>; // vert, color (add normal?)
        std::map<key, index_type> _lut;
        std::vector<index_type> _indices;
    };

    namespace detail
    {
        // "mesh.style" in the shader
        struct MeshStyleRecord
        {
            Color color = Color(1, 1, 1, 0);
            float depthOffset = 0.0f;
            int hasTexture = 0;
            int hasLighting = 0;
            std::uint32_t padding[1]; // pad to 16 bytes

            inline void populate(const MeshStyle& in) {
                color = in.color;
                depthOffset = in.depthOffset;
                hasTexture = in.texture != entt::null ? 1 : 0;
                hasLighting = in.lighting;
            }
        };
        static_assert(sizeof(MeshStyleRecord) % 16 == 0, "MeshStyleRecord must be 16-byte aligned");

        // "mesh" in the shader
        struct MeshStyleUniform
        {
            MeshStyleRecord style;
        };
        static_assert(sizeof(MeshStyleUniform) % 16 == 0, "MeshStyleUniform must be 16-byte aligned");

        // render leaf for collecting and drawing meshes
        struct MeshDrawable
        {
            vsg::Node* node = nullptr;
            TransformDetail* xformDetail = nullptr;
        };

        using MeshDrawList = std::vector<MeshDrawable>;

        // internal data paired with MeshStyle
        struct MeshStyleDetail
        {
            entt::entity texture = entt::null; // last know texture entt, for change tracking

            vsg::ref_ptr<vsg::BindDescriptorSet> bind;
            vsg::ref_ptr<vsg::Data> styleUBOData;
            vsg::ref_ptr<vsg::DescriptorBuffer> styleUBO;
            vsg::ref_ptr<vsg::DescriptorImage> styleTexture;

            using Pass = vsg::ref_ptr<vsg::Commands>;
            std::vector<Pass> passes; // multipass rendering for a style
            MeshDrawList drawList;
        };

        // internal data paired with MeshGeometry
        struct MeshGeometryDetail
        {
            vsg::ref_ptr<MeshGeometryNode> node;
            std::size_t capacity = 0;
        };

        // internal data paired with Mesh
        struct MeshDetail
        {
            vsg::ref_ptr<vsg::Node> node;
        };

        // internal data paired with MeshTexture
        struct MeshTextureDetail
        {
            // nop
            bool unused = true;
        };
    }

    inline void MeshGeometryNode::addTriangle(const vsg::dvec3* verts, const vsg::vec2* uvs, const vsg::vec4* colors)
    {
        vsg::vec3 verts32[3] = {
            vsg::vec3(verts[0]),
            vsg::vec3(verts[1]),
            vsg::vec3(verts[2]) };

        addTriangle(verts32, uvs, colors);
    }

    /**
    * VSG node that renders Mesh components.
    */
    class ROCKY_EXPORT MeshSystemNode : public vsg::Inherit<detail::SimpleSystemNodeBase, MeshSystemNode>
    {
    public:
        //! Construct the mesh renderer
        MeshSystemNode(Registry& registry);

        //! Supported features in a mask format
        enum Features
        {
            DEFAULT = 0,
            NUM_PIPELINES = 1
            //TEXTURE        = 1 << 0,
            //DYNAMIC_STYLE  = 1 << 1,
            //WRITE_DEPTH    = 1 << 2,
            //CULL_BACKFACES = 1 << 3,
            //NUM_PIPELINES  = 16
        };

        //! Returns a mask of supported features for the given mesh
        int featureMask(const Mesh&) const;

        //! One-time initialization of the system        
        void initialize(VSGContext&) override;

        void update(VSGContext&) override;

        void traverse(vsg::RecordTraversal&) const override;

    private:

        inline vsg::PipelineLayout* getPipelineLayout(const Mesh& line) {
            return _pipelines[0].config->layout;
        }

        // Default mesh style to use if a Mesh doesn't have one
        mutable detail::MeshStyleDetail _defaultMeshStyleDetail;
        mutable std::vector<detail::MeshStyleDetail*> _styleDetailBins;

        // Called when a component is marked dirty (i.e., upon first creation or when either the
        // style of the geometry entity is reassigned).
        void createOrUpdateComponent(const Mesh&, detail::MeshDetail&, detail::MeshStyleDetail*,
            detail::MeshGeometryDetail*, VSGContext&);

        // Called when a line geometry component is found in the dirty list
        void createOrUpdateGeometry(const MeshGeometry&, detail::MeshGeometryDetail&, VSGContext&);

        // Called when a line style is found in the dirty list
        void createOrUpdateStyle(const MeshStyle&, detail::MeshStyleDetail&, entt::registry&, VSGContext&);

        // Called when a new mesh texture shows up
        void addOrUpdateTexture(const MeshTexture&, detail::MeshTextureDetail&, entt::registry&);
    };
}
