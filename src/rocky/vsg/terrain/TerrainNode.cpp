/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "TerrainNode.h"
#include "TerrainTileNode.h"
#include "TerrainTileFactory.h"
#include "../VSGUtils.h"

#include <rocky/IOTypes.h>
#include <rocky/Map.h>
#include <rocky/TileLayer.h>

using namespace ROCKY_NAMESPACE;

TerrainProfileNode::TerrainProfileNode(const Profile& in_profile, TerrainNode& in_terrain, VSGContext vsgcontext) :
    profile(in_profile),
    terrain(in_terrain),
    _tiles(in_terrain, this, vsgcontext)
{
    //nop
}

TerrainProfileNode::~TerrainProfileNode()
{
#ifdef ROCKY_DEBUG_MEMCHECK
    Log()->debug("~TerrainProfileNode");
#endif
}

void
TerrainProfileNode::reset(VSGContext vsgcontext)
{
    for (auto& child : this->children)
    {
        vsgcontext->dispose(child);
    }

    children.clear();

    _tiles.releaseAll();

    // create a new engine to render this map
    _tileFactory = std::make_shared<TerrainTileFactory>(
        terrain.map,
        profile,
        terrain.renderingSRS,
        terrain.state,
        vsgcontext,
        settings(),
        this);      // host
}

Result<>
TerrainProfileNode::createRootTiles(VSGContext vsgcontext)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(_tileFactory != nullptr, Failure_AssertionFailure);
    ROCKY_SOFT_ASSERT_AND_RETURN(_tileFactory->state->status.ok(), _tileFactory->state->status.error());
    ROCKY_HARD_ASSERT(children.empty(), "TerrainNode::createRootTiles() called with children already present");

    // once the pipeline exists, we can start creating tiles.
    auto keys = _tileFactory->profile.allKeysAtLOD(terrain.minLevel);

    for (auto& key : keys)
    {
        // create a tile with no parent:
        auto tile = _tileFactory->createTile(key, {}, vsgcontext);

        // ensure it can't page out:
        tile->doNotExpire = true;

        // Add it to the scene graph
        this->addChild(tile);
    }

    vsgcontext->compile(vsg::ref_ptr<TerrainProfileNode>(this));

    return ResultVoidOK;
}

bool
TerrainProfileNode::update(VSGContext vsgcontext)
{
    bool changes = false;

    if (terrain.status.ok())
    {
        if (children.empty())
        {
            auto r = createRootTiles(vsgcontext);
            if (r.failed())
            {
                terrain.status = r.error();
                Log()->warn("TerrainProfileNode initialize failed: " + terrain.status.error().message);
            }
            changes = true;
        }
        else
        {
            ROCKY_HARD_ASSERT(_tileFactory);

            if (_tiles.update(_tileFactory, vsgcontext))
                changes = true;

            if (_tileFactory->update(vsgcontext))
                changes = true;
        }
    }

    return changes;
}

void
TerrainProfileNode::ping(TerrainTileNode* tile, const TerrainTileNode* parent, vsg::RecordTraversal& nv)
{
    _tiles.ping(tile, parent, nv);
}




TerrainNode::TerrainNode(VSGContext vsgcontext) :
    Inherit()
{
    // create the graphics pipeline to render this map
    state = std::make_shared<TerrainState>(vsgcontext);

    state->buildTerrainStateGroup(_profileNodes, vsgcontext);

    if (!_profileNodes)
    {
        status = Failure("Failed to set up terrain state group. Shaders not found?");
    }
    else
    {
        addChild(vsg::MASK_ALL, _profileNodes);
    }

    _sharedRenderData = vsgcontext->sharedRenderData;
}

TerrainNode::~TerrainNode()
{
    _callbacks.clear();

#ifdef ROCKY_DEBUG_MEMCHECK
    Log()->debug("~TerrainNode");
#endif
}

TerrainNode::Stats
TerrainNode::stats() const
{
    Stats result;
    for (auto& child : _profileNodes->children)
    {
        if (auto profileNode = child.cast<TerrainProfileNode>())
        {
            result.numResidentTiles += profileNode->tiles().size();
            result.geometryPoolSize += profileNode->tileFactory().geometryPool.size();
        }
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
    for (auto& c : _profileNodes->children)
    {
        context->dispose(c);
    }

    _profileNodes->children.clear();

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
    for (auto& child : _profileNodes->children)
    {
        if (auto c = child.cast<TerrainProfileNode>())
            c->reset(context);
    }

    ROCKY_HARD_ASSERT(this->referenceCount() > 0);

    // cache the terrain layers so we can detect changes later:
    if (map)
    {
        _terrainLayers = map->layers([](auto layer) {
            return TileLayer::cast(layer) != nullptr; });
    }

    // update the state data with the (possibly new) profile:
    state->updateProfile(profile);
}

void
TerrainNode::rebuildRenderPipeline(VSGContext context)
{
    state->rebuildPipeline(context);
    state->buildTerrainStateGroup(_profileNodes, context);
}

Result<>
TerrainNode::createProfiles(VSGContext vsgcontext)
{
    if (profile.isComposite())
    {
        for (auto& subprofile : profile.subprofiles())
        {
            _profileNodes->addChild(TerrainProfileNode::create(subprofile, *this, vsgcontext));
        }
    }
    else
    {
        _profileNodes->addChild(TerrainProfileNode::create(profile, *this, vsgcontext));
    }

    return ResultVoidOK;
}

bool
TerrainNode::update(VSGContext vsgcontext)
{
    if (vsgcontext->sharedRenderData->sharedDescriptorsChanged(_sharedDataRevision))
    {
        Log()->debug("TerrainNode: shared buffers changed; updating global descriptor set");
        state->buildTerrainStateGroup(_profileNodes, vsgcontext);
        vsgcontext->compile(_profileNodes);
    }


    bool changes = false;
    for (auto& child : _profileNodes->children)
    {
        if (auto c = child.cast<TerrainProfileNode>())
        {
            changes = c->update(vsgcontext) || changes;
        }
    }

    // check for settings changes
    this->children[0].mask = castShadows ? vsg::MASK_ALL : (vsg::MASK_ALL & ~VSG_SHADOW_CAMERA_TRAVERSAL_MASK);

    state->updateSettings(*this);

    return changes;
}

const TerrainSettings&
TerrainProfileNode::settings() const
{
    return terrain;
}

TerrainActivity&
TerrainProfileNode::activity()
{
    return terrain;
}

Result<TerrainIntersection>
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

    // given the intersection object, calcluate the normal vector at the intersection point:
    auto verts = closest->get()->arrays.front()->cast<vsg::vec3Array>();
    auto& indices = closest->get()->indexRatios;
    vsg::vec3 normal = vsg::normalize(vsg::cross(
        verts->at(indices[1].index) - verts->at(indices[0].index),
        verts->at(indices[2].index) - verts->at(indices[0].index)));

    // transform the normal into world space:
    auto worldNormal = glm::dmat3(to_glm(closest->get()->localToWorld)) * glm::dvec3(to_glm(normal));

    TerrainIntersection result;
    result.point = GeoPoint(renderingSRS, closest->get()->worldIntersection);
    result.normal = glm::normalize(worldNormal);

    return result;

    //return GeoPoint(renderingSRS, closest->get()->worldIntersection);
}
