/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky_vsg/ECS.h>
#include <vsg/nodes/Geometry.h>
#include <vsg/commands/DrawIndexed.h>
#include <optional>

namespace ROCKY_NAMESPACE
{
    /**
     * Render settings for a mesh.
     */
    struct MeshStyle
    {
        // if alpha is zero, use the line's per-vertex color instead
        vsg::vec4 color = { 1, 1, 1, 0 };

        // a non-zero value will overlay a wireframe on the mesh
        // for visualization purposes
        float wireframe = 0.0f;

        // depth buffer adjustment (in clip space) to apply to the mesh verts
        // as a simple method or avoiding depth fighting with terrain
        float depth_offset = 1e-7f;
    };

    //! A triangle
    template<typename VEC2=vsg::vec2, typename VEC3=vsg::vec3, typename VEC4=vsg::vec4>
    struct Triangle
    {
        VEC3 verts[3];
        VEC4 colors[3] = { {1,1,1,1}, {1,1,1,1}, {1,1,1,1} };
        VEC2 uvs[3] = { {0,0}, {0,0}, {0,0} };
        float depthoffsets[3] = { 0, 0, 0 };
        //VEC3 normals[3] = { {0,0,1}, {0,0,1}, {0,0,1} };
    };
    using Triangle32 = Triangle<>;
    using Triangle64 = Triangle<vsg::vec2, vsg::dvec3, vsg::vec4>;

    /**
    * Command to render a Mesh's triangles.
    */
    class ROCKY_VSG_EXPORT MeshGeometry : public vsg::Inherit<vsg::Geometry, MeshGeometry>
    {
    public:
        //! Construct a new line string geometry node
        MeshGeometry();

        //! Adds a triangle to the mesh.
        //! Each array MUST be 3 elements long
        void add(
            const vsg::vec3* verts,
            const vsg::vec2* uvs,
            const vsg::vec4* colors,
            const float* depthoffsets);

        inline void add(
            const vsg::dvec3* verts,
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
    class ROCKY_VSG_EXPORT BindMeshDescriptors : public vsg::Inherit<vsg::BindDescriptorSet, BindMeshDescriptors>
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

    // inline
    void MeshGeometry::add(const vsg::dvec3* verts, const vsg::vec2* uvs, const vsg::vec4* colors, const float* depthoffsets)
    {
        vsg::vec3 verts32[3] = { vsg::vec3(verts[0]), vsg::vec3(verts[1]), vsg::vec3(verts[2]) };
        add(verts32, uvs, colors, depthoffsets);
    }

    /**
    * Triangle mesh component
    */
    class ROCKY_VSG_EXPORT Mesh : public ECS::NodeComponent
    {
    public:
        //! Construct a mesh attachment
        Mesh();

        //! Optional texture
        vsg::ref_ptr<vsg::ImageInfo> texture;

        //! Whether to write the the depth buffer
        bool writeDepth = true;

        //! Optional dynamic style data
        std::optional<MeshStyle> style;

        //! Add a triangle to the mesh
        inline void add(const Triangle32& tri);

        //! Add a triangle to the mesh
        inline void add(const Triangle64& tri);

        //! If using style, call this after changing a style to apply it
        void dirty();

        //! serialize as JSON string
        JSON to_json() const override;

        
    public: // NodeComponent
        
        void initializeNode(const ECS::NodeComponent::Params&) override;

        int featureMask() const override;

    private:
        vsg::ref_ptr<BindMeshDescriptors> bindCommand;
        vsg::ref_ptr<MeshGeometry> geometry;
        friend class MeshSystem;
    };


    // mesh inline functions
    inline void Mesh::add(const Triangle32& tri) {
        geometry->add(tri.verts, tri.uvs, tri.colors, tri.depthoffsets);
    }

    inline void Mesh::add(const Triangle64& tri) {
        geometry->add(tri.verts, tri.uvs, tri.colors, tri.depthoffsets);
    }
}
