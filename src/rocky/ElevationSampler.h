/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/ElevationLayer.h>

namespace ROCKY_NAMESPACE
{
    /**
    * A sample of elevation data, which includes the height and the resolution
    */
    struct ElevationSample
    {
        Distance height;
        Distance resolution;
    };

    /**
    * Queries an ElevationLayer for elevation values
    *
    * Usage:
    *   ElevationSampler sampler;
    *   sampler.layer = myElevationLayer; // required
    *
    *   auto sample = sampler.sample(GeoPoint(x, y, srs), io);
    *   if (sample.ok())
    *      // sample.height contains the height.
    *
    */
    class ROCKY_EXPORT ElevationSampler
    {
    public:
        //! Layer from which to query elevations (required)
        ElevationLayer::Ptr layer;

        //! Interpolation method to use when sampling the elevation layer
        Interpolation interpolation = Interpolation::Bilinear;

        //! Value to return when no data is available at the requested coordinates
        float failValue = NO_DATA_VALUE;

        //! Source of heightfields (like a cache) to check before querying the layer (optional).
        std::function<Result<GeoHeightfield>(const TileKey&, const IOOptions&)> preFetch;

    public:

        //! Is this sampler OK to use?
        inline bool ok() const {
            return layer && layer->status().ok();
        }

        //! Compute the height and resolution at the given coordinates
        inline Result<ElevationSample> sample(const GeoPoint& p, const IOOptions& io) const;

        //! Compute the height and resolution at the given coordinates, which are in the ElevationLayer's profile SRS.
        inline Result<ElevationSample> sample(double x, double y, double z, const IOOptions& io) const;

        //! Sample a vector of 3D points, which are in the ElevationLayer's profile SRS, and replace
        //! each one's Z value with the computed height.
        template<class VEC3_ITER>
        inline void sampleRange(VEC3_ITER begin, VEC3_ITER end, const IOOptions& io) const;

        class Session
        {
        public:
            SRSOperation xform;
            unsigned lod = UINT_MAX;
            Distance resolution = Distance(10.0, Units::METERS);
            Angle referenceLatitude = {};

            inline void dirty() {
                pw = -1.0;
            }

        private:
            Session(const IOOptions& in_io) : io(in_io) {}
            const IOOptions& io;
            double pw = 0.0, ph = 0.0, pxmin = 0.0, pymin = 0.0;
            Profile::NumTiles numtiles;
            ElevationSampler* sampler;

            // cache:
            std::uint32_t tx = UINT_MAX, ty = UINT_MAX;
            TileKey key;
            Result<GeoHeightfield> hf;
            friend class ElevationSampler;

            inline std::pair<std::uint32_t, std::uint32_t> tile(double x, double y) const {
                auto rx = (x - pxmin) / pw, ry = (y - pymin) / ph;
                auto tx = std::min((unsigned)(rx * (double)numtiles.x), numtiles.x - 1u);
                auto ty = std::min((unsigned)((1.0 - ry) * (double)numtiles.y), numtiles.y - 1u);
                return { tx, ty };
            }
        };

        //! Construct a new query envelope.
        //! The optional "y" value is a latitude that will help determine the appropriate sampling resolution.
        inline Session session(const IOOptions& io) const;

        //! Samples a point.
        float sample(Session& env, double x, double y, double z = 0.0) const;

        //! Samples a point.
        template<class VEC3_ITER>
        void sampleRange(Session& env, VEC3_ITER begin, VEC3_ITER end) const;

        //! Fetches a new heightfield for a key.
        Result<GeoHeightfield> fetch(const TileKey&, const IOOptions& io) const;

    public:
        const Failure NoLayer = Failure(Failure::ServiceUnavailable, "Elevation layer is not set or not open");
    };




    // inlines ---

    auto ElevationSampler::session(const IOOptions& io) const
        -> ElevationSampler::Session
    {
        return Session(io);
    }

    auto ElevationSampler::sample(double x, double y, double z, const IOOptions& io) const
        -> Result<ElevationSample>
    {
        if (!layer || !layer->status().ok())
            return NoLayer;

        auto env = session(io);
        auto height = sample(env, x, y, z);
        if (height == failValue)
            return Failure_ResourceUnavailable;

        ElevationSample r;
        r.height.set(height, Units::METERS);
        if (env.hf.ok()) {
            r.resolution.set(layer->profile.srs().transformDistance(
                Distance(env.hf->resolution().x, env.hf->srs().units()), Units::METERS, y),
                Units::METERS);
        }
        else {
            r.resolution.set(0, Units::METERS);
        }
        return r;
    }

    auto ElevationSampler::sample(const GeoPoint& p, const IOOptions& io) const
        -> Result<ElevationSample>
    {
        if (!layer || !layer->status().ok())
            return NoLayer;

        if (p.srs == layer->profile.srs()) {
            return sample(p.x, p.y, p.z, io);
        }
        else {
            GeoPoint pt(p);
            pt.transformInPlace(layer->profile.srs());
            return sample(pt.x, pt.y, pt.z, io);
        }
    }

    template<class VEC3_ITER>
    void ElevationSampler::sampleRange(VEC3_ITER begin, VEC3_ITER end, const IOOptions& io) const
    {
        if (!layer || !layer->status().ok())
            return NoLayer;

        if (begin == end)
            return;

        auto env = prepare(io, begin->y);
        sampleRange(env, begin, end);
    }

    template<class VEC3_ITER>
    void ElevationSampler::sampleRange(Session& env, VEC3_ITER begin, VEC3_ITER end) const
    {
        if (!layer || !layer->status().ok())
            return;

        if (begin == end)
            return;

        for (auto iter = begin; iter != end; ++iter)
        {
            iter->z = sample(env, iter->x, iter->y);
        }
    }
}
