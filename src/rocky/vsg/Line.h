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
        int stipple_pattern = 0xffff;
        int stipple_factor = 1;
        float resolution = 100000.0f; // meters
        float depth_offset = 0.0f; // meters
    };

    /**
    * LineString component - holds one or more separate line string geometries
    * sharing the same style.
    */
    class Line : public ECS::VisibleComponent
    {
    public:
        //! Dynamic line styling. This is optional.
        std::optional<LineStyle> style;

        //! Whether lines should write to the depth buffer
        bool write_depth = false;

        //! When set, the line vertices will be transformed relative to this point 
        //! for precision localization. Furthermore all line point should be expressed
        //! in the SRS of the referencePoint.
        GeoPoint referencePoint;

        //! Geometry
        using Part = std::vector<vsg::dvec3>;
        std::vector<Part> parts;

        //! Pushes a new sub-geometry along with its range of points. Each point
        //! is expressed in normal VSG coordinates, UNLESS referencePoint is set,
        //! in which case each point should be expressing in the SRS of the referencePoint.
        //! @param begin Iterator of first point to add to the new sub-geometry
        //! @param end Iterator past the final point to add to the new sub-geometry
        template<class DVEC3_ITER>
        inline void push(DVEC3_ITER begin, DVEC3_ITER end)
        {
            Part part;
            auto size = std::distance(begin, end);
            part.reserve(size);

            for (DVEC3_ITER i = begin; i != end; ++i)
            {
                part.emplace_back(i->x, i->y, i->z);
            }
            parts.emplace_back(std::move(part));
        }
    };
}
