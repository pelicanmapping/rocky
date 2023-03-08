/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TerrainTileModelFactory.h"
#include "Map.h"
#include "Metrics.h"
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

CreateTileManifest::CreateTileManifest()
{
    _includesElevation = false;
    _includesConstraints = false;
    _progressive.set_default(false);
}

void
CreateTileManifest::insert(shared_ptr<Layer> layer)
{
    if (layer)
    {
        _layers[layer->uid()] = layer->revision();

        if (std::dynamic_pointer_cast<ElevationLayer>(layer))
        {
            _includesElevation = true;
        }

        //else if (std::dynamic_pointer_cast<TerrainConstraintLayer>(layer))
        //{
        //    _includesConstraints = true;
        //}
    }
}

bool
CreateTileManifest::excludes(const Layer* layer) const
{
    return !empty() && _layers.find(layer->uid()) == _layers.end();
}

bool
CreateTileManifest::empty() const
{
    return _layers.empty();
}

bool
CreateTileManifest::inSyncWith(const Map* map) const
{
#if 0
    for(auto& iter : _layers)
    {
        auto layer = map->getLayerByUID(iter.first);

        // note: if the layer is null, it was removed, so let it pass.
        if (layer && layer->revision() != iter.second)
        {
            return false;
        }
    }
#endif
    return true;
}

void
CreateTileManifest::updateRevisions(const Map* map)
{
#if 0
    for (auto& iter : _layers)
    {
        auto layer = map->getLayerByUID(iter.first);
        if (layer)
        {
            iter.second = layer->revision();
        }
    }
#endif
}

bool
CreateTileManifest::includes(const Layer* layer) const
{
    return includes(layer->uid());
}

bool
CreateTileManifest::includes(UID uid) const
{
    return empty() || _layers.find(uid) != _layers.end();
}

bool
CreateTileManifest::includesElevation() const
{
    return empty() || _includesElevation;
}

bool
CreateTileManifest::includesConstraints() const
{
    return _includesConstraints;
}

void
CreateTileManifest::setProgressive(bool value)
{
    _progressive = value;
}

//.........................................................................

TerrainTileModelFactory::TerrainTileModelFactory()
{
    //nop
}

TerrainTileModel
TerrainTileModelFactory::createTileModel(
    const Map* map,
    const TileKey& key,
    const CreateTileManifest& manifest,
    const IOOptions& io)
{
    ROCKY_PROFILING_ZONE;

    // Make a new model:
    TerrainTileModel model;
    model.key = key;
    model.revision = map->revision();

    // assemble all the components:
    addColorLayers(model, map, key, manifest, io, false);

    unsigned border = 0u;
    addElevation(model, map, key, manifest, border, io);

    return std::move(model);
}

TerrainTileModel
TerrainTileModelFactory::createStandaloneTileModel(
    const Map* map,
    const TileKey& key,
    const CreateTileManifest& manifest,
    const IOOptions& io)
{
    ROCKY_PROFILING_ZONE;

    // Make a new model:
    TerrainTileModel model;
    model.key = key;
    model.revision = map->revision();

    // assemble all the components:
    addColorLayers(model, map, key, manifest, io, true);

    addStandaloneElevation(model, map, key, manifest, 0u, io);

    return std::move(model);
}

bool
TerrainTileModelFactory::addImageLayer(
    TerrainTileModel& model,
    shared_ptr<const ImageLayer> imageLayer,
    const TileKey& key,
    const IOOptions& io)
{
    ROCKY_PROFILING_ZONE;
    ROCKY_PROFILING_ZONE_TEXT(imageLayer->getName());

    if (imageLayer->isOpen() &&
        imageLayer->isKeyInLegalRange(key) &&
        imageLayer->mayHaveData(key))
    {
        auto result = imageLayer->createImage(key, io);
        if (result.value.valid())
        {
            TerrainTileModel::ColorLayer m;
            m.layer = imageLayer;
            m.revision = imageLayer->revision();
            m.image = result.value;
            model.colorLayers.emplace_back(std::move(m));

            if (imageLayer->isDynamic() || imageLayer->getAsyncLoading())
            {
                model.requiresUpdate = true;
            }

            return true;
        }

        // ResourceUnavailable just means the driver could not produce data
        // for the tilekey; it is not an actual read error.
        else if (result.status.failed() && result.status.code != Status::ResourceUnavailable)
        {
            Log::warn() << "Problem getting data from \"" << imageLayer->name() << "\" : "
                << result.status.message << std::endl;
        }
    }
    return false;
}

