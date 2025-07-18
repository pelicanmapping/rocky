/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Color.h>
#include <rocky/GeoPoint.h>
#include <rocky/ecs/Component.h>
#include <optional>

namespace ROCKY_NAMESPACE
{
    /**
     * Render settings for a mesh.
     */
    struct MeshStyle
    {
        // if alpha is zero, use the line's per-vertex color instead
        Color color = { 1, 1, 1, 0 };

        // vertex adjustment (in meters) to apply to the mesh verts
        // as a simple method or avoiding depth fighting
        float depth_offset = 0.0f;
    };

    //! A mesh triangle
    template<typename VEC2 = glm::fvec2, typename VEC3 = glm::dvec3, typename VEC4 = glm::vec4>
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
    struct Mesh : public RevisionedComponent
    {
        //! Whether to write the the depth buffer
        bool writeDepth = true;

        //! Whether to cull backfaces
        bool cullBackfaces = true;

        //! Optional dynamic style data
        std::optional<MeshStyle> style;

        //! Triangles comprising the mesh
        std::vector<Triangle> triangles;

        //! Reference SRS for the triangles data.
        SRS srs;

        //! Entity that holds texture information (optional)
        entt::entity texture = entt::null;
    };
}
