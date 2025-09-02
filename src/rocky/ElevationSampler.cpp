/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#include "ElevationSampler.h"

using namespace ROCKY_NAMESPACE;

auto ElevationSampler::fetch(const TileKey& key, const IOOptions& io) const -> Result<GeoImage>
{
    // check the cache first.
    if (preFetch)
    {
        auto r = preFetch(key, io);
        if (r.ok())
        {
            return r;
        }
    }

    // failing that, check the layer, and fall back to parent tiles if necessary.
    for (auto k = key; k.valid(); k.makeParent())
    {
        auto r = layer->createTile(k, io);
        if (r.ok())
            return r;
    }

    return Failure{};
}

bool ElevationSession::transformAndClamp(double& x, double& y, double& z) const
{
    if (_xform.from() != srs)
    {
        _xform = srs.to(_sampler->layer->profile.srs());
        _pw = -1.0;
    }

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

    // xform into the layer's SRS if necessary.
    double xa = x, ya = y, za = z;
    _xform.transform(xa, ya, za);
    
    // simple ONE TILE caching internally.
    auto [new_tx, new_ty] = tile(xa, ya);

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
        auto r = _cache.hf.read(xa, ya);
        if (r.ok())
        {
            x = xa, y = ya, z = r.value().r;
            return true;
        }
    }
    
    return false;
}