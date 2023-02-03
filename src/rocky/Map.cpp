/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Map.h"
#include "VisibleLayer.h"
#include <tuple>

using namespace ROCKY_NAMESPACE;

#define LC "[Map] "

//...................................................................

#if 0
Config
Map::Options::getConfig() const
{
    Config conf = ConfigOptions::getConfig();

    conf.set( "name",         name() );
    conf.set( "profile",      profile() );
    //conf.set( "cache",        cache() );
    //conf.set( "cache_policy", cachePolicy() );

    //conf.set( "elevation_interpolation", "nearest",     elevationInterpolation(), NEAREST);
    //conf.set( "elevation_interpolation", "average",     elevationInterpolation(), AVERAGE);
    //conf.set( "elevation_interpolation", "bilinear",    elevationInterpolation(), BILINEAR);
    //conf.set( "elevation_interpolation", "triangulate", elevationInterpolation(), TRIANGULATE);

    conf.set( "profile_layer", profileLayer() );

    return conf;
}

void
Map::Options::fromConfig(const Config& conf)
{
    //elevationInterpolation().init(BILINEAR);
    
    conf.get( "name",         name() );
    conf.get( "profile",      profile() );
    //conf.get( "cache",        cache() );  
    //conf.get( "cache_policy", cachePolicy() );

    // legacy support:
    //if ( conf.value<bool>( "cache_only", false ) == true )
    //    cachePolicy()->usage = CachePolicy::USAGE_CACHE_ONLY;

    //if ( conf.value<bool>( "cache_enabled", true ) == false )
    //    cachePolicy()->usage = CachePolicy::USAGE_NO_CACHE;

    //conf.get( "elevation_interpolation", "nearest",     elevationInterpolation(), NEAREST);
    //conf.get( "elevation_interpolation", "average",     elevationInterpolation(), AVERAGE);
    //conf.get( "elevation_interpolation", "bilinear",    elevationInterpolation(), BILINEAR);
    //conf.get( "elevation_interpolation", "triangulate", elevationInterpolation(), TRIANGULATE);

    conf.get( "profile_layer", profileLayer() );
}

//...................................................................
#endif

Map::Map() :
    _instance(Instance::create())
{
    construct(Config(), _instance->ioOptions());
}

Map::Map(Instance::ptr instance) :
    _instance(instance ? instance : Instance::create())
{
    construct(Config(), _instance->ioOptions());
}

Map::Map(Instance::ptr instance, const IOOptions& io) :
    _instance(instance ? instance : Instance::create())
{
    construct(Config(), io);
}

Map::Map(
    const Config& conf,
    Instance::ptr instance,
    const IOOptions& io) :

    _instance(instance ? instance : Instance::create())
{
    construct(Config(), io);
}

void
Map::construct(const Config& conf, const IOOptions& io)
{
    // reset the revision:
    _dataModelRevision = 0;

    _mapDataMutex.setName("Map dataMutex(OE)");

    // Generate a UID.
    _uid = rocky::createUID();

#if 0
    // elevation sampling
    _elevationPool = ElevationPool::create(_instance);
    _elevationPool->setMap( this );
#endif

    conf.get("name", _name);
    //conf.get("profile", _profile);

    //conf.set( "cache",        cache() );
    //conf.set( "cache_policy", cachePolicy() );

    //conf.set( "elevation_interpolation", "nearest",     elevationInterpolation(), NEAREST);
    //conf.set( "elevation_interpolation", "average",     elevationInterpolation(), AVERAGE);
    //conf.set( "elevation_interpolation", "bilinear",    elevationInterpolation(), BILINEAR);
    //conf.set( "elevation_interpolation", "triangulate", elevationInterpolation(), TRIANGULATE);

    conf.get("profile_layer", _profileLayer);

    if (conf.hasChild("profile"))
        setProfile(Profile(conf.child("profile"))); 

    // set a default profile if neccesary.
    if (!profile().valid())
    {
        setProfile(Profile::GLOBAL_GEODETIC);
    }
}

Config
Map::getConfig() const
{
    Config conf("map");
    conf.set("name", _name);

    if (profile().valid())
        conf.set("profile", profile().getConfig());

    //conf.set( "cache",        cache() );
    //conf.set( "cache_policy", cachePolicy() );

    //conf.set( "elevation_interpolation", "nearest",     elevationInterpolation(), NEAREST);
    //conf.set( "elevation_interpolation", "average",     elevationInterpolation(), AVERAGE);
    //conf.set( "elevation_interpolation", "bilinear",    elevationInterpolation(), BILINEAR);
    //conf.set( "elevation_interpolation", "triangulate", elevationInterpolation(), TRIANGULATE);

    conf.set("profile_layer", _profileLayer);

    return conf;
}


