/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/Common.h>
#include <rocky/vsg/ecs/TransformData.h>
#include <rocky/GeoPoint.h>
#include <rocky/Horizon.h>
#include <rocky/Utils.h>

namespace ROCKY_NAMESPACE
{
    template<class T>
    struct PositionedObjectAdapter : public PositionedObject
    {
        vsg::ref_ptr<T> object;
        const GeoPoint& objectPosition() const override {
            return object->objectPosition();
        }
        static std::shared_ptr<PositionedObjectAdapter<T>> create(vsg::ref_ptr<T> object_) {
            auto r = std::make_shared<PositionedObjectAdapter<T>>();
            r->object = object_;
            return r;
        }
    };

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
        GeoTransform();

        //! Call this is you change position directly.
        void dirty();

        //! Same as changing position and calling dirty().
        void setPosition(const GeoPoint& p);

        const GeoPoint& objectPosition() const {
            return position; // .peek();
        }

    public:

        //! Disables the copy constructor.
        GeoTransform(const GeoTransform& rhs) = delete;

        void traverse(vsg::RecordTraversal&) const override;

    public:

        mutable TransformData transformData;
    };

} // namespace
