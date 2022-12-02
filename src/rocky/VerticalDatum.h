/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/Heightfield.h>
#include <rocky/GeoExtent.h>
#include <rocky/Units.h>

namespace rocky
{
    class Geoid;

    /** 
     * Reference information for vertical (height) information.
     */
    class ROCKY_EXPORT VerticalDatum :
        public Inherit<Object, VerticalDatum>
    {
    public:
        //! Creates an vertical datum from an initialization string. This method
        //! uses an internal cache so that there is only ever one instance or each
        //! unique vertical datum.
        static shared_ptr<VerticalDatum> get(const std::string& init);


    public: // static transform methods

        //! Transforms a Z coordinate from one vertical datum to another.
        static bool transform(
            const VerticalDatum* from,
            const VerticalDatum* to,
            double lat_deg,
            double lon_deg,
            double& in_out_z );

        //! Transforms a Z coordinate from one vertical datum to another.
        static bool transform(
            const VerticalDatum* from,
            const VerticalDatum* to,
            double lat_deg,
            double lon_deg,
            float& in_out_z );

        //! Transforms the values in a height field from one vertical datum to another.
        static bool transform(
            const VerticalDatum* from,
            const VerticalDatum* to,
            const GeoExtent& hf_extent,
            shared_ptr<Heightfield> hf);

    public: // raw transformations

        //! Converts an MSL value (height relative to a mean sea level model) to the
        //! corresponding HAE value (height above the model's reference ellipsoid)
        virtual double msl2hae( double lat_deg, double lon_deg, double msl ) const;

        //! Converts an HAE value (height above the model's reference ellipsoid) to the
        //! corresponding MSL value (height relative to a mean sea level model)
        virtual double hae2msl(double lat_deg, double lon_deg, double hae) const;


    public: // properties

        /** Gets the readable name of this SRS. */
        const std::string& getName() const { return _name; }

        /** Gets the underlying geoid */
        shared_ptr<Geoid> getGeoid() const { return _geoid; }

        /** Tests this SRS for equivalence with another. */
        virtual bool isEquivalentTo( const VerticalDatum* rhs ) const;
        
    public:

        /** Creates a geoid-based VSRS. */
        VerticalDatum(
            const std::string& name,
            shared_ptr<Geoid> geoid);


    private:

        std::string _name;
        shared_ptr<Geoid> _geoid;
    };
}
