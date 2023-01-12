/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/GeoPoint.h>

namespace rocky
{
    class GeoExtent;

    /**
     * A simple circular bounding area consiting of a GeoPoint and a linear radius.
     */
     class ROCKY_EXPORT GeoCircle
     {
     public:
         /** Construct an INVALID GeoCircle */
        GeoCircle();

        /** Copy another GoeCircle */
        GeoCircle(const GeoCircle& rhs);

        /** Construct a new GeoCircle */
        GeoCircle(
             const GeoPoint& center,
             double          radius );

        virtual ~GeoCircle() { }

        /** The center point of the circle */
        const GeoPoint& center() const { return _center; }
        void setCenter( const GeoPoint& value ) { _center = value; }

        /** Circle's radius, in linear map units (or meters for a geographic SRS) */
        double radius() const { return _radius; }
        void setRadius( double value ) { _radius = value; }

        /** SRS of the center point */
        const SRS& getSRS() const { return _center.getSRS(); }

        /** equality test */
        bool operator == ( const GeoCircle& rhs ) const;

        /** inequality test */
        bool operator != ( const GeoCircle& rhs ) const { return !operator==(rhs); }

        /** validity test */
        bool valid() const { return _center.valid() && _radius > 0.0; }

        /** transform the GeoCircle to another SRS */
        GeoCircle transform(const SRS& srs) const;

        /** transform the GeoCircle to another SRS */
        bool transform(const SRS& srs, GeoCircle& out_circle) const;

        /** does this GeoCircle intersect another? */
        bool intersects(const GeoCircle& rhs) const;

     public:

         static GeoCircle INVALID;

     protected:
         GeoPoint _center;
         double   _radius;
     };
}
