/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky_vsg/MapObject.h>
#include <rocky_vsg/engine/MeshState.h>
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
    * Triangle mesh attachment
    */
    class ROCKY_VSG_EXPORT Mesh : public rocky::Inherit<Attachment, Mesh>
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

    public:

        //! Add a triangle to the mesh
        inline void add(const Triangle32& tri);

        //! Add a triangle to the mesh
        inline void add(const Triangle64& tri);

        //! serialize as JSON string
        JSON to_json() const override;

        //! If using style, call this after changing a style to apply it
        void dirty();

    protected:
        void createNode(Runtime& runtime) override;

    private:
        vsg::ref_ptr<BindMeshStyle> _bindStyle;
        vsg::ref_ptr<MeshGeometry> _geometry;
    };


    // mesh inline functions

    inline void Mesh::add(const Triangle32& tri) {
        _geometry->add(tri.verts, tri.uvs, tri.colors, tri.depthoffsets);
    }

    inline void Mesh::add(const Triangle64& tri) {
        _geometry->add(tri.verts, tri.uvs, tri.colors, tri.depthoffsets);
    }
}
