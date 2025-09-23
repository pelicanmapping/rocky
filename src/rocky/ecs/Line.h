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

        //! Marks the line dirty
        inline void dirty() override;

        //! Mark the style as dirty
        [[deprecated]] inline void dirtyStyle() {
            dirty();
        }

        //! Mark the points vector as dirty
        [[deprecated]] inline void dirtyPoints() {
            dirty();
        }

        //! Call this to reset the underlying data if you plan to re-use the line
        //! later for a different set of points.
        void recycle(entt::registry&);

    private:
        // track the capacity separately from the vector since the vector's copy constructor
        // will NOT necessary copy the capacity.
        std::size_t _capacity = 0;
        friend class LineSystemNode;
    };

    inline void Line::dirty() {
        _capacity = points.capacity();
        ++revision;
    }
}
