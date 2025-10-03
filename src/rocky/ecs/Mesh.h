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

        //! Or, separate arrays containing verts, colors, UVs, and indices.
        std::vector<glm::dvec3> verts;
        std::vector<glm::fvec4> colors;
        std::vector<glm::fvec2> uvs;
        std::vector<std::uint32_t> indices;

        //! Reference SRS for the triangles data.
        SRS srs;
    };

    //! Mesh style component
    struct MeshStyle : public ComponentBase2<MeshStyle>
    {
        // if alpha is zero, use the line's per-vertex color insteade
        Color color = { 1, 1, 1, 0 };

        // vertex adjustment (in meters) to apply to the mesh verts
        // as a simple method or avoiding depth fighting
        float depthOffset = 0.0f;

        //! Entity hotsing the (optional) Texture to use
        entt::entity texture = entt::null;

        bool wireframe = false;
        bool writeDepth = true;
        bool cullBackfaces = true;
    };

    //! Mesh comonent
    struct Mesh : public ComponentBase2<Mesh>
    {
        //! Entity hosting the MeshGeometry to use
        entt::entity geometry = entt::null;

        //! Entity hosting the MeshStyle to use
        entt::entity style = entt::null;

        //! Useful constructors
        inline Mesh() = default;
        inline Mesh(const MeshGeometry& geometry_) : geometry(geometry_.owner) {}
        inline Mesh(const MeshGeometry& geometry_, const MeshStyle& style_) : geometry(geometry_.owner), style(style_.owner) {}
    };
}
