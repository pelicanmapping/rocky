/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TerrainTileNode.h"
#include "TerrainContext.h"
#include "SurfaceNode.h"
#include "TerrainSettings.h"

#include <rocky/Math.h>
#include <rocky/Notify.h>
#include <rocky/ImageLayer.h>
#include <rocky/TerrainTileModel.h>

#include <vsg/vk/State.h>
#include <vsg/ui/FrameStamp.h>
#include <vsg/nodes/StateGroup.h>

using namespace rocky;

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
    const TerrainTileDescriptors& in_initialDescriptorModel,
    TerrainTileHost* in_host)
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
    _needsData = true;
    _needsChildren = false;
    _needsUpdate = false;
    
    if (in_geometry.valid())
    {
        // scene graph is: tile->surface->stateGroup->geometry
        // construct a state group for this tile's render model
        surface = SurfaceNode::create(key);

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
    auto[tw, th] = key.getProfile()->getNumTiles(key.getLOD());

    double x = (double)key.getTileX();
    double y = (double)(th - key.getTileY() - 1);

    _tileKeyValue = fvec4(
        (float)(x - tw / 2), //(int)fmod(x, m),
        (float)(y - th / 2), // (int)fmod(y, m),
        (float)key.getLOD(),
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
        bound = surface->boundingSphere;
    }
}

void
TerrainTileNode::setElevation(
    shared_ptr<Image> image,
    const dmat4& matrix)
{
    if (surface)
    {
        if (image != getElevationRaster() ||
            matrix != getElevationMatrix() ||
            !this->bound.valid())
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
    lastTraversalFrame.exchange(nv.getFrameStamp()->frameCount);
    lastTraversalTime.exchange(nv.getFrameStamp()->time);
    lastTraversalRange.exchange(std::min(
        (float)lastTraversalRange,
        (float)distanceTo(bound.center, nv.getState())));

    if (hasChildren())
        _needsChildren = false;

    // TODO: this is no good from a multi-threaded record perspective
    //_needsChildren = false;

    if (surface->isVisibleFrom(nv.getState()))
    {
        // determine whether we can and should subdivide to a higher resolution:
        bool childrenInRange = shouldSubDivide(nv.getState());

        if (childrenInRange && hasChildren())
        {
            // children are available, traverse them now.
            children[1]->accept(nv);
        }
        else
        {
            // children do not exist or are out of range; use this tile's geometry
            children[0]->accept(nv);

            if (childrenInRange && !childLoader.isWorking())
            {
                _needsChildren = true;
            }
        }
    }

    if (hasChildren())
    {
        // always ping all children at once so the system can never
        // delete one of a quad.
        _host->ping(
            subTile(0), subTile(1), subTile(2), subTile(3),
            nv);
    }

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
}

void
TerrainTileNode::update(const vsg::FrameStamp* fs, const IOOptions& io)
{
#if 0
    unsigned numUpdatedTotal = 0u;
    unsigned numFuturesResolved = 0u;

    for(auto& pass : _renderModel._passes)
    {
        for(auto& sampler : pass.samplers())
        {
            // handle "future" textures. This is a texture that was installed
            // by an "async" image layer that is working in the background
            // to load. Once it is available we can merge it into the real texture
            // slot for rendering.
            if (sampler._futureTexture)
            {
                FutureTexture* ft = dynamic_cast<FutureTexture*>(
                    sampler._futureTexture->osgTexture().get());

                if (ft->succeeded())
                {
                    sampler._texture = sampler._futureTexture;
                    sampler._futureTexture = nullptr;
                    sampler._matrix.makeIdentity();
                    ++numFuturesResolved;
                }
                else if (ft->failed())
                {
                    sampler._futureTexture = nullptr;
                    ++numFuturesResolved;
                }

                ++numUpdatedTotal;
            }

            if (sampler.ownsTexture() && sampler._texture->needsUpdates())
            {
                sampler._texture->update(nv);

                ++numUpdatedTotal;
            }
        }
    }

    // if no updates were detected, don't check next time.
    if (numUpdatedTotal == 0)
    {
        _imageUpdatesActive = false;
    }

    // if we resolve any future-textures, inform the children
    // that they need to update their inherited samplers.
    if (numFuturesResolved > 0)
    {
        for (int i = 0; i < 4; ++i)
        {
            if ((int)getNumChildren() > i)
            {
                TerrainTileNode* child = getSubTile(i);
                if (child)
                    child->refreshInheritedData(this, _context->getRenderBindings());
            }
        }
    }
#endif
}

void
TerrainTileNode::inherit()
{
    vsg::ref_ptr<TerrainTileNode> safe_parent = parent;
    if (safe_parent)
    {
        auto& sb = scaleBias[key.getQuadrant()];

        renderModel = safe_parent->renderModel;
        renderModel.applyScaleBias(sb);

        revision = safe_parent->revision;
    }
}

void
TerrainTileNode::merge(
    const TerrainTileModel& tile_model,
    const CreateTileManifest& manifest)
{
    //if (manifest.includesConstraints())
    //{
    //    createGeometry(terrain, nullptr);
    //}

    if (tile_model.colorLayers.size() > 0)
    {
        auto& layer = tile_model.colorLayers[0];
        if (layer.image.valid())
        {
            renderModel.color.image = layer.image.getImage();
            renderModel.color.matrix = layer.matrix;
            //_renderModel.descriptorModel.color = nullptr;
        }
    }

    if (tile_model.elevation.heightfield.valid())
    {
        renderModel.elevation.image = tile_model.elevation.heightfield.getHeightfield();
        renderModel.elevation.matrix = tile_model.elevation.matrix;
        //_renderModel.descriptorModel.elevation = nullptr;
    }

    if (tile_model.normalMap.image.valid())
    {
        renderModel.normal.image = tile_model.normalMap.image.getImage();
        renderModel.normal.matrix = tile_model.normalMap.matrix;
        //_renderModel.descriptorModel.normal = nullptr;
    }

    _needsData = false;

    //refreshStateGroup(terrain);

    // NEEDS STATE UPDATE?? _needsState = true??
}
