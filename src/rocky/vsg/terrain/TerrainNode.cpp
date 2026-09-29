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
#include <cmath>
#include <cfloat>

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
        terrain.onTileBoundsChanged.fire(key);
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
    onTileBoundsChanged.fire(TileKey{});
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

namespace
{
    //! Accumulates leaf tile boxes and covered map-space area, rejecting missing/invalid resident surfaces.
    //! Descending the hierarchy avoids including enormous ancestor boxes in an otherwise local query.
    bool accumulateHeightRange(
        const TerrainTileNode& tile, const GeoExtent& footprint, const glm::dmat4& worldToLocal,
        glm::dvec2& range, double& coveredArea)
    {
        const auto overlap = tile.key.extent().intersectionSameSRS(footprint);
        if (!overlap.valid() || overlap.width() <= 0.0 || overlap.height() <= 0.0)
            return true;

        if (tile.children.size() > 1u)
        {
            auto* quad = tile.children[1]->cast<vsg::QuadGroup>();
            if (!quad)
                return false;
            for (const auto& child : quad->children)
            {
                auto* childTile = child ? child->cast<TerrainTileNode>() : nullptr;
                if (!childTile || !accumulateHeightRange(*childTile, footprint, worldToLocal, range, coveredArea))
                    return false;
            }
            return true;
        }

        if (!tile.surface || !tile.surface->localbbox.valid())
            return false;

        const auto& box = tile.surface->localbbox;
        const auto matrix = worldToLocal * to_glm(tile.surface->matrix);
        for (unsigned corner = 0u; corner < 8u; ++corner)
        {
            const glm::dvec4 local(
                (corner & 1u) ? box.max.x : box.min.x,
                (corner & 2u) ? box.max.y : box.min.y,
                (corner & 4u) ? box.max.z : box.min.z, 1.0);
            const auto point = matrix * local;
            if (!std::isfinite(point.z))
                return false;
            range.x = std::min(range.x, point.z);
            range.y = std::max(range.y, point.z);
        }
        coveredArea += overlap.width() * overlap.height();
        return true;
    }

    //! Rejects invalid transforms and non-finite coordinates before constructing an intersection ray.
    bool finitePoint(const GeoPoint& point)
    {
        return point.valid() && std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
    }

    //! Finds the first loaded terrain triangle along a world-space segment; callers synchronize scene changes.
    Result<TerrainIntersection> intersectSegment(
        const TerrainNode& terrain, const vsg::dvec3& start, const vsg::dvec3& end)
    {
        vsg::LineSegmentIntersector lsi(start, end);
        terrain.accept(lsi);

        if (lsi.intersections.empty())
            return Failure{};

        auto closest = std::min_element(
            lsi.intersections.begin(), lsi.intersections.end(),
            [](const auto& lhs, const auto& rhs) { return lhs->ratio < rhs->ratio; });

        // Terrain geometry stores float vertices in a local frame, with a double-precision world transform.
        auto verts = closest->get()->arrays.front()->cast<vsg::vec3Array>();
        auto& indices = closest->get()->indexRatios;
        vsg::vec3 normal = vsg::normalize(vsg::cross(
            verts->at(indices[1].index) - verts->at(indices[0].index),
            verts->at(indices[2].index) - verts->at(indices[0].index)));

        auto worldNormal = glm::dmat3(to_glm(closest->get()->localToWorld)) * glm::dvec3(to_glm(normal));

        TerrainIntersection result;
        result.point = GeoPoint(terrain.renderingSRS, closest->get()->worldIntersection);
        result.normal = glm::normalize(worldNormal);
        return result;
    }
}

Result<glm::dvec2>
TerrainNode::localHeightRange(const GeoExtent& footprint, const glm::dmat4& worldToLocal) const
{
    if (!footprint.valid() || !renderingSRS.valid() || !_profileNodes)
        return Failure{};
    for (unsigned column = 0; column < 4u; ++column)
        for (unsigned row = 0; row < 4u; ++row)
            if (!std::isfinite(worldToLocal[column][row]))
                return Failure{};

    for (const auto& child : _profileNodes->children)
    {
        auto* profileNode = child->cast<TerrainProfileNode>();
        if (!profileNode)
            continue;
        const auto query = footprint.transform(profileNode->profile.srs());
        if (!query.valid() || !profileNode->profile.extent().contains(query))
            continue;

        glm::dvec2 range(DBL_MAX, -DBL_MAX);
        double coveredArea = 0.0;
        for (const auto& root : profileNode->children)
        {
            auto* tile = root->cast<TerrainTileNode>();
            if (!tile || !accumulateHeightRange(*tile, query, worldToLocal, range, coveredArea))
                return Failure{};
        }
        const double queryArea = query.width() * query.height();
        if (queryArea > 0.0 && std::isfinite(queryArea) &&
            coveredArea >= queryArea * (1.0 - 1e-9) && range.x <= range.y)
            return range;
    }
    return Failure{};
}

Result<TerrainIntersection>
TerrainNode::intersect(const GeoPoint& input) const
{
    GeoPoint world = input.transform(renderingSRS);
    if (!finitePoint(world))
        return Failure{};

    if (renderingSRS.isGeocentric())
        return intersectSegment(*this, to_vsg(world) * 2.0, vsg::dvec3(0.0, 0.0, 0.0));
    else
        return intersectSegment(*this, { world.x, world.y, 1e6 }, { world.x, world.y, -1e6 });
}

Result<TerrainIntersection>
TerrainNode::intersectVertical(const GeoPoint& input) const
{
    if (!finitePoint(input) || !renderingSRS.valid())
        return Failure{};

    // On an ellipsoid, varying geodetic height follows the surface normal, not the center-to-point radius.
    // On a projected map, keep map XY fixed instead. Neither ray depends on the anchor's current altitude.
    const auto querySRS = renderingSRS.isGeocentric() ? renderingSRS.geodeticSRS() : renderingSRS;
    auto location = input.transform(querySRS);
    if (!finitePoint(location))
        return Failure{};

    location.z = 1e6;
    auto start = location.transform(renderingSRS);
    location.z = -1e6;
    auto end = location.transform(renderingSRS);
    if (!finitePoint(start) || !finitePoint(end))
        return Failure{};

    return intersectSegment(*this, to_vsg(start), to_vsg(end));
}
