/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/ECS.h>
#include <rocky/GeoPoint.h>
#include <rocky/SRS.h>
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

        // vertex adjustment (in meters) to apply to the mesh verts
        // as a simple method or avoiding depth fighting
        float depth_offset = 0.0f;
    };

    //! A mesh triangle
    template<typename VEC2 = vsg::vec2, typename VEC3 = vsg::dvec3, typename VEC4 = vsg::vec4>
    struct Triangle_t
    {
        VEC3 verts[3];
        VEC4 colors[3] = { {1,1,1,1}, {1,1,1,1}, {1,1,1,1} };
        VEC2 uvs[3] = { {0,0}, {0,0}, {0,0} };
        float depthoffsets[3] = { 0, 0, 0 };
        //VEC3 normals[3] = { {0,0,1}, {0,0,1}, {0,0,1} };
    };
    using Triangle = Triangle_t<>;

    namespace detail
    {

        /**
        * Command to render a Mesh's triangles.
        */
        class ROCKY_EXPORT MeshGeometry : public vsg::Inherit<vsg::Geometry, MeshGeometry>
        {
        public:
            //! Construct a new line string geometry node
            MeshGeometry();

            //! Adds a triangle to the mesh.
            //! Each array MUST be 3 elements long
            inline void add(
                const vsg::dvec3& refPoint,
                const vsg::vec3* verts,
                const vsg::vec2* uvs,
                const vsg::vec4* colors,
                const float* depthoffsets);

            inline void add(
                const vsg::dvec3& refPoint,
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

        private:
            void add(
                const vsg::vec3* verts,
                const vsg::vec2* uvs,
                const vsg::vec4* colors,
                const float* depthoffsets);
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

        inline void MeshGeometry::add(const vsg::dvec3& refpoint, const vsg::vec3* verts, const vsg::vec2* uvs, const vsg::vec4* colors, const float* depthoffsets)
        {
            vsg::vec3 verts32[3] = {
                vsg::vec3(vsg::dvec3(verts[0]) - refpoint),
                vsg::vec3(vsg::dvec3(verts[1]) - refpoint),
                vsg::vec3(vsg::dvec3(verts[2]) - refpoint) };

            add(verts32, uvs, colors, depthoffsets);
        }

        inline void MeshGeometry::add(const vsg::dvec3& refpoint, const vsg::dvec3* verts, const vsg::vec2* uvs, const vsg::vec4* colors, const float* depthoffsets)
        {
            vsg::vec3 verts32[3] = {
                vsg::vec3(verts[0] - refpoint),
                vsg::vec3(verts[1] - refpoint),
                vsg::vec3(verts[2] - refpoint) };

            add(verts32, uvs, colors, depthoffsets);
        }
    }

    /**
    * Triangle mesh component
    */
    class ROCKY_EXPORT Mesh : public ECS::NodeComponent
    {
    public:
        //! Construct a mesh attachment
        Mesh();

        //! Optional texture
        vsg::ref_ptr<vsg::ImageInfo> texture;

        //! Whether to write the the depth buffer
        bool writeDepth = true;

        //! Whether to cull backfaces
        bool cullBackfaces = true;

        //! Optional dynamic style data
        std::optional<MeshStyle> style;

        //! Add a triangle to the mesh
        inline void add(const Triangle& tri);

        //! If using style, call this after changing a style to apply it
        void dirty();

        //! serialize as JSON string
        JSON to_json() const override;

        
    public: // NodeComponent
        
        void initializeNode(const ECS::NodeComponent::Params&) override;

        int featureMask() const override;

    private:
        vsg::ref_ptr<detail::BindMeshDescriptors> bindCommand;
        vsg::ref_ptr<detail::MeshGeometry> geometry;
        vsg::dvec3 refPoint;
        friend class MeshSystem;
    };

    inline void Mesh::add(const Triangle& tri)
    {
        geometry->add(refPoint, tri.verts, tri.uvs, tri.colors, tri.depthoffsets);
    }
}
