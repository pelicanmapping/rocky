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

MapNode::MapNode(std::shared_ptr<Map> in_map) :
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
    terrainNode = TerrainNode::create(instance.runtime());
    addChild(terrainNode);

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

    if (status.ok() && terrainNode)
    {
        status = terrainNode->from_json(j["terrain"].dump(), io);
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

    if (terrainNode)
    {
        j["terrain"] = json::parse(terrainNode->to_json());
    }

    return j.dump();
}

const TerrainSettings&
MapNode::terrainSettings() const
{
    return *terrainNode.get();
}

TerrainSettings&
MapNode::terrainSettings()
{
    return *terrainNode.get();
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

bool
MapNode::update(const vsg::FrameStamp* f)
{
    ROCKY_HARD_ASSERT_STATUS(instance.status());
    ROCKY_HARD_ASSERT(map != nullptr && terrainNode != nullptr);

    bool changes = false;

    if (terrainNode->map == nullptr)
    {
        auto st = terrainNode->setMap(map, worldSRS());

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

    return terrainNode->update(f, instance.io());
}

void
MapNode::accept(vsg::RecordTraversal& rv) const
{
    auto viewID = rv.getState()->_commandBuffer->viewID;

    auto& viewlocal = _viewlocal[viewID];

    if (worldSRS().isGeocentric())
    {
        if (viewlocal.horizon == nullptr)
        {
            viewlocal.horizon = std::make_shared<Horizon>(worldSRS().ellipsoid());
        }

        auto eye = vsg::inverse(rv.getState()->modelviewMatrixStack.top()) * vsg::dvec3(0, 0, 0);
        bool is_ortho = rv.getState()->projectionMatrixStack.top()(3, 3) != 0.0;
        viewlocal.horizon->setEye(to_glm(eye), is_ortho);

        rv.setValue("rocky.horizon", viewlocal.horizon);
    }

    rv.setValue("rocky.worldsrs", worldSRS());

    rv.setObject("rocky.terraintilehost", terrainNode);

    Inherit::accept(rv);
}
