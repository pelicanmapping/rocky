/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/ecs/Component.h>
#include <rocky/Color.h>
#include <rocky/SRS.h>
#include <array>

namespace ROCKY_NAMESPACE
{
    //! A mesh triangle
    template<typename VEC2 = glm::fvec2, typename VEC3 = glm::dvec3, typename VEC4 = glm::vec4>
    struct Triangle_t
    {
        std::array<VEC3, 3> verts;
        std::array<VEC4, 3> colors = { VEC4{1,1,1,1}, VEC4{1,1,1,1}, VEC4{1,1,1,1} };
        std::array<VEC2, 3> uvs = { VEC2{0,0}, VEC2{0,0}, VEC2{0,0} };
    };
    using Triangle = Triangle_t<>;


    //! Mesh geometry component
    struct MeshGeometry : public ComponentBase2<MeshGeometry>
    {
        //! Separate arrays containing verts, colors, UVs, and indices.
        std::vector<glm::dvec3> vertices;
        std::vector<glm::fvec4> colors;
        std::vector<glm::fvec3> normals;
        std::vector<glm::fvec2> uvs;
        std::vector<std::uint32_t> indices;

        //! Alternatively, you can define a collection of triangles.
        //! Prefer the array method above.
        std::vector<Triangle> triangles;

        //! Reference SRS for the triangles data.
        SRS srs;
    };

    //! Mesh style component
    struct MeshStyle : public ComponentBase2<MeshStyle>
    {
        //! Default color for the mesh
        Color color = { 1, 1, 1, 1 };

        //! If true, use the per-vertex colors defined in the associated MeshGeometry
        bool useGeometryColors = false;

        // vertex adjustment (in meters) to apply to the mesh verts
        // as a simple method or avoiding depth fighting
        float depthOffset = 0.0f;

        //! Entity hotsing the (optional) Texture to use
        entt::entity texture = entt::null;

        //! Whether to display only the edges of the mesh triangles
        bool wireframe = false;

        //! Whether to apply normal-based lighting to the mesh
        bool lighting = false;

        //! Whether the mesh should write to the depth buffer
        bool writeDepth = true;

        //! Whether to draw back-facing triangles
        //! (front-facing triangles are wound counter-clockwise)
        bool drawBackfaces = false;

        //! Whether to use a two-pass rendering technique that will help
        //! blend semi-transparent objects propertly. "writeDepth" must
        //! also be TRUE for this to take effect.
        bool twoPassAlpha = false;
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
