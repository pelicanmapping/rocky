/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky_vsg/Common.h>
#include <rocky_vsg/ViewLocal.h>
#include <rocky/GeoPoint.h>
#include <vsg/nodes/Group.h>
#include <vsg/nodes/Transform.h>

namespace ROCKY_NAMESPACE
{
    /**
     * Transform node that accepts geospatial coordinates.
     */
    class ROCKY_VSG_EXPORT GeoTransform : public vsg::Inherit<vsg::Group, GeoTransform>
    {
    public:
        //! Construct an invalid geotransform
        GeoTransform();

        GeoTransform(const GeoTransform& rhs) = delete;

        //! Geospatial position
        void setPosition(const GeoPoint& p);

        //! Geospatial position
        const GeoPoint& position() const;

    public:

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

} // namespace


