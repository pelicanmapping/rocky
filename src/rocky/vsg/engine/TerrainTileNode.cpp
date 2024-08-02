/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TerrainTileNode.h"
#include "SurfaceNode.h"
#include "Runtime.h"
#include "TerrainEngine.h"

#include <rocky/vsg/TerrainSettings.h>

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
    const glm::dmat4 scaleBias[4] =
    {
        glm::dmat4(0.5,0,0,0, 0,0.5,0,0, 0,0,1.0,0, 0.0,0.5,0,1.0),
        glm::dmat4(0.5,0,0,0, 0,0.5,0,0, 0,0,1.0,0, 0.5,0.5,0,1.0),
        glm::dmat4(0.5,0,0,0, 0,0.5,0,0, 0,0,1.0,0, 0.0,0.0,0,1.0),
        glm::dmat4(0.5,0,0,0, 0,0.5,0,0, 0,0,1.0,0, 0.5,0.0,0,1.0)
    }; 
}

TerrainTileNode::TerrainTileNode(
    const TileKey& in_key,
    TerrainTileNode* in_parent,
    vsg::ref_ptr<vsg::Node> in_geometry,
    const SRS& worldSRS,
    const TerrainTileDescriptors& in_initialDescriptors,
    TerrainTileHost* in_host,
    Runtime& runtime)
{
    key = in_key;
    renderModel.descriptors = in_initialDescriptors;
    _host = in_host;

    doNotExpire = (in_parent == nullptr);
    revision = 0;
    surface = nullptr;
    stategroup = nullptr;
    lastTraversalFrame = 0;
    lastTraversalRange = FLT_MAX;
    _needsSubtiles = false;
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

TerrainTileNode::~TerrainTileNode()
{
    //Log::info() << "Destructing " << key.str() << std::endl;
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
TerrainTileNode::setElevation(shared_ptr<Image> image, const glm::dmat4& matrix)
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
    auto& vp = state->_commandBuffer->viewDependentState->viewportData->at(0);
    auto min_screen_height_ratio = (_host->settings().tilePixelSize + _host->settings().screenSpaceError) / vp[3];
    auto d = state->lodDistance(bound);
    return (d > 0.0) && (bound.r > (d * min_screen_height_ratio));
}

void
TerrainTileNode::accept(vsg::RecordTraversal& rv) const
{
    auto frame = rv.getFrameStamp()->frameCount;

    // is this a new frame (since the last time we were here)?
    auto new_frame = lastTraversalFrame.exchange(frame) != frame;

    // swap out the range; used for page out
    lastTraversalRange.exchange(std::min(
        (float)(new_frame ? FLT_MAX : (float)lastTraversalRange),
        (float)distanceTo(bound.center, rv.getState())));

    // swap out the time; used for page out
    lastTraversalTime.exchange(rv.getFrameStamp()->time);

    if (subtilesExist())
        _needsSubtiles = false;

    if (surface->isVisible(rv.getState()))
    {
        // determine whether we can and should subdivide to a higher resolution:
        bool subtilesInRange = shouldSubDivide(rv.getState());

        if (subtilesInRange && subtilesExist())
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

            if (subtilesInRange && subtilesLoader.empty())
            {
                _needsSubtiles = true;
            }
        }
    }

#ifndef AGGRESSIVE_PAGEOUT
    if (subtilesExist())
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
TerrainTileNode::unloadSubtiles(Runtime& runtime)
{
    // Feed the children to the garbage disposal before removing them
    // so any vulkan objects are safely destroyed
    if (children.size() > 1)
        runtime.dispose(children[1]);

    children.resize(1);
    subtilesLoader.reset();
    _needsSubtiles = false;
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
