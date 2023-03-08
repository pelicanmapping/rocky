/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "LayerCollection.h"
#include "Map.h"

using namespace ROCKY_NAMESPACE;

LayerCollection::LayerCollection(Map* map) :
    _map(map)
{
    //nop
}

Status
LayerCollection::add(shared_ptr<Layer> layer)
{
    return add(layer, _map->_instance.ioOptions());
}

Status
LayerCollection::add(shared_ptr<Layer> layer, const IOOptions& io)
{
    if (indexOf(layer) != count())
        return layer->status();

    if (layer->getOpenAutomatically())
        layer->open();

    Revision new_revision;
    unsigned index = -1;
    {
        std::unique_lock lock(_map->_mapDataMutex);
        _layers.push_back(layer);
        index = _layers.size() - 1;
        new_revision = ++_map->_dataModelRevision;
    }

    _map->onLayerAdded.fire(layer, index, new_revision);
    return layer->status();
}

void
LayerCollection::remove(shared_ptr<Layer> layer)
{
    if (layer == nullptr)
        return;

    // ensure it's in the map
    if (indexOf(layer) == count())
        return;

    unsigned int index = -1;
    shared_ptr<Layer> layerToRemove(layer);
    Revision new_revision;

    // Close the layer if we opened it.
    if (layer->getOpenAutomatically())
    {
        layer->close();
    }

    if (layerToRemove)
    {
        std::unique_lock lock(_map->_mapDataMutex);
        index = 0;
        for (auto i = _layers.begin(); i != _layers.end(); ++i)
        {
            if (layer == layerToRemove)
            {
                _layers.erase(i);
                new_revision = ++_map->_dataModelRevision;
                break;
            }
        }
    }

    // a separate block b/c we don't need the mutex
    if (new_revision >= 0)
    {
        _map->onLayerRemoved.fire(layerToRemove, new_revision);
    }
}

void
LayerCollection::move(shared_ptr<Layer> layer, unsigned new_index)
{
    unsigned oldIndex = 0;
    unsigned actualIndex = 0;
    Revision newRevision;

    if (layer)
    {
        std::unique_lock lock(_map->_mapDataMutex);

        // find it:
        LayerVector::iterator i_oldIndex = _layers.end();

        for (LayerVector::iterator i = _layers.begin();
            i != _layers.end();
            i++, actualIndex++)
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

unsigned
LayerCollection::count() const
{
    return _layers.size();
}

unsigned
LayerCollection::indexOf(shared_ptr<Layer> layer) const
{
    if (!layer)
        return count();

    std::shared_lock lock(_map->_mapDataMutex);
    unsigned index = 0;
    for (; index < _layers.size(); ++index)
    {
        if (_layers[index] == layer)
            break;
    }
    return index;
}

shared_ptr<Layer>
LayerCollection::_at(unsigned index) const
{
    std::shared_lock lock(_map->_mapDataMutex);
    return index < _layers.size() ? _layers[index] : nullptr;
}

shared_ptr<Layer>
LayerCollection::_withName(const std::string& name) const
{
    std::shared_lock lock(_map->_mapDataMutex);
    for (auto layer : _layers)
        if (layer->name() == name)
            return layer;
    return nullptr;
}

shared_ptr<Layer>
LayerCollection::_withUID(const UID& uid) const
{
    std::shared_lock lock(_map->_mapDataMutex);
    for (auto layer : _layers)
        if (layer->uid() == uid)
            return layer;
    return nullptr;
}

std::vector<shared_ptr<Layer>>
LayerCollection::all() const
{
    std::shared_lock lock(_map->_mapDataMutex);
    return _layers;
}
