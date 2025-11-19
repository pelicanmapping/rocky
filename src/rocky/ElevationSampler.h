/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/ElevationLayer.h>
#include <array>

namespace ROCKY_NAMESPACE
{
    class ElevationSession;

    /**
    * A sample of elevation data.
    */
    struct ElevationSample
    {
        float height = rocky::NO_DATA_VALUE;
    };

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

        //! Value to return when no data is available at the requested coordinates
        float failValue = NO_DATA_VALUE;

        //! Optional cache for fetching elevation tiles
        std::shared_ptr<Cache<TileKey, Result<GeoImage>>> cache;

    public:

        //! Is this sampler OK to use?
        inline bool ok() const {
            return layer && layer->status().ok();
        }

        //! Compute the height at the given coordinates
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
        Result<GeoImage> fetch(const TileKey&, const IOOptions& io) const;
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
        unsigned level = UINT_MAX;

        //! Resolution of elevation data to clamp to (use this OR lod)
        Distance resolution = Distance(10.0, Units::METERS);

        //! Reference latitude for resolution calculations (optional).
        Angle referenceLatitude = {};

        //! Clamps a range of points. All points are expected to be in the "srs" SRS.
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
            mutable GeoImage hf;
        }
        _cache;

        friend class ElevationSampler;

        inline std::pair<std::uint32_t, std::uint32_t> tile(double x, double y) const {
            auto rx = (x - _pxmin) / _pw, ry = (y - _pymin) / _ph;
            auto out_tx = std::min((unsigned)(rx * (double)_numtiles.x), _numtiles.x - 1u);
            auto out_ty = std::min((unsigned)((1.0 - ry) * (double)_numtiles.y), _numtiles.y - 1u);
            return { out_tx, out_ty };
        }

        template<typename VEC3_ITER>
        inline bool clampTransformedRange(VEC3_ITER begin, VEC3_ITER end) const;
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
        std::array<glm::dvec3, 1> range = { { p } };
        if (sesh.clampRange(range.begin(), range.end()))
        {
            if (!p.srs.isGeodetic())
                p.srs.to(p.srs.geodeticSRS()).transform(range[0].x, range[0].y, range[0].z);

            return ElevationSample{ (float)range[0].z };
        }
        else
        {
            return Failure{};
        }
    }

    auto ElevationSampler::clamp(const GeoPoint& p, const IOOptions& io) const
        -> Result<GeoPoint>
    {
        return clamp(p, Distance{}, io);
    }

    auto ElevationSampler::clamp(const GeoPoint& p, const Distance& resolution, const IOOptions& io) const
        -> Result<GeoPoint>
    {
        if (!layer || !layer->status().ok())
            return NoLayer;

        auto sesh = session(io);
        sesh.srs = p.srs;
        sesh.resolution = resolution;

        std::array<glm::dvec3, 1> range = { { p } };
        if (sesh.clampRange(range.begin(), range.end()))
            return GeoPoint(p.srs, range[0]);
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

    template<class VEC3_ITER>
    bool ElevationSession::clampRange(VEC3_ITER begin, VEC3_ITER end) const
    {
        if (!_sampler->layer || !_sampler->layer->status().ok())
            return false;

        if (begin == end)
            return true;

        if (_xform.from() != srs)
        {
            _xform = srs.to(_sampler->layer->profile.srs());
        }

        _xform.transformRange(begin, end);

        bool result = clampTransformedRange(begin, end);

        if (result)
            _xform.inverseRange(begin, end);

        return result;
    }

    template<class VEC3_ITER>
    bool ElevationSession::clampTransformedRange(VEC3_ITER begin, VEC3_ITER end) const
    {
        for (auto iter = begin; iter != end; ++iter)
        {
            if (_pw <= 0.0)
            {
                auto& profile = _sampler->layer->profile;

                if (level == UINT_MAX)
                {
                    double r = profile.srs().transformDistance(resolution, profile.srs().units(), referenceLatitude);
                    const_cast<ElevationSession*>(this)->level = profile.levelOfDetailForHorizResolution(r, _sampler->layer->tileSize);
                }

                _pw = profile.extent().width();
                _ph = profile.extent().height();
                _pxmin = profile.extent().xmin();
                _pymin = profile.extent().ymin();
                _numtiles = profile.numTiles(level);
                _cache.tx = UINT_MAX;
                _cache.ty = UINT_MAX;
                _cache.status = Failure{};
            }

            auto& x = iter->x;
            auto& y = iter->y;
            auto& z = iter->z;

            // simple ONE TILE caching internally.
            auto [new_tx, new_ty] = tile(x, y);

            if (new_tx != _cache.tx || _cache.ty != new_ty)
            {
                _cache.status.clear();

                _cache.key = _sampler->layer->bestAvailableTileKey(TileKey(level, new_tx, new_ty, _sampler->layer->profile));
                if (_cache.key.valid())
                {
                    auto r = _sampler->fetch(_cache.key, *_io);
                    if (r.ok())
                        _cache.hf = std::move(r.value());
                    else
                        _cache.status = r.error();

                    _cache.tx = new_tx, _cache.ty = new_ty;
                }
                else
                {
                    // invalid data
                    _cache.tx = UINT_MAX, _cache.ty = UINT_MAX;
                    _cache.status = Failure{};
                }
            }

            if (_cache.status.ok())
            {
                auto r = GeoHeightfield(_cache.hf).read(x, y);
                if (r.ok())
                    z = r.value();
                else
                    return false;
            }
            else
            {
                return false;
            }
        }

        return true;
    }
}
