/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Color.h>
#include <rocky/SRS.h>
#include <rocky/Units.h>
#include <rocky/ecs/Component.h>
#include <vector>

namespace ROCKY_NAMESPACE
{
    //! Settings when constructing a similar set of line drawables
    struct ROCKY_EXPORT LineStyle : public Component<LineStyle>
    {
        // if alpha is zero, use the line's per-vertex color instead
        Color color = StockColor::White;
        float width = 2.0f;

        //! Units shared by width and outlineWidth. Screen pixels preserve a
        //! constant apparent size; distance units preserve a physical size.
        //! Only screen-size and distance unit domains are supported.
        UnitsType widthUnits = Units::PIXELS;

        //! Color of the optional outline drawn around the line.
        Color outlineColor = StockColor::Black;

        //! Visible outline thickness outside each edge of the line, expressed
        //! in widthUnits.
        //! A value less than or equal to zero disables outlining.
        float outlineWidth = 0.0f;

        std::uint16_t stipplePattern = 0xFFFF;
        int stippleFactor = 1;
        float resolution = 100000.0f; // meters
        float depthOffset = 0.0f; // meters
        bool useGeometryColors = false;
        bool transparencyBin = false;
    };


    enum struct LineTopology
    {
        Strip, // a single line strip
        Segments // a series of disconnected line segments
    };


    struct ROCKY_EXPORT LineGeometry : public Component<LineGeometry>
    {
        //! Goemetry configuration
        LineTopology topology = LineTopology::Strip;

        //! SRS of the points in the points vector (when set)
        SRS srs;

        //! Points can be absolute (in the world SRS), relative to a topocentric
        //! coordinate system (if a topocentric Transform is in use), or in the SRS of the 
        //! referencePoint if that is in use.
        std::vector<glm::dvec3> points;

        //! Colors per point (optional). These apply when this geometry is coupled with a
        //! style that has useGeometryColors = true.
        std::vector<Color> colors;

        //! reset this geometry for reuse.
        void recycle(entt::registry&);
    };


    /**
    * Line string component - holds one or more separate line string geometries
    * sharing the same style.
    */
    class ROCKY_EXPORT Line : public Component<Line>
    {
    public:
        //! Entity holding the LineStyle to use
        entt::entity style = entt::null;

        //! Entity holding the LineGeometry to use
        entt::entity geometry = entt::null;

        //! Useful constructors
        inline Line() = default;
        inline Line(const LineGeometry& geometry_) : geometry(geometry_.owner) {}
        inline Line(const LineGeometry& geometry_, const LineStyle& style_) : geometry(geometry_.owner), style(style_.owner) {}
    };
}
