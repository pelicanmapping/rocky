/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TerrainTileNode.h"
#include "SurfaceNode.h"
#include "TerrainEngine.h"
#include "TerrainSettings.h"

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

void
TerrainTileNode::accept(vsg::RecordTraversal& rv) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(host != nullptr, void());

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
    {
        needsSubtiles = false;
    }

    //auto* host = rv.getObject<TerrainTileHost>("rocky.terraintilehost");

    if (surface->isVisible(rv))
    {
        auto state = rv.getState();

        // should we subdivide?
        auto& vp = state->_commandBuffer->viewDependentState->viewportData->at(0);
        auto min_screen_height_ratio = (host->settings().tilePixelSize + host->settings().screenSpaceError) / vp[3];
        auto d = state->lodDistance(bound);
        bool subtilesInRange = (d > 0.0) && (bound.r > (d * min_screen_height_ratio));

        // TODO: someday, when we support orthographic cameras, look at this approach 
        // that would theoritically keep the same LOD across the visible scene:
        //double tile_height = surface->localbbox.max.y - surface->localbbox.min.y;
        //return (d > 0.0) && (tile_height > (d * min_screen_height_ratio));

        if (subtilesInRange && subtilesExist())
        {
            // children are available, traverse them now.
            children[1]->accept(rv);

#ifdef AGGRESSIVE_PAGEOUT
            // always ping all children at once so the system can never
            // delete one of a quad.
            host->ping(subTile(0), this, rv);
            host->ping(subTile(1), this, rv);
            host->ping(subTile(2), this, rv);
            host->ping(subTile(3), this, rv);
#endif
        }
        else
        {
            // children do not exist or are out of range; use this tile's geometry
            children[0]->accept(rv);

            if (subtilesInRange && subtilesLoader.empty())
            {
                needsSubtiles = true;
            }
        }
    }

#ifndef AGGRESSIVE_PAGEOUT
    if (subtilesExist())
    {
        // always ping all children at once so the system can never
        // delete one of a quad.
        host->ping(subTile(0), this, rv);
        host->ping(subTile(1), this, rv);
        host->ping(subTile(2), this, rv);
        host->ping(subTile(3), this, rv);
    }
#endif

    // keep this tile alive if requested
    if (doNotExpire)
    {
        host->ping(const_cast<TerrainTileNode*>(this), nullptr, rv);
    }
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
        surface->setElevation(renderModel.elevation.image, renderModel.elevation.matrix);
        bound = surface->recomputeBound();
    }
}
