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
    constexpr unsigned MAX_MESH_STYLES = 1024;
    constexpr unsigned MAX_MESH_TEXTURES = 1024;

    namespace detail
    {
        struct MeshStyleRecord
        {
            Color color;
            float depthOffset = 0.0f;
            int textureIndex = -1;
            std::uint32_t padding[2]; // pad to 16 bytes

            inline void populate(const MeshStyle& in) {
                color = in.color;
                depthOffset = in.depthOffset;
            }
        };
        static_assert(sizeof(MeshStyleRecord) % 16 == 0, "MeshStyleRecord must be 16-byte aligned");


        struct MeshStyleLUT
        {
            std::array<MeshStyleRecord, MAX_MESH_STYLES> lut;
        };
        static_assert(sizeof(MeshStyleLUT) % 16 == 0, "MeshStyleLUT must be 16-byte aligned");
        

        struct MeshUniforms
        {
            std::int32_t styleIndex = -1; // index into the style LUT.

            // pad to 16 bytes
            std::uint32_t padding[3];
        };
        static_assert(sizeof(MeshUniforms) % 16 == 0, "MeshUniforms must be 16-byte aligned");

        class ROCKY_EXPORT BindMeshDescriptors : public vsg::Inherit<vsg::BindDescriptorSet, BindMeshDescriptors>
        {
        public:
            vsg::ref_ptr<vsg::Data> _uniformsData;
            vsg::ref_ptr<vsg::DescriptorBuffer> _uniformsBuffer;
        };
    }

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
        struct MeshStyleDetail
        {
            entt::entity texture = entt::null; // last know texture entt
            int styleAtlasIndex = -1; // index into the Styles LUT SSBO
        };

        struct MeshGeometryDetail
        {
            vsg::ref_ptr<vsg::Node> node;
            vsg::ref_ptr<MeshGeometryNode> geomNode;
            vsg::ref_ptr<vsg::CullNode> cullNode;
            std::size_t capacity = 0;
        };

        struct MeshDetail
        {
            vsg::ref_ptr<vsg::StateGroup> node;
            BindMeshDescriptors* bind = nullptr;
        };

        struct MeshTextureDetail
        {
            int texAtlasIndex = -1; // index into the texture array
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

        std::array<bool, MAX_MESH_STYLES> _styleInUse;
        unsigned _styleLUTSize = 0;

        std::array<bool, MAX_MESH_TEXTURES> _textureInUse;
        unsigned _textureArenaSize = 0;

        inline vsg::PipelineLayout* getPipelineLayout(const Mesh& line) {
            return _pipelines[0].config->layout;
        }

        struct RenderLeaf {
            vsg::Node* node = nullptr;
            TransformDetail* xformDetail = nullptr;
        };
        mutable std::vector<RenderLeaf> _renderLeaves;

        // SSBO data for the Style LUT
        mutable vsg::ref_ptr<vsg::ubyteArray> _styleAtlasData;

        // GPU buffer for tye Style LUT
        mutable vsg::ref_ptr<vsg::DescriptorBuffer> _styleAtlasBuffer;

        // GPU buffer for the texture atlas array
        mutable vsg::ref_ptr<vsg::DescriptorImage> _textureAtlasBuffer;

        // Called when a component is marked dirty (i.e., upon first creation or when either the
        // style of the geometry entity is reassigned).
        void createOrUpdateComponent(const Mesh&, detail::MeshDetail&, detail::MeshStyleDetail*,
            detail::MeshGeometryDetail*, VSGContext&);

        // Called when a line geometry component is found in the dirty list
        void createOrUpdateGeometry(const MeshGeometry&, detail::MeshGeometryDetail&, VSGContext&);

        // Called when a line style is found in the dirty list
        void createOrUpdateStyle(const MeshStyle&, detail::MeshStyleDetail&, entt::registry&);

        // Called when a new mesh texture shows up
        void addOrUpdateTexture(const MeshTexture&, detail::MeshTextureDetail&);
    };
}
