/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/ecs/Mesh.h>
#include <rocky/vsg/VSGContext.h>
#include <rocky/vsg/ecs/ECSNode.h>
#include <rocky/vsg/ecs/RenderTextureParticipant.h>
#include <rocky/vsg/ecs/ECSTypes.h>
#include <algorithm>

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

        void compile(vsg::Context&) override;

        //! Whether all Vulkan resources needed to record this geometry exist.
        bool ready(std::uint32_t deviceID) const
        {
            if (_verts.empty())
                return true;

            if (commands.empty() || !_drawCommand || _drawCommand->indexCount == 0 ||
                !indices || !indices->buffer ||
                indices->buffer->sizeVulkanData() <= deviceID ||
                indices->buffer->vk(deviceID) == VK_NULL_HANDLE)
                return false;

            for (const auto& array : arrays)
            {
                if (!array || !array->buffer ||
                    array->buffer->sizeVulkanData() <= deviceID ||
                    array->buffer->vk(deviceID) == VK_NULL_HANDLE)
                    return false;
            }

            const auto& vertexData = _vulkanData[deviceID];
            if (vertexData.vkBuffers.size() != arrays.size())
                return false;

            return std::all_of(vertexData.vkBuffers.begin(), vertexData.vkBuffers.end(),
                [](VkBuffer buffer) { return buffer != VK_NULL_HANDLE; });
        }

        void record(vsg::CommandBuffer& commandBuffer) const override
        {
            if (ready(commandBuffer.deviceID))
                vsg::Geometry::record(commandBuffer);
        }

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
        struct MeshStyleUniform
        {
            Color color;
            float depthOffset; // meters
            std::uint32_t stipplePattern = 0xFFFFFFFF; // Note, only uses lower 16 bits            
            std::uint32_t featureMask = 0; // 1 = texture; 2 = lighting; 4 = per-vertex colors
            std::uint32_t padding[1]; // pad to 16 bytes

            inline void populate(const MeshStyle& in) {
                color = in.color;
                depthOffset = in.depthOffset;
                stipplePattern = in.stipplePattern;
                featureMask =
                    (in.texture != entt::null ? 0x01 : 0) |
                    (in.lighting ? 0x02 : 0) |
                    (in.useGeometryColors ? 0x04 : 0);
            }
        };
        static_assert(sizeof(MeshStyleUniform) % 16 == 0, "MeshStyleUniform must be 16-byte aligned");


        // "mesh" in the shader
        struct MeshUniform
        {
            MeshStyleUniform style;
        };
        static_assert(sizeof(MeshUniform) % 16 == 0, "MeshUniform must be 16-byte aligned");


        // ECS component: internal data paired with MeshStyle
        struct MeshStyleDetail : public StyleDetail<MeshStyleDetail>
        {
            entt::entity texture = entt::null; // last know texture entt, for change tracking

            vsg::ref_ptr<vsg::BindDescriptorSet> bind;
            vsg::ref_ptr<vsg::Data> styleUBOData;
            vsg::ref_ptr<vsg::DescriptorBuffer> styleUBO;
            vsg::ref_ptr<vsg::DescriptorImage> styleTexture;
        };


        // ECS component: internal data paired with MeshGeometry
        struct MeshGeometryDetail
        {
            struct View
            {
                vsg::ref_ptr<vsg::Node> root;
                vsg::ref_ptr<MeshGeometryNode> geomNode;
                std::size_t capacity = 0;
            };
            ViewLocal<View> views;
        };


        // ECS component: internal data paired with MeshTexture
        struct MeshTextureDetail
        {
            // nop
            bool unused = true;
        };
    }

    /**
    * VSG node that renders Mesh components.
    */
    class ROCKY_EXPORT MeshSystemNode :
        public vsg::Inherit<detail::SimpleSystemNodeBase, MeshSystemNode>,
        public RenderTextureParticipant
    {
    public:
        RenderTextureParticipant* renderTextureParticipant() override { return this; }
        vsg::Node* renderTextureNode() override { return this; }
        vsg::ref_ptr<vsg::Node> renderTextureCompileNode() override { return pipelineCompileNode(); }
        int renderTextureOrder() const override { return RenderTextureOrder::Mesh; }
        RenderTextureSourceStatus renderTextureSourceStatus(entt::registry&, entt::entity) const override;
        void expandRenderTextureBounds(entt::registry&, entt::entity, RenderTextureBounds&, const SRS&, bool) override;
        void contributeRenderTextureRevision(entt::registry&, entt::entity, RenderTextureRevision&) override;
        //! Construct the mesh renderer
        MeshSystemNode(Registry& registry);

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
        inline vsg::PipelineLayout* getPipelineLayout(const Mesh& line) {
            return _pipelines[0].config->layout;
        }

        // Default mesh style to use if a Mesh doesn't have one
        mutable detail::MeshStyleDetail _defaultStyleDetail;
        mutable std::vector<detail::MeshStyleDetail*> _styleDetailBins;
        std::uint32_t _deviceID = 0u;

        // Called when a line geometry component is found in the dirty list
        void createOrUpdateGeometry(const MeshGeometry&, detail::MeshGeometryDetail&);

        // Called when a specific view's properties change (e.g. srs switch)
        void createOrUpdateGeometryForView(ViewIDType, const MeshGeometry&, detail::MeshGeometryDetail&);

        // Called when a line style is found in the dirty list
        void createOrUpdateStyle(const MeshStyle&, detail::MeshStyleDetail&, entt::registry&, VSGContext);

        // Called when a new mesh texture shows up
        void addOrUpdateTexture(const MeshTexture&, detail::MeshTextureDetail&, entt::registry&);



        void on_construct_Mesh(entt::registry& r, entt::entity e);
        void on_construct_MeshStyle(entt::registry& r, entt::entity e);
        void on_construct_MeshGeometry(entt::registry& r, entt::entity e);
        void on_construct_Texture(entt::registry& r, entt::entity e);
        void on_destroy_MeshStyle(entt::registry& r, entt::entity e);
        void on_destroy_MeshStyleDetail(entt::registry& r, entt::entity e);
        void on_destroy_MeshGeometry(entt::registry& r, entt::entity e);
        void on_destroy_MeshGeometryDetail(entt::registry& r, entt::entity e);
        void on_destroy_MeshTexture(entt::registry& r, entt::entity e);
        void on_destroy_MeshTextureDetail(entt::registry& r, entt::entity e);
        void on_update_Mesh(entt::registry& r, entt::entity e);
        void on_update_MeshStyle(entt::registry& r, entt::entity e);
        void on_update_MeshGeometry(entt::registry& r, entt::entity e);
        void on_update_Texture(entt::registry& r, entt::entity e);
    };
}

EVSG_type_name(rocky::MeshGeometryNode)
EVSG_type_name(rocky::MeshSystemNode)
