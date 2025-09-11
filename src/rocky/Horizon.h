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

        //! Sets the eye position to use when testing for occlusion.
        //! @param eyeECEF eye point in world geocentric coordinates
        //! @param isOrtho true if the camera is using an orthographic projection
        //! Returns true if the value changed; false if it was the same
        bool setEye(const glm::dvec3& eyeECEF, bool isOrtho = false);
        const glm::dvec3& getEye() const { return _eye; }

        //! Whether a point is visible over the horizon.
        //! @param point Point in geocetric coordinates
        //! @param radius Optional radius (meters) around the point
        //! @return true if the point is visible
        bool isVisible(double x, double y, double z, double radius = 0.0) const;

        template<typename T>
        bool isVisible(const T& vec3, double radius = 0.0) const {
            return isVisible(vec3.x, vec3.y, vec3.z, radius);
        }

        //! Caclulate distance from eye to visible horizon
        //! @param Distance (meters) to the visible horizon
        double getDistanceToVisibleHorizon() const;

        //! Gets the radius of the ellipsoid under the eye
        //! @return Radius in meters of the ellipsoid
        double getRadius() const;

        //! Whether this object has been initialized with a valid ellipsoid
        operator bool() const { return _valid; }

    protected:
        Ellipsoid _em;
        bool _valid = false;
        bool _orthographic = false; // assume orthographic projection
        glm::dvec3 _eye;       // world eyepoint
        glm::dvec3 _eyeUnit;   // unit eye vector (scaled)
        glm::dvec3 _VC;        // eye->center vector (scaled)
        double _VCmag = 0.0;    // distance from eye to center (scaled)
        double _VCmag2 = 0.0;   // distance from eye to center squared (scaled)
        double _VHmag2 = 0.0;   // distance from eye to horizon squared (scaled)
        double _coneCos = 0.0;  // cosine of half-cone
        double _coneTan = 0.0;  // tangent of half-cone
        glm::dvec3 _scale;     // transforms from world to unit space
        glm::dvec3 _scaleInv;  // transforms from unit to world space
        double _minHAE = 0.0;;   // minumum height above ellipsoid to test
        double _minVCmag = 0.0; // derived from minHAE
    };
}
