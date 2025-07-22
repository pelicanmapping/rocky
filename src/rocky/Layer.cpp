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

Layer::Layer(std::string_view conf) :
    super()
{
    construct(conf);
}

void
Layer::construct(std::string_view conf)
{
    _uid = rocky::createUID();

    const auto j = parse_json(conf);
    get_to(j, "name", _name);
    get_to(j, "open", openAutomatically);
    get_to(j, "attribution", attribution);
    get_to(j, "l2_cache_size", l2CacheSize);

    _status = Status(
        Status::ResourceUnavailable,
        openAutomatically ? "Layer closed" : "Layer disabled");
}

JSON
Layer::to_json() const
{
    auto j = json::object();
    set(j, "type", getLayerTypeName());
    set(j, "name", _name);
    set(j, "open", openAutomatically);
    set(j, "attribution", attribution);
    set(j, "l2_cache_size", l2CacheSize);
    return j.dump();
}

void
Layer::setLayerTypeName(const std::string& value)
{
    _layerTypeName = value;
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

Status
Layer::open(const IOOptions& io)
{
    // Cannot open a layer that's already open OR is disabled.
    if (isOpen())
    {
        return status();
    }

    std::unique_lock lock(_state_mutex);

    setStatus(openImplementation(io));

    Log()->debug("Layer \"{}\" status = {}", name(), status().toString());

    return status();
}

void
Layer::close()
{
    if (isOpen())
    {
        std::unique_lock lock(_state_mutex);
        closeImplementation();
        _status = Status(Status::ResourceUnavailable, "Layer closed");
    }
}

Status
Layer::openImplementation(const IOOptions& io)
{
    return Status_OK;
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
