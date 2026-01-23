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
    //! Mesh geometry component
    struct MeshGeometry : public Component<MeshGeometry>
    {
        //! Separate arrays containing verts, colors, UVs, and indices.
        std::vector<glm::dvec3> vertices;
        std::vector<glm::fvec4> colors;
        std::vector<glm::fvec3> normals;
        std::vector<glm::fvec2> uvs;
        std::vector<std::uint32_t> indices;

        //! Reference SRS for the vertex data.
        SRS srs;

        //! Reserve space for X points and Y indices
        inline void reserve(size_t x, size_t y) {
            vertices.reserve(x);
            colors.reserve(x);
            normals.reserve(x);
            uvs.reserve(x);
            indices.reserve(y);
        }
    };

    //! Mesh style component
    struct MeshStyle : public Component<MeshStyle>
    {
        //! Default color for the mesh
        Color color = { 1, 1, 1, 1 };

        //! If true, use the per-vertex colors defined in the associated MeshGeometry
        bool useGeometryColors = false;

        // vertex adjustment (in meters) to apply to the mesh verts
        // as a simple method or avoiding depth fighting
        float depthOffset = 0.0f;

        //! Entity hosting the (optional) MeshTexture to use
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
    struct Mesh : public Component<Mesh>
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
