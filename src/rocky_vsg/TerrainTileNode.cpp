/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TerrainTileNode.h"
#include "TerrainContext.h"
#include "SurfaceNode.h"
#include "TerrainSettings.h"
#include "RuntimeContext.h"

#include <rocky/Math.h>
#include <rocky/ImageLayer.h>
#include <rocky/TerrainTileModel.h>

#include <vsg/vk/State.h>
#include <vsg/ui/FrameStamp.h>
#include <vsg/nodes/StateGroup.h>
#include <vsg/state/ViewDependentState.h>

using namespace ROCKY_NAMESPACE;

#define LC "[TerrainTileNode] "

// uncomment to use SSE.
#define USE_SSE

// if you define this, the engine will be more aggressive about paging out tiles
// that are not in the frustum.
#define AGGRESSIVE_PAGEOUT

namespace
{
    // Scale and bias matrices, one for each TileKey quadrant.
    const dmat4 scaleBias[4] =
    {
        dmat4(0.5,0,0,0, 0,0.5,0,0, 0,0,1.0,0, 0.0,0.5,0,1.0),
        dmat4(0.5,0,0,0, 0,0.5,0,0, 0,0,1.0,0, 0.5,0.5,0,1.0),
        dmat4(0.5,0,0,0, 0,0.5,0,0, 0,0,1.0,0, 0.0,0.0,0,1.0),
        dmat4(0.5,0,0,0, 0,0.5,0,0, 0,0,1.0,0, 0.5,0.0,0,1.0)
    }; 
}

TerrainTileNode::TerrainTileNode(
    const TileKey& in_key,
    TerrainTileNode* in_parent,
    vsg::ref_ptr<vsg::Node> in_geometry,
    const fvec2& in_morphConstants,
    float in_childrenVisibilityRange,
    const SRS& worldSRS,
    const TerrainTileDescriptors& in_initialDescriptors,
    TerrainTileHost* in_host,
    RuntimeContext& runtime)
{
    key = in_key;
    morphConstants = in_morphConstants;
    childrenVisibilityRange = in_childrenVisibilityRange;
    renderModel.descriptors = in_initialDescriptors;
    _host = in_host;

    doNotExpire = (in_parent == nullptr);
    revision = 0;
    surface = nullptr;
    stategroup = nullptr;
    lastTraversalFrame = 0;
    lastTraversalRange = FLT_MAX;
    _needsChildren = false;
    _needsUpdate = false;
 
    ROCKY_HARD_ASSERT(in_geometry.valid());

    if (in_geometry.valid())
    {
        // scene graph is: tile->surface->stateGroup->geometry
        // construct a state group for this tile's render model
        surface = SurfaceNode::create(key, worldSRS, runtime);

        // empty state group - the registry will populate this later
        stategroup = vsg::StateGroup::create();
        stategroup->addChild(in_geometry);

        surface->addChild(stategroup);

        this->addChild(surface);
    }
}

void
TerrainTileNode::recomputeBound()
{
    if (surface.valid())
    {
        surface->recomputeBound();
        bound = surface->worldBoundingSphere;
    }
}

void
TerrainTileNode::setElevation(shared_ptr<Image> image, const dmat4& matrix)
{
    if (surface)
    {
        if (image != getElevationRaster() || matrix != getElevationMatrix() || !this->bound.valid())
        {
            surface->setElevation(image, matrix);
            recomputeBound();
        }
    }
}

bool
TerrainTileNode::shouldSubDivide(vsg::State* state) const
{
    // can we subdivide at all?
    if (childrenVisibilityRange == FLT_MAX)
        return false;

#ifdef USE_SSE

    const float TILE_SIZE_PIXELS = 256.0f;

    auto& vp = state->_commandBuffer->viewDependentState->viewportData->at(0);
    float min_screen_height_ratio = (TILE_SIZE_PIXELS + _host->settings().screenSpaceError) / vp[3];
    float d = state->lodDistance(bound);
    return (d > 0.0) && (bound.r > (d * min_screen_height_ratio));

#else

    // are the children in range?
    // Note - this method prefered when using morphing.
    return surface->anyChildBoxWithinRange(childrenVisibilityRange, state);

#endif
}

void
TerrainTileNode::accept(vsg::RecordTraversal& rv) const
{
    auto frame = rv.getFrameStamp()->frameCount;

    // is this a new frame (since the last time we were here)?
    auto new_frame = lastTraversalFrame.exchange(frame) != frame;

    // swap out the range; used for page out
    lastTraversalRange.exchange(std::min(
        (float)(new_frame ? FLT_MAX : lastTraversalRange),
        (float)distanceTo(bound.center, rv.getState())));

    // swap out the time; used for page out
    lastTraversalTime.exchange(rv.getFrameStamp()->time);

    if (hasChildren())
        _needsChildren = false;

    if (surface->isVisible(rv.getState()))
    {
        // determine whether we can and should subdivide to a higher resolution:
        bool childrenInRange = shouldSubDivide(rv.getState());

        if (childrenInRange && hasChildren())
        {
            // children are available, traverse them now.
            children[1]->accept(rv);

#ifdef AGGRESSIVE_PAGEOUT
            // always ping all children at once so the system can never
            // delete one of a quad.
            _host->ping(subTile(0), this, rv);
            _host->ping(subTile(1), this, rv);
            _host->ping(subTile(2), this, rv);
            _host->ping(subTile(3), this, rv);
#endif
        }
        else
        {
            // children do not exist or are out of range; use this tile's geometry
            children[0]->accept(rv);

            if (childrenInRange && childrenLoader.empty())
            {
                _needsChildren = true;
            }
        }
    }

#ifndef AGGRESSIVE_PAGEOUT
    if (hasChildren())
    {
        // always ping all children at once so the system can never
        // delete one of a quad.
        _host->ping(subTile(0), this, rv);
        _host->ping(subTile(1), this, rv);
        _host->ping(subTile(2), this, rv);
        _host->ping(subTile(3), this, rv);
    }
#endif

    // keep this tile alive if requested
    if (doNotExpire)
    {
        _host->ping(const_cast<TerrainTileNode*>(this), nullptr, rv);
    }
}

void
TerrainTileNode::unloadChildren()
{
    children.resize(1);
    childrenLoader.reset();
    elevationLoader.reset();
    elevationMerger.reset();
    dataLoader.reset();
    dataMerger.reset();
    _needsChildren = true;
}

void
TerrainTileNode::inheritFrom(vsg::ref_ptr<TerrainTileNode> parent)
{
    if (parent)
    {
        auto& sb = scaleBias[key.getQuadrant()];

        renderModel = parent->renderModel;
        renderModel.applyScaleBias(sb);

        revision = parent->revision;

        // prompts regeneration of the local bounds
        setElevation(renderModel.elevation.image, renderModel.elevation.matrix);
    }
}
