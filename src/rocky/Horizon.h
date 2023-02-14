/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/Ellipsoid.h>
#include <rocky/Math.h>

namespace ROCKY_NAMESPACE
{
    /**
     * Horizon operations (for a geocentric map).
     */
    class ROCKY_EXPORT Horizon
    {
    public:
        //! Construct a horizon using a default WGS84 ellipsoid model
        Horizon();

        //! Construct a horizon providing the ellipsoid model
        //! @param ellipsoid Ellipsoid that defines the horizon
        Horizon(const Ellipsoid& ellipsoid);

        //! Ellipsoid model to use for occlusion testing
        //! @param ellipsoid Ellipsoid that defines the horizon
        void setEllipsoid(const Ellipsoid& ellipsoid);

        //! Set the minimum allowable height-above-ellipsoid to consider when doing
        //! horizon visibility testing.
        void setMinHAE(double meters);
        double getMinHAE() const { return _minHAE; }

        //! Sets the eye position to use when testing for occlusion.
        //! @param eyeECEF eye point in world geocentric coordinates
        //! Returns true if the value changed; false if it was the same
        bool setEye(const dvec3& eyeECEF);
        const dvec3& getEye() const { return _eye; }

        //! Whether a point is visible over the horizon.
        //! @param point Point in geocetric coordinates
        //! @param radius Optional radius (meters) around the point
        //! @return true if the point is visible
        bool isVisible(const dvec3& point, double radius = 0.0) const;

        //! Sets the output variable to the horizon plane plane with its
        //! normal pointing at the eye.
        // bool getPlane(osg::Plane& out_plane) const;

        //! Caclulate distance from eye to visible horizon
        //! @param Distance (meters) to the visible horizon
        double getDistanceToVisibleHorizon() const;

        //! Gets the radius of the ellipsoid under the eye
        //! @return Radius in meters of the ellipsoid
        double getRadius() const;

    protected:
        Ellipsoid _em;
        bool _valid;
        dvec3 _eye;       // world eyepoint
        dvec3 _eyeUnit;   // unit eye vector (scaled)
        dvec3 _VC;        // eye->center vector (scaled)
        double _VCmag;    // distance from eye to center (scaled)
        double _VCmag2;   // distance from eye to center squared (scaled)
        double _VHmag2;   // distance from eye to horizon squared (scaled)
        double _coneCos;  // cosine of half-cone
        double _coneTan;  // tangent of half-cone
        dvec3 _scale;     // transforms from world to unit space
        dvec3 _scaleInv;  // transforms from unit to world space
        double _minHAE;   // minumum height above ellipsoid to test
        double _minVCmag; // derived from minHAE
    };
}
