/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "MapNode.h"
#include "TerrainNode.h"
#include "Utils.h"
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
    _instance(instance),
    _map(Map::create(instance))
{
    construct({});
}

MapNode::MapNode(shared_ptr<Map> map) :
    _instance(reinterpret_cast<InstanceVSG&>(_map->instance()))
{
    construct({});
}

MapNode::MapNode(const JSON& conf, const InstanceVSG& instance) :
    _instance(instance),
    _map(Map::create(instance))
{
    construct(conf);
}

void
MapNode::construct(const JSON& conf)
{
    const auto j = parse_json(conf);
    get_to(j, "screen_space_error", _screenSpaceError);

    _terrain = TerrainNode::create(runtime(), conf);
    addChild(_terrain);

    _isOpen = false;

    // make a group for the model layers.  This node is a PagingManager instead of a regular Group to allow PagedNode's to be used within the layers.
    _layerNodes = vsg::Group::create();
    this->addChild(_layerNodes);

    _readyForUpdate = true;
}

JSON
MapNode::to_json() const
{
    auto j = json::object();
    set(j, "screen_space_error", _screenSpaceError);

    // all map layers
    auto layers_json = json::array();
    for (auto& layer : _map->layers().all())
    {
        layers_json.push_back(parse_json(layer->to_json()));
    }

    if (!layers_json.empty())
    {
        j["layers"] = layers_json;
    }

    return j.dump();
}

RuntimeContext&
MapNode::runtime()
{
    ROCKY_HARD_ASSERT(_instance.status().ok());
    return _instance.runtime();
}

vsg::ref_ptr<TerrainNode>
MapNode::terrainNode() const
{
    return _terrain;
}

bool
MapNode::open()
{
    if (_isOpen)
        return _isOpen;

    _isOpen = true;

    return true;
}

MapNode::~MapNode()
{
    //nop
}

shared_ptr<Map>
MapNode::map() const
{
    return _map;
}

const SRS&
MapNode::mapSRS() const
{
    return map() && map()->profile().valid() ?
        map()->profile().srs() :
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
MapNode::setScreenSpaceError(float value)
{
    _screenSpaceError = value;

    ROCKY_TODO("SSE");
    //// update the corresponding terrain option:
    //getTerrainOptions().setScreenSpaceError(value);

    //// update the uniform:
    //_sseU->set(value);
}

float
MapNode::screenSpaceError() const
{
    return _screenSpaceError;
}

vsg::ref_ptr<vsg::Group>
MapNode::getLayerNodeGroup() const
{
    return _layerNodes;
}

shared_ptr<MapNode>
MapNode::get(const vsg::Node* graph, unsigned travmask)
{
    ROCKY_HARD_ASSERT(false, "NYI");
    return nullptr;
    //return findRelativeNodeOfType<MapNode>( graph, travmask );
}

void
MapNode::update(const vsg::FrameStamp* f)
{
    ROCKY_HARD_ASSERT(_instance.status().ok());

    if (_terrain->map() == nullptr)
    {
        auto st = _terrain->setMap(map(), worldSRS());

        if (st.failed())
        {
            Log::warn() << st.message << std::endl;
        }
    }

    _terrain->update(f, _instance.ioOptions());
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

    Inherit::accept(rv);
}
