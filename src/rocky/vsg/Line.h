/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/ECS.h>
#include <rocky/SRS.h>
#include <vsg/nodes/Geometry.h>
#include <vsg/commands/DrawIndexed.h>
#include <vsg/state/BindDescriptorSet.h>
#include <optional>

namespace ROCKY_NAMESPACE
{
    //! Settings when constructing a similar set of line drawables
    //! Note, this structure is mirrored on the GPU so alignment rules apply!
    struct LineStyle
    {
        // if alpha is zero, use the line's per-vertex color instead
        vsg::vec4 color = { 1, 1, 1, 0 };
        float width = 2.0f; // pixels
        int stipple_pattern = ~0;
        int stipple_factor = 1;
        float resolution = 100000.0f; // meters
        float depth_offset = 0.0f; // meters
    };

    /**
    * LineString component - holds one or more separate line string geometries
    * sharing the same style.
    */
    class Line : public ecs::RevisionedComponent
    {
    public:
        //! Dynamic line styling
        LineStyle style;

        //! Whether lines should write to the depth buffer
        bool write_depth = false;

        //! When set, the line vertices will be transformed relative to this point 
        //! for precision localization. Furthermore all line point should be expressed
        //! in the SRS of the referencePoint.
        GeoPoint referencePoint;

        //! Maximum reserved size. Set this if you know the maximum number of points you 
        //! plan to use.
        std::size_t staticSize = 0;

        //! Geometry. NB, the actual array elements are stored on the heap
        std::vector<vsg::dvec3> points;
    };
}
