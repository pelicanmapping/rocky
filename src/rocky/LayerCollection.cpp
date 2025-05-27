/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "LayerCollection.h"
#include "Map.h"
#include "ImageLayer.h" // temp
#include "ElevationLayer.h" // temp

using namespace ROCKY_NAMESPACE;

LayerCollection::LayerCollection(Map* map) :
    _map(map),
    _map_mutex(map->_mapDataMutex)
{
    //nop
}

Status
LayerCollection::add(std::shared_ptr<Layer> layer)
{
    // check if it's already in the collection
    if (indexOf(layer) != size())
    {
        return layer->status();
    }

    // insert the new layer into the map safely
    Revision new_revision;
    int index = -1;
    {
        std::unique_lock lock(_map->_mapDataMutex);
        _layers.push_back(layer);
        index = (int)_layers.size() - 1;
        new_revision = ++_map->_dataModelRevision;
    }

    // invoke the callback
    _map->onLayerAdded.fire(layer, index, new_revision);

    // return the layer's open status
    return openOnAdd ? layer->status() : Status_OK;
}

void
LayerCollection::remove(std::shared_ptr<Layer> layer)
{
    if (layer == nullptr)
        return;

    // ensure it's in the map
    if (indexOf(layer) == size())
        return;

    unsigned int index = -1;
    std::shared_ptr<Layer> layerToRemove(layer);
    Revision new_revision = -1;

    // Close the layer when we remove it from the map.
    if (closeOnRemove && layer->openAutomatically)
    {
        layer->close();
    }

    if (layerToRemove)
    {
        std::unique_lock lock(_map->_mapDataMutex);
        index = 0;
        for (auto i = _layers.begin(); i != _layers.end(); ++i)
        {
            if (*i == layerToRemove)
            {
                _layers.erase(i);
                new_revision = ++_map->_dataModelRevision;
                break;
            }
        }
    }

    // a separate block b/c we don't need the mutex
    if (layerToRemove && new_revision >= 0)
    {
        _map->onLayerRemoved.fire(layerToRemove, new_revision);
    }
}

void
LayerCollection::move(std::shared_ptr<Layer> layer, unsigned new_index)
{
    unsigned oldIndex = 0;
    unsigned actualIndex = 0;
    Revision newRevision = -1;

    if (layer)
    {
        std::unique_lock lock(_map->_mapDataMutex);

        // find it:
        auto i_oldIndex = _layers.end();

        for (auto i = _layers.begin(); i != _layers.end(); i++, actualIndex++)
        {
            if (*i == layer)
            {
                i_oldIndex = i;
                oldIndex = actualIndex;
                break;
            }
        }

        if (i_oldIndex == _layers.end())
            return; // layer not found in list

        // erase the old one and insert the new one.
        _layers.erase(i_oldIndex);
        _layers.insert(_layers.begin() + new_index, layer);

        newRevision = ++_map->_dataModelRevision;
    }

    // a separate block b/c we don't need the mutex
    if (layer)
    {
        _map->onLayerMoved.fire(layer, oldIndex, new_index, newRevision);
    }
}

std::size_t
LayerCollection::size() const
{
    return _layers.size();
}

unsigned
LayerCollection::indexOf(std::shared_ptr<Layer> layer) const
{
    if (!layer)
        return (unsigned)size();

    std::shared_lock lock(_map->_mapDataMutex);
    unsigned index = 0;
    for (; index < _layers.size(); ++index)
    {
        if (_layers[index] == layer)
            break;
    }
    return index;
}

std::shared_ptr<Layer>
LayerCollection::_at(unsigned index) const
{
    std::shared_lock lock(_map->_mapDataMutex);
    return index < _layers.size() ? _layers[index] : nullptr;
}

std::shared_ptr<Layer>
LayerCollection::_withName(const std::string& name) const
{
    std::shared_lock lock(_map->_mapDataMutex);
    for (auto layer : _layers)
        if (layer->name() == name)
            return layer;
    return nullptr;
}

std::shared_ptr<Layer>
LayerCollection::_withUID(const UID& uid) const
{
    std::shared_lock lock(_map->_mapDataMutex);
    for (auto layer : _layers)
        if (layer->uid() == uid)
            return layer;
    return nullptr;
}

std::vector<std::shared_ptr<Layer>>
LayerCollection::all() const
{
    std::shared_lock lock(_map->_mapDataMutex);
    return _layers;
}