#if 0
ElevationPool*
Map::getElevationPool() const
{
    return _elevationPool.get();
}
#endif


Revision
Map::dataModelRevision() const
{
    return _dataModelRevision;
}

void
Map::setProfile(const Profile& value)
{
    bool notifyLayers = (!_profile.valid());

    if (value.valid())
    {
        _profile = value;
    }

#if 0
    // If we just set the profile, tell all our layers they are now added
    // to a valid map.
    if (_profile.valid() && notifyLayers)
    {
        for(auto& layer : _layers)
        {
            if (layer->isOpen())
            {
                layer->addedToMap(this);
            }
        }
    }
#endif
}

const Profile&
Map::profile() const
{
    return _profile;
}

std::set<std::string>
Map::attributions() const
{
    std::set<std::string> result;

    util::ScopedReadLock lock(_mapDataMutex);
    for(auto& layer : _layers)
    {
        if (layer->isOpen())
        {
            auto visibleLayer = VisibleLayer::cast(layer);

            if (!visibleLayer || visibleLayer->getVisible())
            {
                std::string attribution = layer->attribution();
                if (!attribution.empty())
                {
                    result.insert(attribution);
                }
            }
        }
    }
    
    return std::move(result);
}

void
Map::addLayer(shared_ptr<Layer> layer)
{
    addLayer(layer, _instance->ioOptions());
}

void
Map::addLayer(shared_ptr<Layer> layer, const IOOptions& io)
{
    if (layer == NULL)
        return;

    // ensure it's not already in the map
    if (indexOfLayer(layer.get()) != numLayers())
        return;

    if (layer->getOpenAutomatically())
    {
        layer->open();
    }

#if 0
    // do we need this? Won't the callback to this?
    if (layer->isOpen() && profile() != NULL)
    {
        layer->addedToMap(this);
    }
#endif

    // Add the layer to our stack.
    Revision newRevision;
    unsigned index = -1;
    {
        util::ScopedWriteLock lock( _mapDataMutex );

        _layers.push_back( layer );
        index = _layers.size() - 1;
        newRevision = ++_dataModelRevision;
    }

    onLayerAdded.fire(layer, index, newRevision);
}

void
Map::insertLayer(shared_ptr<Layer> layer, unsigned index)
{
    insertLayer(layer, index, _instance->ioOptions());
}

void
Map::insertLayer(
    shared_ptr<Layer> layer,
    unsigned index,
    const IOOptions& io)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(layer != nullptr, void());

    // ensure it's not already in the map
    if (indexOfLayer(layer.get()) != numLayers())
        return;

    if (layer->getOpenAutomatically())
    {
        layer->open(io);
    }

#if 0
    if (layer->isOpen() && profile() != NULL)
    {
        layer->addedToMap(this);
    }
#endif

    // Add the layer to our stack.
    int newRevision;
    {
        util::ScopedWriteLock lock(_mapDataMutex);

        if (index >= _layers.size())
            _layers.push_back(layer);
        else
            _layers.insert(_layers.begin() + index, layer);

        newRevision = ++_dataModelRevision;
    }

    onLayerAdded.fire(layer, index, newRevision);
}

void
Map::removeLayer(shared_ptr<Layer> layer)
{
    if (layer == NULL)
        return;

    // ensure it's in the map
    if (indexOfLayer(layer.get()) == numLayers())
        return;

    unsigned int index = -1;

    shared_ptr<Layer> layerToRemove(layer);
    Revision newRevision;

#if 0
    layer->removedFromMap(this);
#endif

    // Close the layer if we opened it.
    if (layer->getOpenAutomatically())
    {
        layer->close();
    }

    if ( layerToRemove.get() )
    {
        util::ScopedWriteLock lock( _mapDataMutex );
        index = 0;
        for(LayerVector::iterator i = _layers.begin(); i != _layers.end(); ++i)
        {
            if (layer == layerToRemove )
            {
                _layers.erase(i);
                newRevision = ++_dataModelRevision;
                break;
            }
        }
    }

    // a separate block b/c we don't need the mutex
    if ( newRevision >= 0 )
    {
        onLayerRemoved.fire(layerToRemove, newRevision);
    }
}

