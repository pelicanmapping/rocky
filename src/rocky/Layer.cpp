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
        openAutomatically() ? "Layer closed" : "Layer disabled");
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

const optional<bool>&
Layer::openAutomatically() const
{
    return _openAutomatically;
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

Status
Layer::openImplementation(const IOOptions& io)
{
    return StatusOK;
}

void
Layer::closeImplementation()
{
    //nop
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

const optional<std::string>&
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
