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
    //! Settings when constructing a similar set of line drawables
    //! Note, this structure is mirrored on the GPU so alignment rules apply!
    struct ROCKY_EXPORT LineStyle : public ComponentBase2<LineStyle>
    {
        // if alpha is zero, use the line's per-vertex color instead
        Color color = Color{ 1, 1, 1, 1 };
        float width = 2.0f; // pixels
        int stipplePattern = ~0;
        int stippleFactor = 1;
        float resolution = 100000.0f; // meters
        float depthOffset = 0.0f; // meters
    };


    enum struct LineTopology
    {
        Strip, // a single line strip
        Segments // a series of disconnected line segments
    };


    struct ROCKY_EXPORT LineGeometry : public ComponentBase2<LineGeometry>
    {
        //! Goemetry configuration
        LineTopology topology = LineTopology::Strip;

        //! SRS of the points in the points vector (when set)
        SRS srs;

        //! Points can be absolute (in the world SRS), relative to a topocentric
        //! coordinate system (if a topocentric Transform is in use), or in the SRS of the 
        //! referencePoint if that is in use.
        std::vector<glm::dvec3> points;

        //! reset this geometry for reuse.
        void recycle(entt::registry&);
    };


    /**
    * Line string component - holds one or more separate line string geometries
    * sharing the same style.
    */
    class ROCKY_EXPORT Line : public ComponentBase2<Line>
    {
    public:
        //! Entity holding the LineStyle to use
        entt::entity style = entt::null;

        //! Entity holding the LineGeometry to use
        entt::entity geometry = entt::null;

        //! Whether lines should write to the depth buffer
        bool writeDepth = true;

        //! Call this to reset the underlying data if you plan to re-use the line
        //! later for a different set of points.
        //void recycle(entt::registry&);

        //! Useful constructors
        inline Line() = default;
        inline Line(entt::entity geometry_) : geometry(geometry_) {}
        inline Line(entt::entity geometry_, entt::entity style_) : geometry(geometry_), style(style_) {}
        inline Line(const LineGeometry& geometry_) : geometry(geometry_.owner) {}
        inline Line(const LineGeometry& geometry_, const LineStyle& style_) : geometry(geometry_.owner), style(style_.owner) {}
    };
}
