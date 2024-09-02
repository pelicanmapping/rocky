/**
 * rocky c++
 * Copyright 2024 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/GeoPoint.h>

namespace ROCKY_NAMESPACE
{
    /**
     * A georeferenced 3D point with terrain-relative altitude
     */
    class ROCKY_EXPORT EntityPosition
    {
    public:
        GeoPoint basePosition;
        double altitude;

        //! Constructs an empty (and invalid) EntityPosition.
        EntityPosition();

        //! Constructs a EntityPosition
        EntityPosition(const GeoPoint& srs, double altitude);

        //! Destruct
        ~EntityPosition() { }

        //! Transforms this EntityPosition into another SRS and puts the
        //! output in the "output"
        //! @return true upon success, false upon failure
        bool transform(const SRS& outSRS, EntityPosition& output) const;

        //! Transforms this point in place to another SRS
        bool transformInPlace(const SRS& srs);

        bool operator == (const EntityPosition& rhs) const {
            return basePosition == rhs.basePosition && altitude == rhs.altitude;
        }

        bool operator != (const EntityPosition& rhs) const {
            return !operator==(rhs);
        }

        //! Does this object contain a valid geo point?
        bool valid() const {
            return basePosition.valid();
        }

    public:
        static EntityPosition INVALID;

        // copy/move ops
        EntityPosition(const EntityPosition& rhs) = default;
        EntityPosition& operator=(const EntityPosition& rhs) = default;
        EntityPosition(EntityPosition&& rhs) { *this = rhs; }
        EntityPosition& operator=(EntityPosition&& rhs);
    };


    /**
    * Base class for any object that has a position on a Map.
    */
    class TerrainRelativePositionedObject
    {
    public:
        //! Center position of the object
        virtual const EntityPosition& objectPosition() const = 0;
    };

}
