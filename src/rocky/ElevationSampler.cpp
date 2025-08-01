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

bool ElevationSampler::clamp(Session& env, double& x, double& y, double& z) const
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
    double xa = x, ya = y, za = z;
    env.xform.transform(xa, ya, za);
    
    // simple ONE TILE caching internally.
    auto [tx, ty] = env.tile(xa, ya);

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

    if (env.hf.ok())
    {
        x = xa, y = ya;
        z = env.hf->heightAtLocation(xa, ya, interpolation);
        return true;
    }
    else
    {
        return false;
    }
}
