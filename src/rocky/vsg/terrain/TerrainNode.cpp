/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TerrainNode.h"
#include "TerrainTileNode.h"
#include "TerrainEngine.h"
#include "../VSGUtils.h"

#include <rocky/IOTypes.h>
#include <rocky/Map.h>
#include <rocky/TileLayer.h>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::util;



TerrainProfileNode::TerrainProfileNode(const Profile& in_profile, TerrainNode& in_terrain) :
    profile(in_profile),
    terrain(in_terrain),
    _tiles(in_terrain, this)
{
    //nop
}

void
TerrainProfileNode::reset(VSGContext context)
{
    for (auto& child : this->children)
    {
        context->dispose(child);
    }

    children.clear();

    _tiles.releaseAll();

    // create a new engine to render this map
    _engine = std::make_shared<TerrainEngine>(
        terrain.map,
        profile,
        terrain.renderingSRS,
        terrain.terrainState,
        context,    // runtime API
        settings(), // settings
        this);      // host
}

Result<>
TerrainProfileNode::createRootTiles(VSGContext context)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(_engine != nullptr, Failure_AssertionFailure);
    ROCKY_SOFT_ASSERT_AND_RETURN(_engine->stateFactory.status.ok(), _engine->stateFactory.status.error());
    ROCKY_HARD_ASSERT(children.empty(), "TerrainNode::createRootTiles() called with children already present");

    // once the pipeline exists, we can start creating tiles.
    auto keys = _engine->profile.allKeysAtLOD(terrain.minLevel);

    for (auto& key : keys)
    {
        // create a tile with no parent:
        auto tile = _engine->createTile(key, {});

        // ensure it can't page out:
        tile->doNotExpire = true;

        // Add it to the scene graph
        this->addChild(tile);
    }

    context->compile(vsg::ref_ptr<TerrainProfileNode>(this));

    return ResultVoidOK;
}

bool
TerrainProfileNode::update(VSGContext context)
{
    bool changes = false;

    if (terrain.status.ok())
    {
        if (children.empty())
        {
            auto r = createRootTiles(context);
            if (r.failed())
            {
                terrain.status = r.error();
                Log()->warn("TerrainProfileNode initialize failed: " + terrain.status.error().message);
            }
            changes = true;
        }
        else
        {
            ROCKY_HARD_ASSERT(_engine);

            if (_tiles.update(context->viewer()->getFrameStamp(), context->io, _engine))
                changes = true;

            changes = _engine->update(context);
        }
    }

    return changes;
}

void
TerrainProfileNode::ping(TerrainTileNode* tile, const TerrainTileNode* parent, vsg::RecordTraversal& nv)
{
    _tiles.ping(tile, parent, nv);
}




TerrainNode::TerrainNode(VSGContext context) :
    terrainState(context)
{
    // create the graphics pipeline to render this map
    if (!terrainState.setupTerrainStateGroup(*this, context))
    {
        status = Failure("Failed to set up terrain state group");
    }
}

TerrainNode::Stats
TerrainNode::stats() const
{
    Stats result;
    for (auto& child : children)
    {
        auto* profileNode = static_cast<TerrainProfileNode*>(child.get());
        result.numResidentTiles += profileNode->tiles().size();
        result.geometryPoolSize += profileNode->engine().geometryPool.size();
    }
    return result;
}

Result<>
TerrainNode::from_json(const std::string& JSON, const IOOptions& io)
{
    return TerrainSettings::from_json(JSON);
}

std::string
TerrainNode::to_json() const
{
    return TerrainSettings::to_json();
}

Result<>
TerrainNode::setMap(Map::Ptr in_map, const Profile& in_profile, const SRS& in_renderingSRS, VSGContext context)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(in_map, Failure_AssertionFailure);
    ROCKY_SOFT_ASSERT_AND_RETURN(status.ok(), status.error());

    // remove old hooks:
    if (map)
    {
        _callbacks.clear();
    }

    // dispose of all children
    for (auto& c : children)
    {
        context->dispose(c);
    }

    children.clear();

    map = in_map;
    profile = in_profile;
    renderingSRS = in_renderingSRS;

    if (map)
    {
        _callbacks += map->onLayersChanged([this, context](auto...)
            {
                auto newLayers = map->layers([](auto layer) {
                    return TileLayer::cast(layer) != nullptr; });

                if (newLayers != _terrainLayers)
                {
                    reset(context);
                }
            });
    }

    auto r = createProfiles(context);
    if (r.ok())
    {
        status.clear();
        reset(context);
    }
    else
    {
        status = r.error();
    }

    if (status.ok())
        return ResultVoidOK;
    else
        return status.error();
}

void
TerrainNode::reset(VSGContext context)
{
    // reset all profile nodes:
    for (auto& child : this->children)
    {
        if (auto c = child.cast<TerrainProfileNode>())
            c->reset(context);
    }

    ROCKY_HARD_ASSERT(this->referenceCount() > 0);

    context->compile(vsg::ref_ptr<TerrainNode>(this));

    // cache the terrain layers so we can detect changes later:
    if (map)
    {
        _terrainLayers = map->layers([](auto layer) {
            return TileLayer::cast(layer) != nullptr; });
    }
}

Result<>
TerrainNode::createProfiles(VSGContext context)
{
    if (profile.isComposite())
    {
        for (auto& subprofile : profile.subprofiles())
        {
            auto profileNode = TerrainProfileNode::create(subprofile, *this);
            this->addChild(profileNode);
        }
    }
    else
    {
        auto profileNode = TerrainProfileNode::create(profile, *this);
        this->addChild(profileNode);
    }

    return ResultVoidOK;
}

bool
TerrainNode::update(VSGContext context)
{
    bool changes = false;
    for (auto& child : children)
    {
        if (auto c = child.cast<TerrainProfileNode>())
            changes = c->update(context) || changes;
    }

    // check for settings change
    terrainState.updateSettings(*this);

    return changes;
}

const TerrainSettings&
TerrainProfileNode::settings() const
{
    return terrain;
}

Result<GeoPoint>
TerrainNode::intersect(const GeoPoint& input) const
{
    if (!input)
        return Failure{};

    // world vector from earth's center to the input point:
    GeoPoint world = input.transform(renderingSRS);

    vsg::dvec3 start, end;
    if (renderingSRS.isGeocentric())
    {
        start = to_vsg(world) * 2.0;
        end.set(0, 0, 0);
    }
    else
    {
        start.set(world.x, world.y, 1e6);
        end.set(world.x, world.y, -1e6);
    }

    vsg::LineSegmentIntersector lsi(start, end);

    this->accept(lsi);

    if (lsi.intersections.empty())
        return Failure{};

    // there should be only one, but we will take the closest one anyway:
    auto closest = std::min_element(
        lsi.intersections.begin(), lsi.intersections.end(),
        [](const auto& lhs, const auto& rhs) { return lhs->ratio < rhs->ratio; });

    return GeoPoint(renderingSRS, closest->get()->worldIntersection);
}
