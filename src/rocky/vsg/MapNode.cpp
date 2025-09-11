/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "MapNode.h"
#include "VSGUtils.h"
#include "json.h"
#include <rocky/Horizon.h>
#include <rocky/vsg/NodeLayer.h>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::util;

#undef LC
#define LC "[MapNode] "

MapNode::MapNode(VSGContext context)
{
    map = Map::create();

    terrainNode = TerrainNode::create(context);
    this->addChild(terrainNode);

    // default to geodetic:
    profile = Profile("global-geodetic");
}

Result<>
MapNode::from_json(const std::string& JSON, const IOOptions& io)
{
    const auto j = parse_json(JSON);
    if (j.status.failed())
        return j.status.error();

    if (map && j.contains("map"))
    {        
        auto r = map->from_json(j.at("map").dump(), io);
        if (r.failed())
            return r.error();
    }

    if (j.contains("profile"))
    {
        get_to(j, "profile", profile);
    }

    if (terrainNode && j.contains("terrain"))
    {
        auto r = terrainNode->from_json(j.at("terrain").dump(), io);
        if (r.failed())
            return r.error();
    }

    return ResultVoidOK;
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

    if (profile.valid() && profile != Profile("global-geodetic"))
    {
        j["profile"] = profile;
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
MapNode::srs() const
{
    if (profile.srs().isGeodetic() || profile.srs().isQSC())
        return profile.srs().geocentricSRS();
    else
        return profile.srs();
}

bool
MapNode::update(VSGContext context)
{
    //ROCKY_HARD_ASSERT_STATUS(context.status());
    ROCKY_HARD_ASSERT(map != nullptr && terrainNode != nullptr);

    bool changes = false;

    if (terrainNode->map == nullptr || terrainNode->profile != profile)
    {
        auto st = terrainNode->setMap(map, profile, srs(), context);

        if (st.failed())
        {
            Log()->warn(st.error().message);
        }
    }

    // on our first update, open any layers that are marked to automatic opening.
    if (!_openedLayers)
    {
        auto r = map->openAllLayers(context->io);
        if (r.failed())
        {
            Log()->warn("Failed to open at least one layer... " + r.error().message);
        }

        _openedLayers = true;
    }

    return terrainNode->update(context);
}

void
MapNode::traverse(vsg::RecordTraversal& record) const
{
    auto viewID = record.getCommandBuffer()->viewID;

    auto& horizon = _horizon[viewID];

    if (srs().isGeocentric())
    {
        if (!horizon)
        {
            horizon.setEllipsoid(srs().ellipsoid());
        }

        auto eye = vsg::inverse(record.getState()->modelviewMatrixStack.top()) * vsg::dvec3(0, 0, 0);
        bool is_ortho = record.getState()->projectionMatrixStack.top()(3, 3) != 0.0;
        horizon.setEye(to_glm(eye), is_ortho);

        record.setValue("rocky.horizon", &_horizon);
    }

    record.setValue("rocky.worldsrs", srs());

    record.setObject("rocky.terraintilehost", terrainNode);

    Inherit::traverse(record);

    map->each<NodeLayer>([&](auto layer)
        {
            if (layer->isOpen() && layer->node)
            {
                layer->node->accept(record);
            }
        });
}

void
MapNode::traverse(vsg::Visitor& visitor)
{
    visitor.setValue("rocky.worldsrs", srs());

    Inherit::traverse(visitor);

    map->each<NodeLayer>([&](auto layer)
        {
            if (layer->isOpen() && layer->node)
            {
                layer->node->accept(visitor);
            }
        });
}

void
MapNode::traverse(vsg::ConstVisitor& visitor) const
{
    visitor.setValue("rocky.worldsrs", srs());

    Inherit::traverse(visitor);

    map->each<NodeLayer>([&](auto layer)
        {
            if (layer->isOpen() && layer->node)
            {
                layer->node->accept(visitor);
            }
        });
}

vsg::ref_ptr<MapNode>
MapNode::create(VSGContext context)
{
    //ROCKY_SOFT_ASSERT_AND_RETURN(context, {}, "ILLEGAL: null context");
    //ROCKY_SOFT_ASSERT_AND_RETURN(context->viewer, {}, "ILLEGAL: context does not contain a viewer");
    //ROCKY_SOFT_ASSERT_AND_RETURN(context->viewer->updateOperations, {}, "ILLEGAL: viewer does not contain update operations");

    auto mapNode = vsg::ref_ptr<MapNode>(new MapNode(context));

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
