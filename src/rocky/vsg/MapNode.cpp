/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "MapNode.h"
#include "engine/Utils.h"
#include "json.h"
#include <rocky/Horizon.h>
#include <rocky/ImageLayer.h>
#include <rocky/ElevationLayer.h>

#include <vsg/io/Options.h>
#include <vsg/app/RecordTraversal.h>
#include <vsg/vk/State.h>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::util;

#undef LC
#define LC "[MapNode] "

MapNode::MapNode(const InstanceVSG& instance) :
    instance(instance),
    map(Map::create(instance))
{
    construct();
}

MapNode::MapNode(shared_ptr<Map> in_map) :
    instance(reinterpret_cast<InstanceVSG&>(in_map->instance())),
    map(in_map)
{
    construct();
}

MapNode::MapNode(const JSON& conf, const InstanceVSG& instance) :
    instance(instance),
    map(Map::create(instance))
{
    construct();
}

void
MapNode::construct()
{
    terrain = TerrainNode::create(instance.runtime());
    addChild(terrain);

    // make a group for the model layers.  This node is a PagingManager instead of a regular Group to allow PagedNode's to be used within the layers.
    _layerNodes = vsg::Group::create();
    this->addChild(_layerNodes);
}

Status
MapNode::from_json(const std::string& JSON, const IOOptions& io)
{
    const auto j = parse_json(JSON);

    Status status = j.status;

    if (status.ok() && map)
    {
        status = map->from_json(j["map"].dump(), io);
    }

    if (status.ok() && terrain)
    {
        status = terrain->from_json(j["terrain"].dump(), io);
    }

    return status;
}

std::string
MapNode::to_json() const
{
    auto j = json::object();

    if (map)
    {
        j["map"] = json::parse(map->to_json());
    }

    if (terrain)
    {
        j["terrain"] = json::parse(terrain->to_json());
    }

    return j.dump();
}

const TerrainSettings&
MapNode::terrainSettings() const
{
    return *terrain.get();
}

TerrainSettings&
MapNode::terrainSettings()
{
    return *terrain.get();
}

const SRS&
MapNode::mapSRS() const
{
    return map && map->profile().valid() ?
        map->profile().srs() :
        SRS::EMPTY;
}

const SRS&
MapNode::worldSRS() const
{
    if (_worldSRS.valid())
        return _worldSRS;
    else if (mapSRS().isGeodetic())
        return SRS::ECEF;
    else
        return mapSRS();
}

void
MapNode::update(const vsg::FrameStamp* f)
{
    ROCKY_HARD_ASSERT_STATUS(instance.status());
    ROCKY_HARD_ASSERT(map != nullptr && terrain != nullptr);

    if (terrain->map == nullptr)
    {
        auto st = terrain->setMap(map, worldSRS());

        if (st.failed())
        {
            Log()->warn(st.message);
        }
    }

    // on our first update, open any layers that are marked to automatic opening.
    if (!_openedLayers)
    {
        map->openAllLayers(instance.io());
        _openedLayers = true;
    }

    terrain->update(f, instance.io());
}

void
MapNode::accept(vsg::RecordTraversal& rv) const
{
    if (worldSRS().isGeocentric())
    {
        std::shared_ptr<Horizon> horizon;
        rv.getState()->getValue("horizon", horizon);
        if (!horizon)
        {
            horizon = std::make_shared<Horizon>(worldSRS().ellipsoid());
            rv.getState()->setValue("horizon", horizon);
        }

        auto eye = vsg::inverse(rv.getState()->modelviewMatrixStack.top()) * vsg::dvec3(0, 0, 0);
        horizon->setEye(to_glm(eye));
    }

    rv.setValue("worldsrs", worldSRS());

    rv.setValue("terrainengine", terrain->engine.get());

    Inherit::accept(rv);
}
