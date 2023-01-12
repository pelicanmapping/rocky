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

namespace rocky
{
    /**
     * Local Tangent Plane SRS.
     * Please call SRS::createLTP() to construct one of these.
     */
    class ROCKY_EXPORT TangentPlaneSRS : public SRS
    {
    public:
        //! New LTP SRS
        //! @param key Key of underlying geographic SRS
        //! @param originLLA lat/long/alt origin of reference point
        TangentPlaneSRS(
            const Key& key, 
            const dvec3& originLLA);

        TangentPlaneSRS(const TangentPlaneSRS&) = delete;

    public: // SRS

        bool isGeographic() const override { return false; }
        bool isProjected() const override { return true; }
        bool isLTP() const override { return true; }

        // This SRS uses a WGS84 lat/long SRS under the hood for reprojection.
        // So we need the pre/post transforms to move from LTP to LLA and back.

        virtual bool preTransform(
            std::vector<dvec3>& input,
            const SRS** new_srs) const override;

        virtual bool postTransform(
            std::vector<dvec3>& input,
            const SRS** new_srs) const override;

    protected: // SRS
        
        bool _isEquivalentTo(
            const SRS* srs,
            bool considerVDatum ) const override;

    private:

        dvec3 _originLLA;
        dmat4 _local2world{ 1 }, _world2local{ 1 };

    };

}
#endif