void
Map::moveLayer(shared_ptr<Layer> layerToMove, unsigned newIndex)
{
    unsigned int oldIndex = 0;
    unsigned int actualIndex = 0;
    Revision newRevision;

    if (layerToMove)
    {
        util::ScopedWriteLock lock( _mapDataMutex );

        // find it:
        LayerVector::iterator i_oldIndex = _layers.end();

        for (LayerVector::iterator i = _layers.begin();
            i != _layers.end();
            i++, actualIndex++)
        {
            if (*i == layerToMove)
            {
                i_oldIndex = i;
                oldIndex = actualIndex;
                break;
            }
        }

        if ( i_oldIndex == _layers.end() )
            return; // layer not found in list

        // erase the old one and insert the new one.
        _layers.erase(i_oldIndex);
        _layers.insert(_layers.begin() + newIndex, layerToMove);

        newRevision = ++_dataModelRevision;
    }

    // a separate block b/c we don't need the mutex
    if (layerToMove)
    {
        onLayerMoved.fire(layerToMove, oldIndex, newIndex, newRevision);
    }
}

void
Map::addLayers(const std::vector<Layer::ptr>& layers)
{
    addLayers(layers, _instance->ioOptions());
}

void
Map::addLayers(
    const std::vector<Layer::ptr>& layers,
    const IOOptions& io)
{
    // This differs from addLayer() in a loop because it will
    // (a) call addedToMap only after all the layers are added, and
    // (b) invoke all the MapModelChange callbacks with the same 
    // new revision number.

    //rocky::Registry::instance()->clearBlacklist();

    for(auto layer : layers)
    {
        if (layer && layer->getOpenAutomatically())
        {
            layer->open(io);
        }
    }

    unsigned firstIndex;
    unsigned count = 0;
    Revision newRevision;

    // Add the layers to the map.
    {
        util::ScopedWriteLock lock( _mapDataMutex );

        firstIndex = _layers.size();
        newRevision = ++_dataModelRevision;

        for (auto layer : layers)
        {
            if (layer)
                _layers.push_back(layer);
        }
    }

    // call addedToMap on each new layer in turn:
    unsigned index = firstIndex;

    for(auto layer : layers)
    {
        if (layer)
        {
#if 0
            if (layer->isOpen() && profile() != NULL)
            {
                layer->addedToMap(this);
            }
#endif

            // a separate block b/c we don't need the mutex
            onLayerAdded.fire(layer, index++, newRevision);
        }
    }
}

unsigned
Map::numLayers() const
{
    util::ScopedReadLock lock( _mapDataMutex );
    return _layers.size();
}

shared_ptr<Layer>
Map::getLayerByName(const std::string& name) const
{
    util::ScopedReadLock lock( _mapDataMutex );
    for (auto layer : _layers)
        if (layer->name() == name)
            return layer;
    return nullptr;
}

shared_ptr<Layer>
Map::getLayerByUID(UID uid) const
{
    util::ScopedReadLock lock( _mapDataMutex );
    for (auto layer : _layers)
        if (layer->uid() == uid)
            return layer;
    return nullptr;
}

shared_ptr<Layer>
Map::getLayerAt(unsigned index) const
{
    util::ScopedReadLock lock( _mapDataMutex );
    if (index >= 0 && index < (unsigned)_layers.size())
        return _layers[index];
    else
        return 0L;
}

unsigned
Map::indexOfLayer(const Layer* layer) const
{
    if (!layer)
        return -1;

    util::ScopedReadLock lock( _mapDataMutex );
    unsigned index = 0;
    for (; index < _layers.size(); ++index)
    {
        if (_layers[index].get() == layer)
            break;
    }
    return index;
}

void
Map::clear()
{
    std::vector<Layer::ptr> layersRemoved;
    Revision newRevision;
    {
        util::ScopedWriteLock lock( _mapDataMutex );

        layersRemoved.swap( _layers );

        // calculate a new revision.
        newRevision = ++_dataModelRevision;
    }

    // a separate block b/c we don't need the mutex
    for (auto layer : layersRemoved)
    {
        onLayerRemoved.fire(layer, newRevision);
    }
}

const SRS&
Map::srs() const
{
    static SRS emptySRS;
    return _profile.valid() ? _profile.srs() : emptySRS;
}

void
Map::removeCallback(UID uid)
{
    onLayerAdded.functions.erase(uid);
    onLayerRemoved.functions.erase(uid);
    onLayerMoved.functions.erase(uid);
}
