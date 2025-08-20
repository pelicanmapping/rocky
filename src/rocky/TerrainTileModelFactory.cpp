/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#include "TerrainTileModelFactory.h"
#include "Map.h"
#include "ElevationLayer.h"
#include "ImageLayer.h"

#define LC "[TerrainTileModelFactory] "

using namespace ROCKY_NAMESPACE;

namespace
{
    void replace_nodata_values(GeoHeightfield& geohf)
    {
        auto grid = geohf.heightfield();
        if (grid)
        {
            for (unsigned col = 0; col < grid->height(); ++col)
            {
                for (unsigned row = 0; row < grid->width(); ++row)
                {
                    if (grid->heightAt(col, row) == NO_DATA_VALUE)
                    {
                        grid->heightAt(col, row) = 0.0f;
                    }
                }
            }
        }
    }
}

//.........................................................................


TerrainTileModel
TerrainTileModelFactory::createTileModel(const Map* map, const TileKey& key, const IOOptions& io) const
{
    // Make a new model:
    TerrainTileModel model;
    model.key = key;
    model.revision = map->revision();

    // assemble all the components:
    addColorLayers(model, map, key, io);

    unsigned border = 0u;
    addElevation(model, map, key, io);

    return model;
}

namespace
{
    // return: true if fallback occurred, false if not.
    bool addImageLayer(const TileKey& startingKey, std::shared_ptr<ImageLayer> layer, bool fallback, TerrainTileModel& model, const IOOptions& io)
    {
        GeoImage geoimage;
        Status status;
        bool fell_back = false;

        TileKey key = startingKey;
        if (fallback)
        {
            while(key.valid() && !geoimage.valid())
            {
                auto r = layer->createImage(key, io);
                if (r.ok())
                {
                    geoimage = r.release();
                }
                else
                {
                    status = r.error();
                    key.makeParent();
                    fell_back = true;
                }
            }
        }
        else
        {
            auto r = layer->createImage(key, io);
            if (r.ok())
                geoimage = r.release();
            else
                status = r.error();
        }

        if (geoimage.valid())
        {
            TerrainTileModel::ColorLayer m;
            m.layer = layer;
            m.revision = layer->revision();
            m.image = std::move(geoimage);
            m.key = key;
            model.colorLayers.emplace_back(std::move(m));
            //if (layer->dynamic())
            //{
            //    model.requiresUpdate = true;
            //}
        }

        // ResourceUnavailable just means the driver could not produce data
        // for the tilekey; it is not an actual read error.
        else if (status.failed())
        {
            if (status.error().type != Failure::ResourceUnavailable &&
                status.error().type != Failure::OperationCanceled)
            {
                Log()->warn("Problem getting data from \"" + layer->name + "\" : " + status.error().string());
            }
        }

        return fell_back;
    }
}

void
TerrainTileModelFactory::addColorLayers(TerrainTileModel& model, const Map* map, const TileKey& key, const IOOptions& io) const
{
    struct Candidate {
        ImageLayer::Ptr layer;
        TileKey key;
    };

    int order = 0;

    // fetch the candidate layers:
    auto layers = map->layers<ImageLayer>([&](auto layer) { 
        return layer->status().ok(); } );

    // first collect the image layers that have intersecting data.
    std::vector<Candidate> candidates;
    bool mayHaveData = false;

    for (auto layer : layers)
    {
#if 1
        auto bestKey = layer->bestAvailableTileKey(key);
        if (bestKey.valid())
        {
            candidates.emplace_back(Candidate{ layer, bestKey });
            mayHaveData = mayHaveData || bestKey == key;
        }
#else
        if (layer->intersects(key))
        {
            candidates.push_back(Candidate{ layer, key });
            mayHaveData = true;
        }
#endif
    }

    if (mayHaveData)
    {
        constexpr bool no_fallback = false;
        constexpr bool yes_fallback = true;

        if (candidates.size() == 1)
        {
            // if only one layer intersects we will not need to composite
            // so just get the raw data for this key if there is any.
            addImageLayer(candidates.front().key, candidates.front().layer, no_fallback, model, io);
        }

        else if (candidates.size() > 1)
        {
            unsigned num_fallbacks = 0;

            for (auto c : candidates)
            {
                if (addImageLayer(c.key, c.layer, yes_fallback, model, io))
                {
                    ++num_fallbacks;
                }
            }

            // now composite them (unless ALL tiles were fallbacks)
            if (compositeColorLayers && num_fallbacks < candidates.size() && model.colorLayers.size() > 1)
            {
                auto& base_image = model.colorLayers.front().image;
                TerrainTileModel::Tile tile = model.colorLayers.front();

                auto comp_image = Image::create(
                    Image::R8G8B8A8_UNORM,
                    base_image.image()->width(),
                    base_image.image()->height());

                comp_image->fill(glm::fvec4(0, 0, 0, 0));

                GeoImage image(comp_image, key.extent());
                std::vector<GeoImage> sources;
                std::vector<float> opacities;
                for (auto& i : model.colorLayers)
                {
                    sources.emplace_back(std::move(i.image));
                    auto* imagelayer = dynamic_cast<const ImageLayer*>(i.layer.get());
                    opacities.emplace_back(imagelayer ? imagelayer->opacity.value() : 1.0f);
                }

                image.composite(sources, opacities);

                TerrainTileModel::ColorLayer layer;
                layer.key = key;
                layer.revision = tile.revision;
                layer.matrix = tile.matrix;
                layer.image = image;

                model.colorLayers.clear();
                model.colorLayers.emplace_back(std::move(layer));
            }

            else if (model.colorLayers.size() == 1 && model.colorLayers.front().key != key)
            {
                // on the off chance that we fell back on a layer, and it ended up
                // being the only layer, throw it out.
                model.colorLayers.clear();
            }
        }
    }
}


bool
TerrainTileModelFactory::addElevation(TerrainTileModel& model, const Map* map, const TileKey& key, const IOOptions& io) const
{
    auto layers = map->layers<ElevationLayer>([&](auto layer) {
        return layer->status().ok(); });

    if (layers.empty())
        return false;

    auto layer = layers.front();

    int combinedRevision = map->revision();

    auto bestKey = layer->bestAvailableTileKey(key);

    if (bestKey == key)
    {
        auto result = layer->createHeightfield(key, io);

        if (result.ok())
        {
            replace_nodata_values(result.value());

            model.elevation.heightfield = std::move(result.value());
            model.elevation.revision = layer->revision();
            model.elevation.key = key;
        }

        // ResourceUnavailable just means the driver could not produce data
        // for the tilekey; it is not an actual read error.
        else if (
            result.error().type != Failure::ResourceUnavailable &&
            result.error().type != Failure::OperationCanceled)
        {
            Log()->warn("Problem getting data from \"" + layer->name + "\" : " + result.error().string());
        }
    }

    return model.elevation.heightfield.valid();
}
