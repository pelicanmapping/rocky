/** rocky c++; Copyright 2026 Pelican Mapping; MIT License */
#pragma once
#include <rocky/Feature.h>
#include <rocky/Profile.h>

namespace ROCKY_NAMESPACE
{
    //! Bounded XYZ/TMS feature access. Configure before querying; raw bytes use shared URI services.
    class ROCKY_EXPORT TiledFeatureSource : public Inherit<FeatureSource, TiledFeatureSource>
    {
    public:
        std::vector<URI> tiles;
        std::string format = "FlatGeobuf";
        Profile profile = Profile("spherical-mercator");
        SRS dataSRS;
        unsigned minZoom = 0, maxZoom = 14;
        bool tms = false;
        std::string sourceLayer;
        bool wholeFeatures = false;
        std::string idField;
        std::size_t decodedCacheBytes = 128u * 1024u * 1024u;

        //! Initializes an independent decoded cache, retained until source destruction/reload.
        TiledFeatureSource();
        //! Releases immutable decoded batches after active users finish.
        ~TiledFeatureSource();
        //! Checks addressing and decoder configuration without performing I/O.
        Result<> validate() const;
        //! Collects overlapping source tiles with bounded fan-out and exact source-core ownership metadata.
        Result<std::shared_ptr<const Batch>> query(const Query&, const IOOptions&) override;
        //! Hierarchical sources have no single feature count.
        int featureCount() const override { return -1; }
        //! Full iteration is empty; use spatial queries for hierarchical data.
        FeatureSource::iterator iterate(const IOOptions&) override;

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;
    };
}
