/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#if 0
#include <rocky/Common.h>
#include <rocky/Profile.h>
#include <rocky/TileKey.h>
#include <rocky/Math.h>
//#include <rocky/Locators>

namespace ROCKY_NAMESPACE { namespace contrib
{
    using namespace ROCKY_NAMESPACE;

    /**
     * Utilities for working with cube and cubeface coordinates.
     */
    class ROCKY_EXPORT CubeUtils
    {
    public:
        /**
         * Converts lat/long into face coordinates. You can optionally supply a "face hint"
         * if you already know which face the result will be in. This is handy for resolving
         * border ambiguities (i.e. a lat/lon that falls on the border of two faces).
         */
        static bool latLonToFaceCoords(
            double lat_deg, double lon_deg,
            double& out_x, double& out_y, int& out_face,
            int faceHint = -1 );

        /**
         * Converts face coordinates into lat/long.
         */
        static bool faceCoordsToLatLon(
            double x, double y, int face,
            double& out_lat_deg, double& out_lon_deg );

        /**
         * Get the face # containing a TileKey.
         */
        static int getFace( const TileKey& key );
        
        /**
         * Converts cube coordinates (0,0=>6,1) to face coordinates (0,0=>1,1,F).
         * WARNING. If the cube coordinate lies on a face boundary, this method will
         * always return the lower-numbered face. The "extent" version of this 
         * method (below) is better b/c it's unambiguous.
         */
        static bool cubeToFace( 
            double& in_out_x, 
            double& in_out_y, 
            int&    out_face );

        /**
         * Converts cube coordinates (0,0=>6,1) to face coordinates (0,0=>1,1,F). This
         * version takes an extent, which is better than the non-extent version since it
         * can resolve face-border ambiguity.
         */
        static bool cubeToFace( 
            double& in_out_xmin, double& in_out_ymin, 
            double& in_out_xmax, double& in_out_ymax, 
            int&    out_face );

        /**
         * Converts face coordinates (0,0=>1,1 +F) to cube coordinates (0,0=>6,1).
         */
        static bool faceToCube(
            double& in_out_x, double& in_out_y, 
            int     face );
    };

    /**
     * The "Cube" SRS represents a 6-face cube, each face being in unit coordinates (0,0=>1,1).
     * the cube as whole lays out all six faces side by side, resulting in a space 
     * measuring (0,0=>6,1). The face number corresponds to the x-axis ordinal.
     */
    class ROCKY_EXPORT CubeSRS : public SRS
    {
    public:
        CubeSRS(
            const Key& key);

        // CUBE is a projected coordinate system.
        virtual bool isGeographic() const  override { return false; }
        virtual bool isProjected() const  override { return true; }

        // This SRS uses a WGS84 lat/long SRS under the hood for reprojection. So we need the
        // pre/post transforms to move from cube to latlong and back.

        virtual bool preTransform(
            std::vector<dvec3>& input,
            const SRS** new_srs) const override;

        virtual bool postTransform(
            std::vector<dvec3>& input,
            const SRS** new_srs) const override;

        virtual bool transformExtentToMBR(
            const SRS& to_srs,
            double& in_out_xmin,
            double& in_out_ymin,
            double& in_out_xmax,
            double& in_out_ymax) const override;

    private:

        bool transformInFaceExtentToMBR(
            const SRS& to_srs,
            int face,
            double& in_out_xmin,
            double& in_out_ymin,
            double& in_out_xmax,
            double& in_out_ymax) const;

        CubeSRS(const CubeSRS&) = delete;
    };

    /**
     * Custom profile for the unified cube tile layout.
     *
     * This is a whole-earth profile consisting of 6 cube faces. The first 4 faces
     * represent the equatorial regions between -45 and 45 degrees latitude. The lat
     * 2 faces represent the polar regions. 
     *
     * The face extents in lat/long are: (lat,lon min => lat,lon max)
     *
     *  Face 0 : (-180, -45 => -90, 45)
     *  Face 1 : (-90, -45 => 0, 45)
     *  Face 2 : (0, -45 => 90, 45)
     *  Face 3 : (90, -45 => 180, 45 )
     *  Face 4 : (-180, 45 => 180, 90)
     *  Face 5 : (-180, -90 => 180, -45)
     *
     * Each face was a local unit coordinate system of (0.0, 0.0 => 1.0, 1.0). The
     * profile lays the 6 faces out in a row, making a cube coordinate system
     * of (0.0, 0.0 => 6.0, 1.0).
     *
     * NOTE! This profile is non-contiguous and cannot be created as a single
     * rectangular domain.
     */
    class ROCKY_EXPORT UnifiedCubeProfile : 
        public Inherit<Profile, UnifiedCubeProfile>
    {
    public:
        UnifiedCubeProfile();

        virtual ~UnifiedCubeProfile();

    public: // utilities

        /**
         * Gets the cube face associated with a tile key (in cube srs).
         */
        static int getFace( const TileKey& key );

    public: // Profile

        virtual bool transformAndExtractContiguousExtents(
            const GeoExtent& input,
            std::vector<GeoExtent>& output) const override;

        //void getIntersectingTiles(
        //    const GeoExtent& extent,
        //    unsigned localLOD,
        //    std::vector< TileKey >& out_intersectingKeys ) const;

        unsigned getEquivalentLOD(const Profile* rhsProfile, unsigned rhsLOD) const;

    private:

        GeoExtent _faceExtent_gcs[6];
        
        GeoExtent transformGcsExtentOnFace( const GeoExtent& gcsExtent, int face ) const;
    };
} }
#endif