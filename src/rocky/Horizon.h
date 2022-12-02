/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/Ellipsoid.h>
#include <rocky/Math.h>

namespace rocky
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
        Horizon(const Ellipsoid& ellipsoid);

        //! Ellipsoid model to use for occlusion testing
        void setEllipsoid(const Ellipsoid& ellipsoid);

        //! Set the minimum allowable height-above-ellipsoid to consider when doing
        //! horizon visibility testing.
        void setMinHAE(double meters);
        double getMinHAE() const { return _minHAE; }

        //! Sets the eye position to use when testing for occlusion.
        //!Returns true if the value changed; false if it was the same
        bool setEye(const dvec3& eyeECEF);
        const dvec3& getEye() const { return _eye; }

        /**
         * Whether a point is visible over the horizon.
         * Specify an optional radius to test a sphere.
         */
        bool isVisible(const dvec3& point, double radius = 0.0) const;

        /**
         * Whether "target" with bounding radius "radius" is visible from "eye".
         * This function does not require you to call setEye.
         */
        bool isVisible(const dvec3& eye, const dvec3& target, double radius) const;

        /**
         * Whether a bounding sphere is visible over the horizon.
         */
        bool isVisible(const Sphere& bs) const {
            return isVisible(bs.center, bs.radius);
        }

        /**
         * Whether the horizon occludes a point/sphere.
         */
        bool occludes(const dvec3& point, double radius = 0.0) const {
            return !isVisible(point, radius);
        }

        /**
         * Sets the output variable to the horizon plane plane with its
         * normal pointing at the eye.
         */
         //bool getPlane(osg::Plane& out_plane) const;

         //! Caclulate distance from eye to visible horizon
        double getDistanceToVisibleHorizon() const;

        /**
         * Gets the radius of the ellipsoid under the eye
         */
        double getRadius() const;

    protected:

        Ellipsoid _em;

        bool _valid;
        dvec3 _eye;
        dvec3 _eyeUnit;
        dvec3 _VC;
        double _VCmag;    // distance from eye to center (scaled)
        double _VCmag2;   // distance from eye to center squared (scaled)
        double _VHmag2;   // distance from eye to horizon squared (scaled)
        double _coneCos;  // cosine of half-cone
        double _coneTan;  // tangent of half-cone

        dvec3 _scale;
        dvec3 _scaleInv;
        double _minVCmag;
        double _minHAE;
    };
}
