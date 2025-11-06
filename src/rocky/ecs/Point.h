/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Color.h>
#include <rocky/SRS.h>
#include <rocky/ecs/Component.h>
#include <vector>

namespace ROCKY_NAMESPACE
{
    struct ROCKY_EXPORT PointStyle : public ComponentBase2<PointStyle>
    {
        // if alpha is zero, use the line's per-vertex color instead
        Color color = Color{ 1, 1, 1, 1 };
        float width = 3.0f; // pixels
        float antialias = 0.25f;
        float depthOffset = 0.0f; // meters
    };


    struct ROCKY_EXPORT PointGeometry : public ComponentBase2<PointGeometry>
    {
        //! SRS of the points in the points vector (when set)
        SRS srs;

        //! Points can be absolute (in the world SRS), relative to a topocentric
        //! coordinate system (if a topocentric Transform is in use), or in the SRS of the 
        //! referencePoint if that is in use.
        std::vector<glm::dvec3> points;

        //! Colors per vertex (optional). If this vector is empty, the color in the 
        //! LineStyle is used for all points.
        std::vector<Color> colors;

        //! reset this geometry for reuse.
        void recycle(entt::registry&);
    };


    /**
    * Point(s) component - holds a collection of points
    */
    class ROCKY_EXPORT Point : public ComponentBase2<Point>
    {
    public:
        //! Entity holding the PointStyle to use
        entt::entity style = entt::null;

        //! Entity holding the PointGeometry to use
        entt::entity geometry = entt::null;

        //! Whether lines should write to the depth buffer
        bool writeDepth = true;

        //! Useful constructors
        inline Point() = default;
        inline Point(const PointGeometry& geometry_) : geometry(geometry_.owner) {}
        inline Point(const PointGeometry& geometry_, const PointStyle& style_) : geometry(geometry_.owner), style(style_.owner) {}
    };
}
