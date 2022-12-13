/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Map.h"
#include "Notify.h"
#include "VisibleLayer.h"
#include <tuple>

using namespace rocky;

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
    construct(Config(), _instance->ioOptions);
}

Map::Map(Instance::ptr instance) :
    _instance(instance ? instance : Instance::create())
{
    construct(Config(), _instance->ioOptions);
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

    // set the object name from the options:
    //if (options().name().has_value())
    //    setName(options().name().get());
    //else
    //    setName("rocky.Map");

    // Generate a UID.
    _uid = rocky::createUID();

    // Set up the map's profile
    //if (options().profile().has_value())
    //    setProfile(Profile::create(options().profile().get()));

    //if (getProfile() == nullptr)
    //    setProfile(Profile::create(Profile::GLOBAL_GEODETIC));

#if 0
    // If the registry doesn't have a default cache policy, but the
    // map options has one, make the map policy the default.
    if (options().cachePolicy().has_value() &&
        !_instance.cachePolicy().has_value())
    {
        _instance.cachePolicy() = options().cachePolicy().get();
        ROCKY_INFO << LC
            << "Setting default cache policy from map ("
            << options().cachePolicy()->usageString() << ")" << std::endl;
    }

    // put the CacheSettings object in there. We will propogate this throughout
    // the data model and the renderer. These will be stored in the readOptions
    // (and ONLY there)
    _cacheSettings = CacheSettings::create();

    // Set up a cache if there's one in the options:
    if (options().cache().has_value())
        _cacheSettings->setCache(CacheFactory::create(options().cache().get()));

    // Otherwise use the registry default cache if there is one:
    if (_cacheSettings->getCache() == nullptr)
        _cacheSettings->setCache(_instance.getDefaultCache());

    // Integrate local cache policy (which can be overridden by the environment)
    _cacheSettings->integrateCachePolicy(options().cachePolicy());

    // store in the options so we can propagate it to layers, etc.
    _cacheSettings->store(_readOptions.get());
    ROCKY_INFO << LC << _cacheSettings->toString() << "\n";

    // remember the referrer for relative-path resolution:
    URIContext( options().referrer() ).store( _readOptions.get() );
#endif

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
        setProfile(Profile::create(conf.child("profile"))); 

    // set a default profile if neccesary.
    if (!getProfile())
    {
        setProfile(Profile::create(Profile::GLOBAL_GEODETIC));
    }
}

