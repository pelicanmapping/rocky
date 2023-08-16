/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky_vsg/Common.h>
#include <rocky_vsg/engine/ViewLocal.h>
#include <rocky/GeoPoint.h>
#include <vsg/nodes/CullGroup.h>
#include <vsg/nodes/Transform.h>

namespace ROCKY_NAMESPACE
{
    /**
     * Transform node that accepts geospatial coordinates and creates
     * a local ENU (X=east, Y=north, Z=up) coordinate frame for its children
     * that is tangent to the earth at the transform's geo position.
     */
    class ROCKY_VSG_EXPORT GeoTransform :
        public vsg::Inherit<vsg::Group, GeoTransform>,
        PositionedObject
    {
    public:
        //! Construct an invalid geotransform
        GeoTransform();

        //! Geospatial position
        void setPosition(const GeoPoint& p);

        //! Geospatial position
        const GeoPoint& position() const override;

    public:

        GeoTransform(const GeoTransform& rhs) = delete;

        void accept(vsg::RecordTraversal&) const override;

    protected:

        GeoPoint _position;

        struct Data {
            bool dirty = true;
            GeoPoint worldPos;
            vsg::dmat4 matrix;
        };
        util::ViewLocal<Data> _viewlocal;

    };


    /**
    * Group that uses the matrix set by a GeoTransform to
    * frustum-cull against a bounding sphere, and then cull against
    * the visible horizon.
    */
    class ROCKY_VSG_EXPORT HorizonCullGroup : public vsg::Inherit<vsg::Group, HorizonCullGroup>
    {
    public:
        //! bounding sphere for frustum culling
        vsg::dsphere bound;

        void accept(vsg::RecordTraversal& rv) const override;
    };

} // namespace


