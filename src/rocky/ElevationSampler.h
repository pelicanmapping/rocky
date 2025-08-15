/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/ElevationLayer.h>

namespace ROCKY_NAMESPACE
{
    class ElevationSession;

    /**
    * A sample of elevation data.
    */
    using ElevationSample = float;

    /**
    * Queries an ElevationLayer for elevation values
    *
    * Usage:
    *   ElevationSampler sampler;
    *   sampler.layer = myElevationLayer; // required
    *
    *   auto sample = sampler.sample(GeoPoint(srs, x, y), io);
    *   if (sample.ok())
    *      // sample.value() contains the height.
    *
    * If you plan to sample collections of points in the same general area,
    * an ElevationSession may be faster:
    *
    *   auto session = sampler.session(io);
    *   session.srs = mySRS; // required, SRS of incoming points
    *   session.clampRange(points.begin(), points.end(); // clamps a range of points
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


        //! Construct a new query envelope.
        //! This is more efficient when you plan to query multiple points in a localized area.
        inline ElevationSession session(const IOOptions& io) const;


    public:
        const Failure NoLayer = Failure(Failure::ServiceUnavailable, "Elevation layer is not set or not open");

    private:
        //! Fetches a new heightfield for a key.
        Result<GeoHeightfield> fetch(const TileKey&, const IOOptions& io) const;
        friend class ElevationSession;
    };


    /**
    * Session for use with batch operations and for more control over sampling resolution.
    */
    class ROCKY_EXPORT ElevationSession
    {
    public:
        //! Default (invalid) constructor.
        //! Create a valid session by calling ElevationSampler::session().
        ElevationSession() = default;

        //! SRS of the incoming points
        SRS srs;

        //! Elevation level of detail to clamp to (use this OR resolution)
        unsigned lod = UINT_MAX;

        //! Resolution of elevation data to clamp to (use this OR lod)
        Distance resolution = Distance(10.0, Units::METERS);

        //! Reference latitude for resolution calculations (optional).
        Angle referenceLatitude = {};

        //! Clamps the incoming point to the elevation data.
        bool clamp(double& x, double& y, double& z) const;

        //! Samples the incoming point and returns the height.
        //! Failure will return the failValue.
        inline float sample(double x, double y, double z) const;

        //! Samples a point.
        template<class VEC3_ITER>
        inline bool clampRange(VEC3_ITER begin, VEC3_ITER end) const;

        //! Force a cache purge if you changed the lod or resolution.
        inline void dirty() {
            _pw = -1.0;
        }

        inline bool ok() const {
            return _sampler != nullptr;
        }
        inline operator bool() const {
            return ok();
        }

    private:
        friend class ElevationSampler;
        ElevationSession(const IOOptions& in_io) : _io(&in_io) {}
        const IOOptions* _io = nullptr;
        mutable double _pw = 0.0, _ph = 0.0, _pxmin = 0.0, _pymin = 0.0;
        mutable Profile::NumTiles _numtiles;
        mutable SRSOperation _xform;
        const ElevationSampler* _sampler = nullptr;

        // cache:
        struct {
            mutable std::uint32_t tx = UINT_MAX, ty = UINT_MAX;
            mutable TileKey key;
            mutable Status status;
            mutable GeoHeightfield hf;
        }
        _cache;

        friend class ElevationSampler;

        inline std::pair<std::uint32_t, std::uint32_t> tile(double x, double y) const {
            auto rx = (x - _pxmin) / _pw, ry = (y - _pymin) / _ph;
            auto out_tx = std::min((unsigned)(rx * (double)_numtiles.x), _numtiles.x - 1u);
            auto out_ty = std::min((unsigned)((1.0 - ry) * (double)_numtiles.y), _numtiles.y - 1u);
            return { out_tx, out_ty };
        }
    };



    // inlines ---

    auto ElevationSampler::session(const IOOptions& io) const
        -> ElevationSession
    {
        ElevationSession sesh(io);
        sesh._sampler = this;
        return sesh;
    }

    auto ElevationSampler::sample(const GeoPoint& p, const IOOptions& io) const
        -> Result<ElevationSample>
    {
        if (!layer || !layer->status().ok())
            return NoLayer;

        auto sesh = session(io);
        sesh.srs = p.srs;

        glm::dvec3 t(p);
        if (sesh.clamp(t.x, t.y, t.z))
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
        sesh.srs = p.srs;

        GeoPoint out(p);
        if (sesh.clamp(out.x, out.y, out.z))
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
        sesh.srs = p.srs;
        sesh.resolution = resolution;

        GeoPoint out(p);
        if (sesh.clamp(out.x, out.y, out.z))
            return out;
        else
            return Failure{};
    }

    template<class VEC3_ITER>
    Result<> ElevationSampler::clampRange(const SRS& srs, VEC3_ITER begin, VEC3_ITER end, const IOOptions& io) const
    {
        if (!layer || !layer->status().ok())
            return NoLayer;

        if (begin == end)
            return NoLayer;

        auto sesh = session(io);
        sesh.srs = srs;
        sesh.referenceLatitude = begin->y;
        return sesh.clampRange(begin, end);
    }

    float ElevationSession::sample(double x, double y, double z) const
    {
        double a = x, b = y, c = z;
        if (clamp(a, b, c))
            return static_cast<float>(c);
        else
            return _sampler->failValue;
    }

    template<class VEC3_ITER>
    bool ElevationSession::clampRange(VEC3_ITER begin, VEC3_ITER end) const
    {
        if (!_sampler->layer || !_sampler->layer->status().ok())
            return false;

        if (begin == end)
            return true;

        bool result = true;

        for (auto iter = begin; iter != end; ++iter)
        {
            if (clamp(iter->x, iter->y, iter->z))
                _xform.inverse(iter->x, iter->y, iter->z);
            else
                result = false;
        }

        return result;
    }
}
