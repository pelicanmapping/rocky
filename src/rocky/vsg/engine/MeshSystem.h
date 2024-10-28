/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/Mesh.h>
#include <rocky/vsg/ECS.h>

namespace ROCKY_NAMESPACE
{
    class Runtime;

    /**
    * Command to render a Mesh's triangles.
    */
    class ROCKY_EXPORT MeshGeometry : public vsg::Inherit<vsg::Geometry, MeshGeometry>
    {
    public:
        //! Construct a new line string geometry node
        MeshGeometry();

        inline void add(
            const vsg::dvec3* verts,
            const vsg::vec2* uvs,
            const vsg::vec4* colors,
            const float* depthoffsets);

        void add(
            const vsg::vec3* verts,
            const vsg::vec2* uvs,
            const vsg::vec4* colors,
            const float* depthoffsets);

        //! Recompile the geometry after making changes.
        //! TODO: just make it dynamic instead
        void compile(vsg::Context&) override;

        vsg::vec4 _defaultColor = { 1,1,1,1 };
        std::vector<vsg::vec3> _verts;
        std::vector<vsg::vec3> _normals;
        std::vector<vsg::vec4> _colors;
        std::vector<vsg::vec2> _uvs;
        std::vector<float> _depthoffsets;
        vsg::ref_ptr<vsg::DrawIndexed> _drawCommand;
        using index_type = unsigned int; // short;
        using key = std::tuple<vsg::vec3, vsg::vec4>; // vert, color (add normal?)
        std::map<key, index_type> _lut;
        std::vector<index_type> _indices;
    };

    /**
    * Command to bind any descriptors associated with Mesh.
    */
    class ROCKY_EXPORT BindMeshDescriptors : public vsg::Inherit<vsg::BindDescriptorSet, BindMeshDescriptors>
    {
    public:
        //! Construct a default styling command
        BindMeshDescriptors();

        //! Initialize this command with the associated layout
        void init(vsg::ref_ptr<vsg::PipelineLayout> layout);

        //! Refresh the data buffer contents on the GPU
        void updateStyle(const MeshStyle&);

        vsg::ref_ptr<vsg::ubyteArray> _styleData;
        vsg::ref_ptr<vsg::ImageInfo> _imageInfo;
    };

    inline void MeshGeometry::add(const vsg::dvec3* verts, const vsg::vec2* uvs, const vsg::vec4* colors, const float* depthoffsets)
    {
        vsg::vec3 verts32[3] = {
            vsg::vec3(verts[0]),
            vsg::vec3(verts[1]),
            vsg::vec3(verts[2]) };

        add(verts32, uvs, colors, depthoffsets);
    }



    struct MeshRenderable : public ECS::NodeComponent
    {
        vsg::ref_ptr<BindMeshDescriptors> bindCommand;
        vsg::ref_ptr<MeshGeometry> geometry;
    };

    /**
    * VSG node that renders Mesh components.
    */
    class ROCKY_EXPORT MeshSystemNode :
        public vsg::Inherit<ECS::SystemNode<Mesh, MeshRenderable>, MeshSystemNode>
    {
    public:
        //! Construct the mesh renderer
        MeshSystemNode(entt::registry& registry);

        virtual ~MeshSystemNode();

        //! Supported features in a mask format
        enum Features
        {
            NONE           = 0,
            TEXTURE        = 1 << 0,
            DYNAMIC_STYLE  = 1 << 1,
            WRITE_DEPTH    = 1 << 2,
            CULL_BACKFACES = 1 << 3,
            NUM_PIPELINES  = 16
        };

        //! Returns a mask of supported features for the given mesh
        int featureMask(const Mesh& mesh) const override;

        //! One-time initialization of the system        
        void initializeSystem(Runtime&) override;

        //! Called by ENTT when the user creates a new component.
        void on_construct(entt::registry& registry, entt::entity);

    protected:

        bool update(entt::entity, Runtime&) override;
    };




    //TODO-- Move this into its own header.

    struct NodeRenderable : public ECS::NodeComponent
    {
        vsg::ref_ptr<vsg::Node> node;
    };

    /**
    * VSG node that renders Node components (just plain vsg nodes)
    */
    class ROCKY_EXPORT NodeSystemNode :
        public vsg::Inherit<ECS::SystemNode<NodeGraph,NodeRenderable>, NodeSystemNode>
    {
    public:
        NodeSystemNode(entt::registry& registry);
        virtual ~NodeSystemNode();

        void on_construct(entt::registry& registry, entt::entity entity);
        bool update(entt::entity, Runtime&) override;
    };

}
