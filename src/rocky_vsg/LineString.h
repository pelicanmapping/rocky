/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky_vsg/MapObject.h>
#include <rocky_vsg/engine/LineState.h>

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
        float depth_offset = 1e-7f; // tbd
    };

    /**
    * LineString Attachment
    */
    class ROCKY_VSG_EXPORT LineString : public rocky::Inherit<Attachment, LineString>
    {
    public:
        //! Construct a line string attachment
        LineString();

        //! Sets the line string geometry to the points in the provided range.
        //! @param begin Iterator to first point in the range
        //! @param end Interator past the last point in the range
        template<class VEC3_ITER>
        inline void setGeometry(VEC3_ITER begin, VEC3_ITER end);

        //! Set the rendering style for this line string
        void setStyle(const LineStyle&);

        //! rendering style for the geometry
        const LineStyle& style() const;

        //! serialize as JSON string
        JSON to_json() const override;

    protected:
        void createNode(Runtime& runtime) override;

    private:
        vsg::ref_ptr<BindLineStyle> _bindStyle;
        vsg::ref_ptr<LineStringGeometry> _geometry;
    };

    // inline implementations
    template<class VEC3_ITER> void LineString::setGeometry(VEC3_ITER begin, VEC3_ITER end) {
        _geometry = LineStringGeometry::create();
        for (VEC3_ITER i = begin; i != end; ++i)
            _geometry->push_back({ (float)i->x, (float)i->y, (float)i->z });
    }


    /**
    * MultiLineString Attachment - holds one or more separate line string geometries
    * sharing the same style.
    */
    class ROCKY_VSG_EXPORT MultiLineString : public rocky::Inherit<Attachment, MultiLineString>
    {
    public:
        //! Construct a new multi-linestring attachment
        MultiLineString();

        //! Pushes a new sub-geometry along with its range of points.
        //! @param begin Iterator of first point to add to the new sub-geometry
        //! @param end Iterator past the final point to add to the new sub-geometry
        template<class VEC3_ITER>
        inline void pushGeometry(VEC3_ITER begin, VEC3_ITER end);

        //! Set the rendering style for this line string
        void setStyle(const LineStyle&);

        //! rendering style for the geometry
        const LineStyle& style() const;

        //! serialize as JSON string
        JSON to_json() const override;

    protected:
        void createNode(Runtime& runtime) override;

    private:
        vsg::ref_ptr<BindLineStyle> _bindStyle;
        std::vector<vsg::ref_ptr<LineStringGeometry>> _geometries;
    };

    // inline implementations
    template<class VEC3_ITER> void MultiLineString::pushGeometry(VEC3_ITER begin, VEC3_ITER end) {
        auto geom = LineStringGeometry::create();
        for (VEC3_ITER i = begin; i != end; ++i)
            geom->push_back({ (float)i->x, (float)i->y, (float)i->z });
        _geometries.push_back(geom);
    }
}
