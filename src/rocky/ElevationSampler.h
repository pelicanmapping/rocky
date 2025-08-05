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
    using ElevationSample = float;

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

        //! Clamps the incoming point to the elevation data.
        inline Result<GeoPoint> clamp(const GeoPoint& p, const IOOptions& io) const;

        //! Clamps the incoming point to the elevation data.
        inline Result<GeoPoint> clamp(const GeoPoint& p, const Distance& resolution, const IOOptions& io) const;

        //! Sample a vector of 3D points and replace each one with the elevation-clamped version.
        template<class VEC3_ITER>
        inline Result<> clampRange(const SRS& srs, VEC3_ITER begin, VEC3_ITER end, const IOOptions& io) const;

        /**
        * Session for use with batch operations and for more control over sampling resolution.
        */
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
            Status status;
            GeoHeightfield hf;
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

        //! Clamps the incoming point to the elevation data.
        bool clamp(Session& env, double& x, double& y, double& z) const;

        //! Samples the incoming point and returns the height.
        inline float sample(Session& env, double x, double y, double z) const;

        //! Samples a point.
        template<class VEC3_ITER>
        inline Result<> clampRange(Session& env, VEC3_ITER begin, VEC3_ITER end) const;

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

    auto ElevationSampler::sample(const GeoPoint& p, const IOOptions& io) const
        -> Result<ElevationSample>
    {
        if (!layer || !layer->status().ok())
            return NoLayer;

        auto sesh = session(io);
        sesh.xform = p.srs.to(layer->profile.srs());

        glm::dvec3 t(p);
        if (clamp(sesh, t.x, t.y, t.z))
            return t.z;
        else
            return Failure{};
    }

    auto ElevationSampler::clamp(const GeoPoint& p, const IOOptions& io) const
        -> Result<GeoPoint>
    {
        if (!layer || !layer->status().ok())
            return NoLayer;

        auto sesh = session(io);
        sesh.xform = p.srs.to(layer->profile.srs());

        GeoPoint out(p);
        if (clamp(sesh, out.x, out.y, out.z))
            return out;
        else
            return Failure{};
    }

    auto ElevationSampler::clamp(const GeoPoint& p, const Distance& resolution, const IOOptions& io) const
        -> Result<GeoPoint>
    {
        if (!layer || !layer->status().ok())
            return NoLayer;

        auto sesh = session(io);
        sesh.xform = p.srs.to(layer->profile.srs());
        sesh.resolution = resolution;

        GeoPoint out(p);
        if (clamp(sesh, out.x, out.y, out.z))
            return out;
        else
            return Failure{};
    }

    float ElevationSampler::sample(Session& env, double x, double y, double z) const
    {
        double a = x, b = y, c = z;
        if (clamp(env, a, b, c))
            return static_cast<float>(c);
        else
            return failValue;
    }

    template<class VEC3_ITER>
    Result<> ElevationSampler::clampRange(const SRS& srs, VEC3_ITER begin, VEC3_ITER end, const IOOptions& io) const
    {
        if (!layer || !layer->status().ok())
            return NoLayer;

        if (begin == end)
            return NoLayer;

        auto env = session(io, begin->y);
        env.xform = srs.to(layer->profile.srs());
        return clampRange(env, begin, end);
    }

    template<class VEC3_ITER>
    Result<> ElevationSampler::clampRange(Session& env, VEC3_ITER begin, VEC3_ITER end) const
    {
        if (!layer || !layer->status().ok())
            return NoLayer;

        if (begin == end)
            return NoLayer;

        for (auto iter = begin; iter != end; ++iter)
        {
            if (clamp(env, iter->x, iter->y, iter->z))
                env.xform.inverse(iter->x, iter->y, iter->z);
        }

        return ResultVoidOK; // ok
    }
}
