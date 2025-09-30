/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/ecs/Component.h>
#include <rocky/Color.h>
#include <rocky/SRS.h>

namespace ROCKY_NAMESPACE
{
    //! A mesh triangle
    template<typename VEC2 = glm::fvec2, typename VEC3 = glm::dvec3, typename VEC4 = glm::vec4>
    struct Triangle_t
    {
        VEC3 verts[3];
        VEC4 colors[3] = { {1,1,1,1}, {1,1,1,1}, {1,1,1,1} };
        VEC2 uvs[3] = { {0,0}, {0,0}, {0,0} };
    };
    using Triangle = Triangle_t<>;


    //! Mesh geometry component
    struct MeshGeometry : public ComponentBase2<MeshGeometry>
    {
        //! Triangles comprising the mesh
        std::vector<Triangle> triangles;

        //! Reference SRS for the triangles data.
        SRS srs;
    };

    //! Mesh style component
    struct MeshStyle : public ComponentBase2<MeshStyle>
    {
        // if alpha is zero, use the line's per-vertex color instead
        Color color = { 1, 1, 1, 0 };

        // vertex adjustment (in meters) to apply to the mesh verts
        // as a simple method or avoiding depth fighting
        float depthOffset = 0.0f;

        // texture
        entt::entity texture = entt::null;
    };

    //! Mesh comonent
    struct Mesh : public ComponentBase2<Mesh>
    {
        entt::entity geometry = entt::null;

        entt::entity style = entt::null;

        //! Whether to write the the depth buffer
        bool writeDepth = true;

        //! Whether to cull backfaces
        bool cullBackfaces = true;

        //! Useful constructors
        inline Mesh() = default;
        inline Mesh(entt::entity geometry_) : geometry(geometry_) {}
        inline Mesh(entt::entity geometry_, entt::entity style_) : geometry(geometry_), style(style_) {}
        inline Mesh(const MeshGeometry& geometry_) : geometry(geometry_.owner) {}
        inline Mesh(const MeshGeometry& geometry_, const MeshStyle& style_) : geometry(geometry_.owner), style(style_.owner) {}
    };
}
