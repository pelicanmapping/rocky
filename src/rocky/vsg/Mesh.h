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
    class MeshSystem;

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

    /**
    * Triangle mesh component
    */
    class ROCKY_EXPORT Mesh : public ECS::Component
    {
    public:
        //! Construct a mesh attachment
        Mesh() = default;

        //! Optional texture
        vsg::ref_ptr<vsg::ImageInfo> texture;

        //! Whether to write the the depth buffer
        bool writeDepth = true;

        //! Whether to cull backfaces
        bool cullBackfaces = true;

        //! Optional dynamic style data
        std::optional<MeshStyle> style;

        //! serialize as JSON string
        std::string to_json() const override {
            return {}; // TODO
        }

        //! Triangles comprising the mesh
        std::vector<Triangle> triangles;

        //! Reference point for absolute coordinate localization (optional)
        GeoPoint referencePoint;
    };



    struct NodeGraph : public ECS::Component
    {
        vsg::ref_ptr<vsg::Node> node;
    };
}