Config
Map::getConfig() const
{
    Config conf("map");
    conf.set("name", _name);

    if (getProfile())
        conf.set("profile", getProfile()->getConfig());

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

#if 0
void
Map::notifyOnLayerOpenOrClose(Layer* layer)
{
    // bump the revision safely:
    Revision newRevision;
    {
        util::ScopedWriteLock lock(_mapDataMutex);
        newRevision = ++_dataModelRevision;
    }

    if (layer->isOpen())
    {
        if (getProfile())
        {
            layer->addedToMap(this);
        }
    }
    else
    {
        layer->removedFromMap(this);
    }

    MapModelChange change(
        layer->isOpen() ? MapModelChange::OPEN_LAYER : MapModelChange::CLOSE_LAYER,
        newRevision,
        layer);

    for( MapCallbackList::iterator i = _mapCallbacks.begin(); i != _mapCallbacks.end(); i++ )
    {
        i->get()->onMapModelChanged(change);
    }
}
#endif

Revision
Map::getDataModelRevision() const
{
    return _dataModelRevision;
}

void
Map::setProfile(shared_ptr<Profile> value)
{
    bool notifyLayers = (_profile == nullptr);

    if (value)
    {
        _profile = value;

        // create a "proxy" profile to use when querying elevation layers with a vertical datum
        if (_profile && _profile->getSRS()->getVerticalDatum() != 0L )
        {
            Config conf = _profile->getConfig();
            conf.remove("vdatum");
            _profileNoVDatum = Profile::create(conf);
            //ProfileOptions po = _profile->toProfileOptions();
            //po.vsrsString().unset();
            //_profileNoVDatum = Profile::create(po);
        }
        else
        {
            _profileNoVDatum = _profile;
        }

        // finally, fire an event if the profile has been set.
        ROCKY_INFO << LC << "Map profile is: " << _profile->toString() << std::endl;
    }

    // If we just set the profile, tell all our layers they are now added
    // to a valid map.
    if (_profile && notifyLayers)
    {
        for(auto& layer : _layers)
        {
            if (layer->isOpen())
            {
                layer->addedToMap(this);
            }
        }
    }
}

shared_ptr<Profile>
Map::getProfile() const
{
    return _profile;
}

#if 0
void
Map::setElevationInterpolation(const RasterInterpolation& value)
{
    options().elevationInterpolation() = value;
}

const RasterInterpolation&
Map::getElevationInterpolation() const
{
    return options().elevationInterpolation().get();
}
#endif

#if 0
Cache*
Map::getCache() const
{
    CacheSettings* cacheSettings = CacheSettings::get(_readOptions.get());
    return cacheSettings ? cacheSettings->getCache() : 0L;
}

void
Map::setCache(Cache* cache)
{
    // note- probably unsafe to do this after initializing the terrain. so don't.
    CacheSettings* cacheSettings = CacheSettings::get(_readOptions.get());
    if (cacheSettings && cacheSettings->getCache() != cache)
        cacheSettings->setCache(cache);
}
#endif

void
Map::getAttributions(std::set<std::string>& attributions) const
{
    util::ScopedReadLock lock(_mapDataMutex);
    for(auto& layer : _layers)
    {
        if (layer->isOpen())
        {
            auto visibleLayer = VisibleLayer::cast(layer);

            //auto visibleLayer = layer->as<VisibleLayer>();
            if (!visibleLayer || visibleLayer->getVisible())
            {
                std::string attribution = layer->getAttribution();
                if (!attribution.empty())
                {
                    attributions.insert(attribution);
                }
            }
        }
    }
}

#if 0
MapCallback*
Map::addMapCallback(MapCallback* cb) const
{
    if ( cb )
        _mapCallbacks.push_back( cb );
    return cb;
}

void
Map::removeMapCallback(MapCallback* cb) const
{
    MapCallbackList::iterator i = std::find( _mapCallbacks.begin(), _mapCallbacks.end(), cb);
    if (i != _mapCallbacks.end())
    {
        _mapCallbacks.erase( i );
    }
}

void
Map::beginUpdate()
{
    MapModelChange msg( MapModelChange::BEGIN_BATCH_UPDATE, _dataModelRevision );

    for( MapCallbackList::iterator i = _mapCallbacks.begin(); i != _mapCallbacks.end(); i++ )
    {
        i->get()->onMapModelChanged( msg );
    }
}

void
Map::endUpdate()
{
    MapModelChange msg( MapModelChange::END_BATCH_UPDATE, _dataModelRevision );

    for( MapCallbackList::iterator i = _mapCallbacks.begin(); i != _mapCallbacks.end(); i++ )
    {
        i->get()->onMapModelChanged( msg );
    }
}
#endif

void
Map::addLayer(shared_ptr<Layer> layer)
{
    addLayer(layer, _instance->ioOptions);
}

void
Map::addLayer(shared_ptr<Layer> layer, const IOOptions& io)
{
    if (layer == NULL)
        return;

    // ensure it's not already in the map
    if (getIndexOfLayer(layer.get()) != getNumLayers())
        return;

    // Store in a ref_ptr for scope to ensure callbacks don't accidentally delete while adding
    //shared_ptr<Layer> layerRef(layer);

    //rocky::Registry::instance()->clearBlacklist();

    //layer->setReadOptions(_readOptions);

    if (layer->getOpenAutomatically())
    {
        layer->open();
    }

    // do we need this? Won't the callback to this?
    if (layer->isOpen() && getProfile() != NULL)
    {
        layer->addedToMap(this);
    }

    // Set up callbacks. Do this *after* calling addedToMap (since the callback invokes addedToMap)
    //installLayerCallbacks(layer);

    // Add the layer to our stack.
    int newRevision;
    unsigned index = -1;
    {
        util::ScopedWriteLock lock( _mapDataMutex );

        _layers.push_back( layer );
        index = _layers.size() - 1;
        newRevision = ++_dataModelRevision;
    }

    // a separate block b/c we don't need the mutex
    //fire_onLayerAdded(layer, index, newRevision);

    onLayerAdded(layer, index, newRevision);

    //for( MapCallbackList::iterator i = _mapCallbacks.begin(); i != _mapCallbacks.end(); i++ )
    //{
    //    i->get()->onMapModelChanged(MapModelChange(
    //        MapModelChange::ADD_LAYER, newRevision, layer, index));
    //}
}

void
Map::insertLayer(shared_ptr<Layer> layer, unsigned index)
{
    insertLayer(layer, index, _instance->ioOptions);
}

void
Map::insertLayer(
    shared_ptr<Layer> layer,
    unsigned index,
    const IOOptions& io)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(layer != nullptr, void());

    // ensure it's not already in the map
    if (getIndexOfLayer(layer.get()) != getNumLayers())
        return;

    if (layer->getOpenAutomatically())
    {
        layer->open(io);
    }

    if (layer->isOpen() && getProfile() != NULL)
    {
        layer->addedToMap(this);
    }

    // Set up callbacks. Do this *after* calling addedToMap (since the callback invokes addedToMap)
    //installLayerCallbacks(layer);

    // Add the layer to our stack.
    int newRevision;
    {
        util::ScopedWriteLock lock(_mapDataMutex);

        if (index >= _layers.size())
            _layers.push_back(layer);
        else
            _layers.insert(_layers.begin() + index, layer);

        newRevision = ++_dataModelRevision;

        //if (layer->options().terrainPatch() == true)
        //    ++_numTerrainPatchLayers;
    }

    onLayerAdded(layer, index, newRevision);
}

