/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Layer.h"
#include "GeoExtent.h"
#include "json.h"

using namespace ROCKY_NAMESPACE;

#define LC "[Layer] \"" << getName() << "\" "

Layer::Layer() :
    super()
{
    construct(JSON());
}

Layer::Layer(const JSON& conf) :
    super()
{
    construct(conf);
}

void
Layer::construct(const JSON& conf)
{
    _revision = 1;
    _uid = rocky::createUID();
    _isClosing = false;
    _isOpening = false;
    _reopenRequired = false;
    _renderType = RENDERTYPE_NONE;

    const auto j = parse_json(conf);
    get_to(j, "name", _name);
    get_to(j, "open", _openAutomatically);
    get_to(j, "attribution", _attribution);
    get_to(j, "l2_cache_size", _l2cachesize);

    _status = Status(
        Status::ResourceUnavailable,
        getOpenAutomatically() ? "Layer closed" : "Layer disabled");
}

JSON
Layer::to_json() const
{
    auto j = json::object();
    set(j, "type", getConfigKey());
    set(j, "name", _name);
    set(j, "open", _openAutomatically);
    set(j, "attribution", _attribution);
    set(j, "l2_cache_size", _l2cachesize);
    return j.dump();
}

void
Layer::setConfigKey(const std::string& value)
{
    _configKey = value;
}

Layer::~Layer()
{
    //nop
}

void
Layer::dirty()
{
    bumpRevision();
}

void
Layer::bumpRevision()
{
    ++_revision;
}

void
Layer::removeCallback(UID uid)
{
    onLayerOpened.functions.erase(uid);
    onLayerClosed.functions.erase(uid);
}

bool
Layer::getOpenAutomatically() const
{
    return _openAutomatically.value();
}

void
Layer::setOpenAutomatically(bool value)
{
    _openAutomatically = value;
}

const Status&
Layer::setStatus(const Status& status) const
{
    _status = status;
    return _status;
}

const Status&
Layer::setStatus(const Status::Code& code, const std::string& message) const
{
    return setStatus(Status(code, message));
}

void
Layer::setCachePolicy(const CachePolicy& value)
{
    _cachePolicy = value;

#if 0
    if (_cacheSettings.valid())
    {
        _cacheSettings->integrateCachePolicy(options().cachePolicy());
    }
#endif
}

const CachePolicy&
Layer::cachePolicy() const
{
    return _cachePolicy.value();
    //return options().cachePolicy().get();
}

Status
Layer::open(const IOOptions& io)
{
    // Cannot open a layer that's already open OR is disabled.
    if (isOpen())
    {
        return status();
    }

    std::unique_lock lock(_state_mutex);

    // be optimistic :)
    _status = StatusOK;

#if 0
    // Install any shader #defines
    if (options().shaderDefine().has_value() && !options().shaderDefine()->empty())
    {
        ROCKY_INFO << LC << "Setting shader define " << options().shaderDefine().get() << "\n";
        getOrCreateStateSet()->setDefine(options().shaderDefine().get());
    }
#endif

    _isOpening = true;
    setStatus(openImplementation(io));
    if (isOpen())
    {
        //fireCallback(&LayerCallback::onOpen);
    }
    _isOpening = false;

    return status();
}

Status
Layer::open()
{
    return open(IOOptions());
}

Status
Layer::openImplementation(const IOOptions& io)
{
#if 0
    // Create some local cache settings for this layer.
    // There might be a CacheSettings object in the readoptions that
    // came from the map. If so, copy it.
    CacheSettings* oldSettings = CacheSettings::get(_readOptions.get());
    _cacheSettings = oldSettings ? new CacheSettings(*oldSettings) : new CacheSettings();

    // If the layer hints are set, integrate that cache policy next.
    _cacheSettings->integrateCachePolicy(layerHints().cachePolicy());

    // bring in the new policy for this layer if there is one:
    _cacheSettings->integrateCachePolicy(options().cachePolicy());

    // if caching is a go, install a bin.
    if (_cacheSettings->isCacheEnabled())
    {
        _runtimeCacheId = getCacheID();

        // make our cacheing bin!
        CacheBin* bin = _cacheSettings->getCache()->addBin(_runtimeCacheId);
        if (bin)
        {
            ROCKY_INFO << LC << "Cache bin is [" << _runtimeCacheId << "]\n";
            _cacheSettings->setCacheBin(bin);
        }
        else
        {
            // failed to create the bin, so fall back on no cache mode.
            ROCKY_WARN << LC << "Failed to open a cache bin [" << _runtimeCacheId << "], disabling caching\n";
            _cacheSettings->cachePolicy() = CachePolicy::NO_CACHE;
        }
    }

    // Store it for further propagation!
    _cacheSettings->store(_readOptions.get());
#endif
    return StatusOK;
}

void
Layer::closeImplementation()
{
    //nop
}

void
Layer::close()
{    
    if (isOpen())
    {
        std::unique_lock lock(_state_mutex);
        _isClosing = true;
        closeImplementation();
        _status = Status(Status::ResourceUnavailable, "Layer closed");
        _runtimeCacheId = "";
        _isClosing = false;
    }
}

bool
Layer::isOpen() const
{
    return status().ok();
}

const Status&
Layer::status() const
{
    return _status;
}

const char*
Layer::typeName() const
{
    return typeid(*this).name();
}

const GeoExtent&
Layer::extent() const
{
    static GeoExtent s_invalid;
    return s_invalid;
}

DateTimeExtent
Layer::dateTimeExtent() const
{
    return DateTimeExtent();
}

#if 0
osg::StateSet*
Layer::getOrCreateStateSet()
{
    if (!_stateSet.valid())
    {
        _stateSet = new osg::StateSet();
        _stateSet->setName("Layer");
    }
    return _stateSet.get();
}

osg::StateSet*
Layer::getStateSet() const
{
    return _stateSet.get();
}
#endif

#if 0
void
Layer::fireCallback(LayerCallback::MethodPtr method)
{
    for (CallbackVector::iterator i = _callbacks.begin(); i != _callbacks.end(); ++i)
    {
        LayerCallback* cb = dynamic_cast<LayerCallback*>(i->get());
        if (cb) (cb->*method)(this);
    }
}
#endif

std::string
Layer::attribution() const
{
    // Get the attribution from the layer if it's set.
    return _attribution;
}

void
Layer::setAttribution(const std::string& value)
{
    _attribution = value;
}

void
Layer::modifyTileBoundingBox(const TileKey& key, const Box& box) const
{
    //NOP
}

const Layer::Hints&
Layer::hints() const
{
    return _hints;
}
