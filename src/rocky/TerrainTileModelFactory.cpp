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

TerrainTileModelFactory::TerrainTileModelFactory()
{
    //nop
}

TerrainTileModel
TerrainTileModelFactory::createTileModel(const Map* map, const TileKey& key, const IOOptions& io) const
{
    // Make a new model:
    TerrainTileModel model;
    model.key = key;
    model.revision = map->revision();

    // assemble all the components:
    addColorLayers(model, map, key, io, false);

    unsigned border = 0u;
    addElevation(model, map, key, io);

    return model;
}

namespace
{
    void addImageLayer(const TileKey& requested_key, std::shared_ptr<ImageLayer> layer, bool fallback, TerrainTileModel& model, const IOOptions& io)
    {
        Result<GeoImage> result;

        TileKey key = requested_key;
        if (fallback)
        {
            while(key.valid() && !result.ok())
            {
                result = layer->createImage(key, io);
                if (!result.ok())
                    key.makeParent();
            }
        }
        else
        {
            result = layer->createImage(key, io);
        }

        if (result.ok())
        {
            if (result.value().valid())
            {
                TerrainTileModel::ColorLayer m;
                m.layer = layer;
                m.revision = layer->revision();
                m.image = result.value();
                m.key = key;
                model.colorLayers.emplace_back(std::move(m));
                //if (layer->dynamic())
                //{
                //    model.requiresUpdate = true;
                //}
            }
            else
            {
                //... assert? result.value() should always be valid.
            }
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
}

void
TerrainTileModelFactory::addColorLayers(
    TerrainTileModel& model,
    const Map* map,
    const TileKey& key,
    const IOOptions& io,
    bool standalone) const
{
    int order = 0;

    // fetch the candidate layers:
    auto layers = map->layers<ImageLayer>([&](auto layer) { 
        return layer->status().ok(); } );

    // first collect the image layers that have intersecting data.
    std::vector<std::shared_ptr<ImageLayer>> intersecting_layers;
    bool inLegalRange = false;
    bool intersects = false;
    for (auto layer : layers)
    {
        auto imageLayer = ImageLayer::cast(layer);
        if (imageLayer)
        {
            inLegalRange = imageLayer->isKeyInLegalRange(key);
            intersects = imageLayer->intersects(key);

            if (imageLayer->isKeyInLegalRange(key) &&
                imageLayer->intersects(key))
            {
                intersecting_layers.push_back(imageLayer);
            }
        }
    }

    if (intersecting_layers.size() == 1 && intersecting_layers.front()->mayHaveData(key))
    {
        // if only one layer intersects we will not need to composite
        // so just get the raw data for this key if there is any.
        addImageLayer(key, intersecting_layers.front(), false, model, io);
    }

    else if (intersecting_layers.size() > 1)
    {
        // More than one layer intersects so we will need to composite.
        // First count the number of layers that MIGHT have data.
        // If any of them do, we must fetch them all for composition.
        bool data_maybe = false;
        for (auto layer : intersecting_layers)
        {
            if (layer->mayHaveData(key))
            {
                data_maybe = true;
                break;
            }
        }

        if (data_maybe)
        {
            for (auto layer : intersecting_layers)
            {
                addImageLayer(key, layer, true, model, io);
            }

            // now composite them.
            if (compositeColorLayers && model.colorLayers.size() > 1)
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

    if (layer != nullptr &&
        layer->isOpen() &&
        layer->isKeyInLegalRange(key) &&
        layer->mayHaveData(key))
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