void
Map::removeLayer(shared_ptr<Layer> layer)
{
    if (layer == NULL)
        return;

    // ensure it's in the map
    if (getIndexOfLayer(layer.get()) == getNumLayers())
        return;

    //rocky::Registry::instance()->clearBlacklist();
    unsigned int index = -1;

    shared_ptr<Layer> layerToRemove(layer);
    Revision newRevision;

    //uninstallLayerCallbacks(layerToRemove.get());

    layer->removedFromMap(this);

    // Close the layer if we opened it.
    if (layer->getOpenAutomatically())
    {
        layer->close();
    }

    if ( layerToRemove.get() )
    {
        util::ScopedWriteLock lock( _mapDataMutex );
        index = 0;
        for(LayerVector::iterator i = _layers.begin();
            i != _layers.end(); ++i)
        {
            if (layer == layerToRemove )
            {
                _layers.erase(i);
                newRevision = ++_dataModelRevision;

                //if (layer->options().terrainPatch() == true)
                //    --_numTerrainPatchLayers;

                break;
            }
        }
    }

    // a separate block b/c we don't need the mutex
    if ( newRevision >= 0 )
    {
        onLayerRemoved(layerToRemove, newRevision);
        //for( MapCallbackList::iterator i = _mapCallbacks.begin(); i != _mapCallbacks.end(); i++ )
        //{
        //    i->get()->onMapModelChanged( MapModelChange(
        //        MapModelChange::REMOVE_LAYER, newRevision, layerToRemove.get(), index) );
        //}
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

        // preserve the layer with a ref:
        //osg::ref_ptr<Layer> layerToMove( layer );

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
        onLayerMoved(layerToMove, oldIndex, newIndex, newRevision);
        //for (MapCallbackList::iterator i = _mapCallbacks.begin(); i != _mapCallbacks.end(); i++)
        //{
        //    i->get()->onMapModelChanged(MapModelChange(
        //        MapModelChange::MOVE_LAYER, newRevision, layer, oldIndex, newIndex));
        //}
    }
}

void
Map::addLayers(const std::vector<Layer::ptr>& layers)
{
    addLayers(layers, _instance->ioOptions);
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
    int newRevision;

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
            if (layer->isOpen() && getProfile() != NULL)
            {
                layer->addedToMap(this);
            }

            // Set up callbacks.
            //installLayerCallbacks(layer);

            // a separate block b/c we don't need the mutex
            onLayerAdded(layer, index++, newRevision);
            //for( MapCallbackList::iterator i = _mapCallbacks.begin(); i != _mapCallbacks.end(); i++ )
            //{
            //    i->get()->onMapModelChanged(MapModelChange(
            //        MapModelChange::ADD_LAYER, newRevision, layer, index++));
            //}
        }
    }
}

//void
//Map::installLayerCallbacks(Layer* layer)
//{
//    // Callback to detect changes in "enabled"
//    layer->addCallback(_layerCB.get());
//}

//void
//Map::uninstallLayerCallbacks(Layer* layer)
//{
//    layer->removeCallback(_layerCB.get());
//}

//Revision
//Map::getLayers(LayerVector& out_list) const
//{
//    out_list.reserve( _layers.size() );
//
//    util::ScopedReadLock lock(_mapDataMutex);
//    for (auto layer : _layers)
//        out_list.emplace_back(layer);
//
//    return _dataModelRevision;
//}

unsigned
Map::getNumLayers() const
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
        if (layer->getUID() == uid)
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
Map::getIndexOfLayer(const Layer* layer) const
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
        onLayerRemoved(layer, newRevision);
    }

    //for( MapCallbackList::iterator i = _mapCallbacks.begin(); i != _mapCallbacks.end(); i++ )
    //{
    //    i->get()->onBeginUpdate();

    //    for(LayerVector::iterator layer = layersRemoved.begin();
    //        layer != layersRemoved.end();
    //        ++layer)
    //    {
    //        i->get()->onMapModelChanged(MapModelChange(MapModelChange::REMOVE_LAYER, newRevision, layer->get()));
    //    }

    //    i->get()->onEndUpdate();
    //}
}

shared_ptr<SRS>
Map::getSRS() const
{
    return _profile ? _profile->getSRS() : nullptr;
}

#if 0
shared_ptr<SRS>
Map::getWorldSRS() const
{
    return getSRS() && getSRS()->isGeographic() ? getSRS()->getGeocentricSRS() : getSRS();
}
#endif

#if 0
int 
Map::getNumTerrainPatchLayers() const
{
    return _numTerrainPatchLayers;
}
#endif

void
Map::removeCallback(UID uid)
{
    onLayerAdded.functions.erase(uid);
    onLayerRemoved.functions.erase(uid);
    onLayerMoved.functions.erase(uid);
}
