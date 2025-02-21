/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/Common.h>
#include <rocky/vsg/ViewLocal.h>
#include <rocky/GeoPoint.h>
#include <rocky/Horizon.h>
#include <rocky/Utils.h>
#include <vsg/nodes/CullGroup.h>
#include <vsg/nodes/Transform.h>
#include <array>

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
    class ROCKY_EXPORT GeoTransform : public vsg::Inherit<vsg::Group, GeoTransform>
    {
    public:
        //util::ring_buffer<GeoPoint> position{ 2, true };
        GeoPoint position;

        //! Sphere for horizon culling
        vsg::dsphere bound = { };

        //! whether horizon culling is active
        bool horizonCulling = true;

        //! whether frustum culling is active
        bool frustumCulling = true;

        //! Whether the transformation should establish a local tangent plane (ENU)
        //! at the position. Disabling this can increase performance for objects
        //! (like billboards) that don't need tangent plane.
        bool localTangentPlane = true;

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

        bool push(vsg::RecordTraversal&, bool cull, const std::optional<vsg::dmat4>& localMatrix = {}) const;

        void pop(vsg::RecordTraversal&) const;

    public:
        struct ViewLocalData
        {
            bool dirty = true;
            vsg::dmat4 local;     // local rotation/offset
            vsg::dmat4 model;     // model matrix
            vsg::mat4 proj;      // projection matrix
            vsg::mat4 modelview; // modelview matrix
            vsg::vec4 viewport;
            SRS world_srs;
            bool culled = false;
            const Ellipsoid* world_ellipsoid = nullptr;
            SRSOperation pos_to_world;
            std::shared_ptr<Horizon> horizon;
        };

        mutable detail::ViewLocal<ViewLocalData> viewLocal;
    };
} // namespace
