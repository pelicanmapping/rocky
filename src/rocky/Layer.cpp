/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Layer.h"
#include "GeoExtent.h"
#include "json.h"

using namespace ROCKY_NAMESPACE;

Layer::Layer() :
    super()
{
    construct({});
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

    // catched JSON parsing errors for ALL subclasses
    if (j.status.failed())
    { 
        fail(j.status.error());
        return;
    }

    get_to(j, "name", name);
    get_to(j, "open", openAutomatically);
    get_to(j, "attribution", attribution);

    _status = Failure(Failure::ResourceUnavailable, openAutomatically ? "Layer closed" : "Layer disabled");
}

std::string
Layer::to_json() const
{
    auto j = json::object();
    set(j, "type", getLayerTypeName());
    set(j, "name", name);
    set(j, "open", openAutomatically);
    set(j, "attribution", attribution);
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

const Failure&
Layer::fail(const Failure& f) const
{
    Log()->debug("{} \"{}\" FAILED with status {}", className(), name, f.message);
    _status = f;
    return _status.error();
}

const Failure&
Layer::fail(const Failure::Type code, std::string_view message) const
{
    return fail(Failure(code, message));
}

Result<>
Layer::open(const IOOptions& io)
{
    // Cannot open a layer that's already open OR is disabled.
    if (!isOpen())
    {
        std::unique_lock lock(_state_mutex);

        auto r = openImplementation(io);

        if (r.ok())
        {
            _status = Status{}; // ok!
        }
        else
        {
            fail(r.error());
            Log()->debug("Layer \"{}\" failed to open: {}", name, r.error().message);
        }
    }

    if (status().ok())
        return ResultVoidOK; // ok
    else
        return status().error();
}

void
Layer::close()
{
    if (isOpen())
    {
        std::unique_lock lock(_state_mutex);
        closeImplementation();
        fail(Failure::ResourceUnavailable, "Layer closed");
    }
}

Result<>
Layer::openImplementation(const IOOptions& io)
{
    return ResultVoidOK; // ok
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
