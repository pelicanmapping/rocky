/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/Common.h>
#include <rocky/ecs/Transform.h>
#include <rocky/vsg/ecs/TransformDetail.h>
#include <rocky/GeoPoint.h>

namespace ROCKY_NAMESPACE
{
    /**
     * Transform node that accepts geospatial coordinates and creates
     * a local ENU (X=east, Y=north, Z=up) coordinate frame for its children
     * that is tangent to the earth at the transform's geo position.
     */
    class ROCKY_EXPORT GeoTransform : public vsg::Inherit<vsg::Group, GeoTransform>,
        public ROCKY_NAMESPACE::Transform
    {
    public:
        //! Sphere for horizon culling
        vsg::dsphere bound = { };

    public:
        //! Construct an invalid geotransform
        GeoTransform() = default;

        //! Call this is you change position directly.
        void dirty();

        //! Shortcut for changing position and calling dirty().
        inline void setPosition(const GeoPoint& p) {
            position = p;
            dirty();
        }

    public:

        //! Disables the copy constructor.
        GeoTransform(const GeoTransform& rhs) = delete;

        void traverse(vsg::RecordTraversal&) const override;

    private:

        mutable TransformDetail _transformDetail;
    };

} // namespace
