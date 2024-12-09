/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "MapNode.h"
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

MapNode::MapNode()
{
    map = Map::create();

    terrainNode = TerrainNode::create();
    this->addChild(terrainNode);

    _layerNodes = vsg::Group::create();
    this->addChild(_layerNodes);

    // default to geodetic:
    profile = Profile::GLOBAL_GEODETIC;
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
    return profile.srs();
}

const SRS&
MapNode::worldSRS() const
{
    if (mapSRS().isGeodetic())
        return mapSRS().geocentricSRS();
    else
        return mapSRS();
}

bool
MapNode::update(VSGContext context)
{
    //ROCKY_HARD_ASSERT_STATUS(context.status());
    ROCKY_HARD_ASSERT(map != nullptr && terrainNode != nullptr);

    bool changes = false;

    if (terrainNode->map == nullptr)
    {
        auto st = terrainNode->setMap(map, profile, context);

        if (st.failed())
        {
            Log()->warn(st.message);
        }
    }

    // on our first update, open any layers that are marked to automatic opening.
    if (!_openedLayers)
    {
        map->openAllLayers(context->io);
        _openedLayers = true;
    }

    return terrainNode->update(context);
}

void
MapNode::traverse(vsg::RecordTraversal& rv) const
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

    Inherit::traverse(rv);
}

vsg::ref_ptr<MapNode>
MapNode::create(VSGContext context)
{
    //ROCKY_SOFT_ASSERT_AND_RETURN(context, {}, "ILLEGAL: null context");
    //ROCKY_SOFT_ASSERT_AND_RETURN(context->viewer, {}, "ILLEGAL: context does not contain a viewer");
    //ROCKY_SOFT_ASSERT_AND_RETURN(context->viewer->updateOperations, {}, "ILLEGAL: viewer does not contain update operations");

    auto mapNode = vsg::ref_ptr<MapNode>(new MapNode());

    if (context && context->viewer && context->viewer->updateOperations)
    {
        auto update = [mapNode, context]()
            {
                context->update();
                mapNode->update(context);
            };

        context->viewer->updateOperations->add(LambdaOperation::create(update), vsg::UpdateOperations::ALL_FRAMES);
    }

    return mapNode;
}
