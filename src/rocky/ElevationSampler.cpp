/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#include "ElevationSampler.h"

using namespace ROCKY_NAMESPACE;

auto ElevationSampler::fetch(const TileKey& key, const IOOptions& io) const -> Result<GeoHeightfield>
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
        auto r = layer->createHeightfield(k, io);
        if (r.ok())
            return r;
    }

    return Failure{};
}

auto ElevationSampler::sample(Session& env, double x, double y, double z) const -> float
{
    if (env.pw <= 0.0)
    {
        auto& profile = layer->profile;

        if (env.lod == UINT_MAX)
        {
            double r = profile.srs().transformDistance(env.resolution, profile.srs().units(), env.referenceLatitude);
            env.lod = profile.levelOfDetailForHorizResolution(r, layer->tileSize);
        }

        env.pw = profile.extent().width();
        env.ph = profile.extent().height();
        env.pxmin = profile.extent().xmin();
        env.pymin = profile.extent().ymin();
        env.numtiles = profile.numTiles(env.lod);
    }

    // xform into the layer's SRS if necessary.
    env.xform.transform(x, y, z);
    
    // simple ONE TILE caching internally.
    auto [tx, ty] = env.tile(x, y);

    if (tx != env.tx || ty != env.ty)
    {
        env.key = layer->bestAvailableTileKey(TileKey(env.lod, tx, ty, layer->profile));
        if (env.key.valid())
        {
            env.hf = fetch(env.key, env.io);
            env.tx = tx, env.ty = ty;
        }
        else
        {
            // invalid data
            tx = UINT_MAX, ty = UINT_MAX;
            env.hf = Failure{};
        }
    }

    return env.hf.ok() ? env.hf->heightAtLocation(x, y, interpolation) : failValue;
}
