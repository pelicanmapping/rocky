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
    if (cache)
    {
        auto r = cache->get(key);
        if (r.has_value())
        {
            return r.value();
        }
    }

    // failing that, check the layer, and fall back to parent tiles if necessary.
    for (auto k = key; k.valid(); k.makeParent())
    {
        auto r = layer->createTile(k, io);
        if (r.ok())
        {
            if (cache)
                cache->put(key, r);
            return r;
        }
    }

    return Failure{};
}