void
TerrainTileModelFactory::addStandaloneImageLayer(
    TerrainTileModel& model,
    shared_ptr<const ImageLayer> imageLayer,
    const TileKey& key,
    const IOOptions& io)
{
    TileKey key_to_use = key;

    bool added = false;
    while (key_to_use.valid() & !added)
    {
        if (addImageLayer(model, imageLayer, key_to_use, io))
            added = true;
        else
            key_to_use.makeParent();
    }
}

void
TerrainTileModelFactory::addColorLayers(
    TerrainTileModel& model,
    const Map* map,
    const TileKey& key,
    const CreateTileManifest& manifest,
    const IOOptions& io,
    bool standalone)
{
    ROCKY_PROFILING_ZONE;

    int order = 0;

    // fetch the candidate layers:
    auto layers = map->layers().get([&manifest](const shared_ptr<Layer>& layer)
        {
            return
                layer->isOpen() &&
                layer->renderType() == layer->RENDERTYPE_TERRAIN_SURFACE &&
                !manifest.excludes(layer.get());
        });

    for(auto layer : layers)
    {
        auto imageLayer = ImageLayer::cast(layer);
        if (imageLayer)
        {
            if (standalone)
            {
                addStandaloneImageLayer(model, imageLayer, key, io);
            }
            else
            {
                addImageLayer(model, imageLayer, key, io);
            }
        }
        else // non-image kind of TILE layer (e.g., splatting)
        {
            TerrainTileModel::ColorLayer colorModel;
            colorModel.layer = layer;
            colorModel.revision = layer->revision();
            model.colorLayers.push_back(std::move(colorModel));
        }
    }
}

bool
TerrainTileModelFactory::addElevation(
    TerrainTileModel& model,
    const Map* map,
    const TileKey& key,
    const CreateTileManifest& manifest,
    unsigned border,
    const IOOptions& io)
{
    ROCKY_PROFILING_ZONE;
    ROCKY_PROFILING_ZONE_TEXT("Elevation");

    bool needElevation = manifest.includesElevation();

    auto layers = map->layers().all();

    if (layers.empty())
        return false;

    int combinedRevision = map->revision();
    if (!manifest.empty())
    {
        for (const auto& layer : layers)
        {
            if (needElevation == false && !manifest.excludes(layer.get()))
            {
                needElevation = true;
            }
            combinedRevision += layer->revision();
        }
    }
    if (!needElevation)
        return false;

    auto layer = map->layers().firstOfType<ElevationLayer>();

    if (layer->isOpen() &&
        layer->isKeyInLegalRange(key) &&
        layer->mayHaveData(key))
    {
        auto result = layer->createHeightfield(key, io);

        if (result.status.ok())
        {
            replace_nodata_values(result.value);

            model.elevation.heightfield = std::move(result.value);
            model.elevation.revision = layer->revision();
        }

        // ResourceUnavailable just means the driver could not produce data
        // for the tilekey; it is not an actual read error.
        else if (result.status.code != Status::ResourceUnavailable)
        {
            Log::warn() << "Problem getting data from \"" << layer->name() << "\" : "
                << result.status.message << std::endl;
        }
    }

    return model.elevation.heightfield.valid();
}

bool
TerrainTileModelFactory::addStandaloneElevation(
    TerrainTileModel& model,
    const Map* map,
    const TileKey& key,
    const CreateTileManifest& manifest,
    unsigned border,
    const IOOptions& io)
{
    TileKey key_to_use = key;

    bool added = false;
    while (key_to_use.valid() & !added)
    {
        if (addElevation(model, map, key_to_use, manifest, border, io))
            added = true;
        else
            key_to_use.makeParent();
    }

    return added;
}
