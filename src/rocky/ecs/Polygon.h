/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Color.h>
#include <rocky/SRS.h>
#include <rocky/ecs/Component.h>
#include <vector>

namespace ROCKY_NAMESPACE
{
    /**
     * One filled polygon with an exterior ring and zero or more holes.
     *
     * Rings are implicitly closed; callers need not repeat the first point at
     * the end. The exterior ring should wind counter-clockwise and holes should
     * wind clockwise when viewed in the geometry's XY coordinate plane.
     */
    struct ROCKY_EXPORT PolygonPart
    {
        using Ring = std::vector<glm::dvec3>;

        Ring outer;
        std::vector<Ring> holes;
    };

    /** Geometry for one or more independent filled polygons. */
    struct ROCKY_EXPORT PolygonGeometry : public Component<PolygonGeometry>
    {
        //! Reference SRS for all ring coordinates, when applicable.
        SRS srs;

        //! Independent polygons. Each polygon may contain holes.
        std::vector<PolygonPart> polygons;

        //! Optional color per polygon, used when PolygonStyle::useGeometryColors is true.
        std::vector<Color> colors;
    };

    /** Appearance shared by a collection of filled polygons. */
    struct ROCKY_EXPORT PolygonStyle : public Component<PolygonStyle>
    {
        //! Default fill color.
        Color color = StockColor::White;

        //! Use the colors in PolygonGeometry instead of color when available.
        bool useGeometryColors = false;

        //! Entity supplying an optional ImageTexture or VSG Texture. Null disables texturing.
        entt::entity texture = entt::null;

        //! Vertex adjustment, in meters, used to mitigate depth fighting for
        //! ordinary rendering. This does not apply when using Overlay.
        float depthOffset = 0.0f;

        //! 4x4 stippling pattern (lower 16 bits only). Ignored by vector overlays.
        std::uint32_t stipplePattern = 0xFFFF;

        //! Tessellation resolution hint, in meters, when building polygon
        //! geometry. A value of zero uses the default curvature-following
        //! resolution.
        float resolution = 0.0f;
    };

    /** Filled polygon primitive. */
    class ROCKY_EXPORT Polygon : public Component<Polygon>
    {
    public:
        //! Entity hosting the PolygonGeometry to use.
        entt::entity geometry = entt::null;

        //! Entity hosting the PolygonStyle to use.
        entt::entity style = entt::null;

        inline Polygon() = default;
        inline Polygon(const PolygonGeometry& geometry_) : geometry(geometry_.owner) { }
        inline Polygon(const PolygonGeometry& geometry_, const PolygonStyle& style_) :
            geometry(geometry_.owner), style(style_.owner) { }
    };
}
