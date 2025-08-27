/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Color.h>
#include <rocky/GeoPoint.h>
#include <rocky/ecs/Component.h>
#include <vector>

namespace ROCKY_NAMESPACE
{
    //! Settings when constructing a similar set of line drawables
    //! Note, this structure is mirrored on the GPU so alignment rules apply!
    struct LineStyle
    {
        // if alpha is zero, use the line's per-vertex color instead
        Color color = Color{ 1, 1, 1, 0 };
        float width = 2.0f; // pixels
        int stipple_pattern = ~0;
        int stipple_factor = 1;
        float resolution = 100000.0f; // meters
        float depth_offset = 0.0f; // meters
    };

    /**
    * Line string component - holds one or more separate line string geometries
    * sharing the same style.
    */
    class ROCKY_EXPORT Line : public BaseComponent
    {
    public:
        //! Dynamic line styling
        LineStyle style;

        //! Whether lines should write to the depth buffer
        bool writeDepth = true;

        //! SRS of the points in the points vector (when set).
        SRS srs;

        //! Maximum reserved size. Set this if you know the maximum number of points you 
        //! plan to use.
        std::size_t staticSize = 0;

        //! Line configuration
        enum class Topology
        {
            Strip, // a single line strip
            Segments // a series of disconnected line segments
        };
        Topology topology = Topology::Strip;

        //! Points can be absolute (in the world SRS), relative to a topocentric
        //! coordinate system (if a topocentric Transform is in use), or in the SRS of the 
        //! referencePoint if that is in use.
        std::vector<glm::dvec3> points;

        //! Marks the entire line dirty
        inline void dirty() override
        {
            styleDirty = true;
            pointsDirty = true;
            ++revision;
        }

        inline void dirtyStyle()
        {
            styleDirty = true;
            ++revision;
        }

        inline void dirtyPoints()
        {
            pointsDirty = true;
            ++revision;
        }

        //! Call this to reset the underlying data if you plan to re-use the line
        //! later for a different set of points.
        void recycle(entt::registry&);

    private:
        bool styleDirty = true;
        bool pointsDirty = true;
        friend class LineSystemNode;
    };
}
