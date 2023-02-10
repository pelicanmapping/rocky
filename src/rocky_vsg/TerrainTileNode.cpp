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

using namespace ROCKY_NAMESPACE;

#define LC "[TerrainTileNode] "

namespace
{
    // Scale and bias matrices, one for each TileKey quadrant.
    const dmat4 scaleBias[4] =
    {
        dmat4(0.5f,0,0,0, 0,0.5f,0,0, 0,0,1.0f,0, 0.0f,0.5f,0,1.0f),
        dmat4(0.5f,0,0,0, 0,0.5f,0,0, 0,0,1.0f,0, 0.5f,0.5f,0,1.0f),
        dmat4(0.5f,0,0,0, 0,0.5f,0,0, 0,0,1.0f,0, 0.0f,0.0f,0,1.0f),
        dmat4(0.5f,0,0,0, 0,0.5f,0,0, 0,0,1.0f,0, 0.5f,0.0f,0,1.0f)
    }; 
}

TerrainTileNode::TerrainTileNode(
    const TileKey& in_key,
    TerrainTileNode* in_parent,
    vsg::ref_ptr<vsg::Node> in_geometry,
    const fvec2& in_morphConstants,
    float in_childrenVisibilityRange,
    const SRS& worldSRS,
    const TerrainTileDescriptors& in_initialDescriptorModel,
    TerrainTileHost* in_host,
    RuntimeContext& runtime)
{
    key = in_key;
    parent = in_parent;
    morphConstants = in_morphConstants;
    childrenVisibilityRange = in_childrenVisibilityRange;
    renderModel.descriptorModel = in_initialDescriptorModel;
    _host = in_host;

    doNotExpire = false;
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

        // empty state group - will populate later in refreshStateGroup
        stategroup = vsg::StateGroup::create();
        stategroup->addChild(in_geometry);

        surface->addChild(stategroup);

        if (children.empty())
            this->addChild(surface);
        else
            children[0] = surface;
    }

    // Encode the tile key in a uniform. Note! The X and Y components are presented
    // modulo 2^16 form so they don't overrun single-precision space.
    auto[tw, th] = key.profile().numTiles(key.levelOfDetail());

    double x = (double)key.tileX();
    double y = (double)(th - key.tileY() - 1);

    _tileKeyValue = fvec4(
        (float)(x - tw / 2), //(int)fmod(x, m),
        (float)(y - th / 2), // (int)fmod(y, m),
        (float)key.levelOfDetail(),
        -1.0f);

    // inherit model data from the parent
    inherit();

    // update the bounding sphere for culling
    recomputeBound();
}

TerrainTileNode::~TerrainTileNode()
{
    //nop
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

void
TerrainTileNode::updateElevationRaster()
{
    if (renderModel.elevation.texture)
        setElevation(renderModel.elevation.image, renderModel.elevation.matrix);
    else
        setElevation(nullptr, dmat4(1));
}

void
TerrainTileNode::refreshAllLayers()
{
    //todo
}

void
TerrainTileNode::refreshLayers(
    const CreateTileManifest& manifest)
{
    //todo
}

bool
TerrainTileNode::shouldSubDivide(vsg::State* state) const
{
    // can we subdivide at all?
    if (childrenVisibilityRange == FLT_MAX)
        return false;

    // are we inside the bounding sphere?
    //else if (distanceTo(bound.center, state) - bound.radius <= 0.0f)
    //    return true;

    // are the children in range?
    else
        return surface->anyChildBoxWithinRange(childrenVisibilityRange, state);

#if 0
        // In PSOS mode, subdivide when the on-screen size of a tile exceeds the maximum
        // allowable on-screen tile size in pixels.
        if (terrain.rangeMode() == osg::LOD::PIXEL_SIZE_ON_SCREEN)
        {
            float tileSizeInPixels = -1.0;

            //if (context->getEngine()->getComputeRangeCallback())
            //{
            //    tileSizeInPixels = (*context->getEngine()->getComputeRangeCallback())(this, *culler->_cv);
            //}    

            if (tileSizeInPixels <= 0.0)
            {
                tileSizeInPixels = _surface->getPixelSizeOnScreen(terrain);
            }
        
            float effectivePixelSize =
                terrain.tilePixelSize + terrain.screenSpaceError;
                //options().tilePixelSize().get() + options().screenSpaceError().get();

            return (tileSizeInPixels > effectivePixelSize);
        }

        // In DISTANCE-TO-EYE mode, use the visibility ranges precomputed in the SelectionInfo.
        else
        {
            float range = terrain.selectionInfo.getRange(_subdivideTestKey);
#if 1
            // slightly slower than the alternate block below, but supports a user overriding
            // CullVisitor::getDistanceToViewPoint -gw
            return _surface->anyChildBoxWithinRange(range, terrain); // *culler);
#else
            return _surface->anyChildBoxIntersectsSphere(
                culler->getViewPointLocal(), 
                range*range / culler->getLODScale());
#endif
        }
#endif
    //}
    //return false;
}

void
TerrainTileNode::accept(vsg::RecordTraversal& nv) const
{
    auto frame = nv.getFrameStamp()->frameCount;

    auto new_frame = lastTraversalFrame.exchange(frame) != frame;

    lastTraversalRange.exchange(std::min(
        (float)(new_frame ? FLT_MAX : lastTraversalRange),
        (float)distanceTo(bound.center, nv.getState())));

    lastTraversalTime.exchange(nv.getFrameStamp()->time);

    if (hasChildren())
        _needsChildren = false;

    if (surface->isVisible(nv.getState()))
    {
        // determine whether we can and should subdivide to a higher resolution:
        bool childrenInRange = shouldSubDivide(nv.getState());

        if (childrenInRange && hasChildren())
        {
            // children are available, traverse them now.
            children[1]->accept(nv);

            // always ping all children at once so the system can never
            // delete one of a quad. (A node can't ping itself because it
            // may be bounding-sphere culled.)
            _host->ping(
                subTile(0), subTile(1), subTile(2), subTile(3),
                nv);
        }
        else
        {
            // children do not exist or are out of range; use this tile's geometry
            children[0]->accept(nv);

            if (childrenInRange && !childLoader.working() && !childLoader.available())
            {
                _needsChildren = true;
            }
        }
    }

    // if we don't have a parent, we need to ping ourselves to say alive
    if (!parent.valid())
    {
        _host->ping(
            const_cast<TerrainTileNode*>(this), 
            nullptr, nullptr, nullptr,
            nv);
    }
}

void
TerrainTileNode::unloadChildren()
{
    children.resize(1);
    childLoader.reset();
    dataLoader.reset();
    dataMerger.reset();
    _needsChildren = true;
}

void
TerrainTileNode::update(const vsg::FrameStamp* fs, const IOOptions& io)
{
    //nop
}

void
TerrainTileNode::inherit()
{
    auto safe_parent = parent.ref_ptr();
    if (safe_parent)
    {
        auto& sb = scaleBias[key.getQuadrant()];

        renderModel = safe_parent->renderModel;
        renderModel.applyScaleBias(sb);

        revision = safe_parent->revision;

        // prompts regeneration of the local bounds
        setElevation(renderModel.elevation.image, renderModel.elevation.matrix);
    }
}
