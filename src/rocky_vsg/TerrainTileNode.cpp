/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TerrainTileNode.h"
#include "TerrainContext.h"
#include "GeometryPool.h"
#include "Merger.h"
#include "SelectionInfo.h"
#include "SurfaceNode.h"
#include "TerrainSettings.h"
#include "TileDrawable.h"
#include "TileNodeRegistry.h"
#include "Utils.h"

#include <rocky/Math.h>
#include <rocky/Notify.h>
#include <rocky/ImageLayer.h>

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
    const TileDescriptorModel& in_initialDescriptorModel,
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


#if 0
void
TerrainTileNode::initializeData(TerrainContext& terrain)
{
    // Initialize the data model by copying the parent's rendering data
    // and scale/biasing the matrices.

    auto parent = getParentTile();
    if (parent)
    {
        unsigned quadrant = getKey().getQuadrant();

        auto bindings = terrain.bindings;

        for(auto& parentPass : parent->_renderModel._passes)
        {
            // If the key is now out of the layer's valid min/max range, skip this pass.
            if (!passInLegalRange(parentPass))
                continue;

            // Copy the parent pass:
            _renderModel._passes.push_back(parentPass);
            RenderingPass& myPass = _renderModel._passes.back();

            // Scale/bias each matrix for this key quadrant.
            for(auto& sampler : myPass.samplers())
            {
                sampler._matrix = pre_mult(sampler._matrix, scaleBias[quadrant]);
                //sampler._matrix.preMult(scaleBias[quadrant]);
            }

            // Are we using image blending? If so, initialize the color_parent 
            // to the color texture.
            if (bindings[SamplerBinding::COLOR_PARENT].isActive())
            {
                myPass.samplers()[SamplerBinding::COLOR_PARENT] = myPass.samplers()[SamplerBinding::COLOR];
            }
        }

        // Copy the parent's shared samplers and scale+bias each matrix to the new quadrant:
        _renderModel._sharedSamplers = parent->_renderModel._sharedSamplers;

        for(auto& sampler : _renderModel._sharedSamplers)
        {
            sampler._matrix = pre_mult(sampler._matrix, scaleBias[quadrant]);
            //sampler._matrix.preMult(scaleBias[quadrant]);
        }

        // Use the elevation sampler to initialize the elevation raster
        // (used for primitive functors, intersection, etc.)
        if (bindings[SamplerBinding::ELEVATION].isActive())
        {
            updateElevationRaster();
        }
    }

    // tell the world.
    //OE_DEBUG << LC << "notify (create) key " << getKey().str() << std::endl;
    //terrain.tiles.notifyTileUpdate(getKey(), this);
}
#endif

#if 0
bool
TerrainTileNode::isDormant() const
{
    ROCKY_TODO("");
    return false;
#if 0
    const unsigned minMinExpiryFrames = 3u;
    unsigned frame = engine->getClock()->getFrame();
    double now = engine->getClock()->getTime();

    bool dormant = 
        frame - _lastTraversalFrame > std::max(options().minExpiryFrames().get(), minMinExpiryFrames) &&
        now - _lastTraversalTime > options().minExpiryTime().get();

    return dormant;
#endif
}

bool
TerrainTileNode::areSiblingsDormant() const
{
    auto parent = getParentTile();
    return parent ? parent->areSubTilesDormant() : true;
}

bool
TerrainTileNode::areSubTilesDormant() const
{
    return
        hasChildren() &&
        subTile(0)->isDormant() &&
        subTile(1)->isDormant() &&
        subTile(2)->isDormant() &&
        subTile(3)->isDormant();
}
#endif

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

    //const Sampler& elev = _renderModel._sharedSamplers[SamplerBinding::ELEVATION];
    //if (elev._texture)
    //    setElevation(elev._image, elev._matrix);
    //else
    //    setElevation(nullptr, dmat4(1));
}

void
TerrainTileNode::refreshAllLayers()
{
    //refreshLayers(CreateTileManifest(), terrain);
}

void
TerrainTileNode::refreshLayers(
    const CreateTileManifest& manifest)
{
#if 0
    auto operation = std::make_shared<LoadTileDataOperation>(
        terrain.map,
        manifest,
        this);

    _loadQueue.lock();
    _loadQueue.push(operation);
    _loadsInQueue = _loadQueue.size();
    if (_loadsInQueue > 0)
        _nextLoadManifestPtr = &_loadQueue.front()->_manifest;
    else
        _nextLoadManifestPtr = nullptr;
    _loadQueue.unlock();
#endif
}

#if 0
void
TerrainTileNode::releaseGLObjects(osg::State* state) const
{
    osg::Group::releaseGLObjects(state);

    if ( _surface.valid() )
        _surface->releaseGLObjects(state);

    _renderModel.releaseGLObjects(state);
}

void
TerrainTileNode::resizeGLObjectBuffers(unsigned maxSize)
{
    osg::Group::resizeGLObjectBuffers(maxSize);

    if ( _surface.valid() )
        _surface->resizeGLObjectBuffers(maxSize);

    _renderModel.resizeGLObjectBuffers(maxSize);
}
#endif

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
TerrainTileNode::update(const vsg::FrameStamp*)
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

#if 1

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

    //_needsData = true;

    //refreshStateGroup(terrain);
}

void
TerrainTileNode::merge(
    const TerrainTileModel& tile_model,
    const CreateTileManifest& manifest,
    shared_ptr<TerrainContext> terrain)
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

    //refreshStateGroup(terrain);

    // NEEDS STATE UPDATE?? _needsState = true??
}

//void
//TerrainTileNode::refreshStateGroup(TerrainContext& terrain)
//{
//    terrain.stateFactory.refreshStateGroup(this);
//}
//
#else
void
TerrainTileNode::merge(
    const TerrainTileModel& model,
    const CreateTileManifest& manifest,
    TerrainContext& terrain)
{
    bool newElevationData = false;
    const RenderBindings& bindings = terrain.bindings;
    RenderingPasses& myPasses = _renderModel._passes;
    std::unordered_set<UID> uidsLoaded;

    // if terrain constraints are in play, regenerate the tile's geometry.
    // this could be kinda slow, but meh, if you are adding and removing
    // constraints, frame drops are not a big concern
    if (manifest.includesConstraints())
    {
        // todo: progress callback here? I believe progress gets
        // checked before merge() anyway.
        createGeometry(terrain, nullptr);
    }

    // First deal with the rendering passes (for color data):
    const SamplerBinding& color = bindings[SamplerBinding::COLOR];
    if (color.isActive())
    {
        // loop over all the layers included in the new data model and
        // add them to our render model (or update them if they already exist)
        for(const auto& colorLayer : model.colorLayers)
        {
            auto& layer = colorLayer.layer;
            if (!layer)
                continue;

            // Look up the parent pass in case we need it
            RenderingPass* pass = 
                _renderModel.getPass(layer->getUID());

            const RenderingPass* parentPass =
                getParentTile() ? getParentTile()->_renderModel.getPass(layer->getUID()) :
                nullptr;

            // ImageLayer?
            auto imageLayer = ImageLayer::cast(layer);
            //ImageLayer* imageLayer = dynamic_cast<ImageLayer*>(layer.get());
            if (imageLayer)
            {
                bool isNewPass = (pass == nullptr);
                if (isNewPass)
                {
                    // Pass didn't exist here, so add it now.
                    pass = &_renderModel.addPass();
                    pass->setLayer(layer);
                }

                pass->setSampler(
                    SamplerBinding::COLOR,
                    colorLayer.image.getImage(),
                    colorLayer.matrix,
                    colorLayer.revision);
                    //colorLayer.texture(),
                    //colorLayer.matrix(),
                    //colorLayer.revision());

                // If this is a new rendering pass, just copy the color into the color-parent.
                if (isNewPass && bindings[SamplerBinding::COLOR_PARENT].isActive())
                {
                    pass->sampler(SamplerBinding::COLOR_PARENT) = pass->sampler(SamplerBinding::COLOR);
                }

                // check to see if this data requires an image update traversal.
                //if (_imageUpdatesActive == false)
                //{
                //    _imageUpdatesActive = colorLayer.texture()->needsUpdates();
                //}

                if (imageLayer->getAsyncLoading())
                {
                    if (parentPass)
                    {
                        pass->inheritFrom(*parentPass, scaleBias[_key.getQuadrant()]);

                        if (bindings[SamplerBinding::COLOR_PARENT].isActive())
                        {
                            Sampler& colorParent = pass->sampler(SamplerBinding::COLOR_PARENT);
                            colorParent._texture = parentPass->sampler(SamplerBinding::COLOR)._texture;
                            colorParent._matrix = parentPass->sampler(SamplerBinding::COLOR)._matrix;
                            colorParent._matrix = pre_mult(colorParent._matrix, scaleBias[_key.getQuadrant()]);
                            //colorParent._matrix.preMult(scaleBias[_key.getQuadrant()]);
                        }
                    }
                    else
                    {
                        // note: this can happen with an async layer load
                        //OE_DEBUG << "no parent pass in my pass. key=" << model->key().str() << std::endl;
                    }

#if 0
                    // check whether it's actually a futuretexture.
                    // if it's not, it is likely an empty texture and we'll ignore it
                    if (dynamic_cast<FutureTexture*>(colorLayer.texture()->osgTexture().get()))
                    {
       
                    }                 pass->sampler(SamplerBinding::COLOR)._futureTexture = colorLayer.texture();
#endif

                    // require an update pass to process the future texture
                    _imageUpdatesActive = true;
                }

                uidsLoaded.insert(pass->sourceUID());
            }

            else // non-image color layer (like splatting, e.g.)
            {
                if (!pass)
                {
                    pass = &_renderModel.addPass();
                    pass->setLayer(colorLayer.layer);
                }

                uidsLoaded.insert(pass->sourceUID());
            }
        }

        // Next loop over all the passes that we OWN, we asked for, but we didn't get.
        // That means they no longer exist at this LOD, and we need to convert them
        // into inherited samplers (or delete them entirely)
        for(int p=0; p<myPasses.size(); ++p)
        {
            RenderingPass& myPass = myPasses[p];

            if (myPass.ownsTexture() && 
                manifest.includes(myPass.layer().get()) &&
                uidsLoaded.count(myPass.sourceUID()) == 0)
            {
                //ROCKY_DEBUG << LC << "Releasing orphaned layer " << myPass.layer()->getName() << std::endl;

                // release the GL objects associated with this pass.
                // taking this out...can cause "flashing" issues
                //myPass.releaseGLObjects(NULL);
                
                bool deletePass = true;

                TerrainTileNode* parent = getParentTile();
                if (parent)
                {
                    const RenderingPass* parentPass = parent->_renderModel.getPass(myPass.sourceUID());
                    if (parentPass)
                    {
                        myPass.inheritFrom(*parentPass, scaleBias[_key.getQuadrant()]);
                        deletePass = false;
                    }
                }

                if (deletePass)
                {
                    myPasses.erase(myPasses.begin() + p);
                    --p;
                }
            }
        }
    }

    // Elevation data:
    const SamplerBinding& elevation = bindings[SamplerBinding::ELEVATION];
    if (elevation.isActive())
    {
        if (model.elevation.heightfield.valid()) //.texture())
        {
            _renderModel.setSharedSampler(
                SamplerBinding::ELEVATION,
                model.elevation.heightfield.getHeightfield(),
                model.elevation.revision);

            updateElevationRaster();

            newElevationData = true;
        }

        else if (
            manifest.includesElevation() && 
            _renderModel._sharedSamplers[SamplerBinding::ELEVATION].ownsTexture())
        {
            // We OWN elevation data, requested new data, and didn't get any.
            // That means it disappeared and we need to delete what we have.
            inheritSharedSampler(SamplerBinding::ELEVATION);

            updateElevationRaster();

            newElevationData = true;
        }
    } 

    // Normals:
    const SamplerBinding& normals = bindings[SamplerBinding::NORMAL];
    if (normals.isActive())
    {
        if (model.normalMap.image.valid())
        {
            _renderModel.setSharedSampler(
                SamplerBinding::NORMAL,
                model.normalMap.image.getImage(),
                model.normalMap.revision);

            updateNormalMap(terrain);
        }

        // If we OWN normal data, requested new data, and didn't get any,
        // that means it disappeared and we need to delete what we have:
        else if (
            manifest.includesElevation() && // not a typo, check for elevation
            _renderModel._sharedSamplers[SamplerBinding::NORMAL].ownsTexture())
        {
            inheritSharedSampler(SamplerBinding::NORMAL);
            updateNormalMap(terrain);
        }
    }

#if 0
    // Land Cover:
    const SamplerBinding& landCover = bindings[SamplerBinding::LANDCOVER];
    if (landCover.isActive())
    {
        if (model->landCover().texture())
        {
            _renderModel.setSharedSampler(
                SamplerBinding::LANDCOVER,
                model->landCover().texture(),
                model->landCover().revision());
        }

        else if (
            manifest.includesLandCover() &&
            _renderModel._sharedSamplers[SamplerBinding::LANDCOVER].ownsTexture())
        {
            // We OWN landcover data, requested new data, and didn't get any.
            // That means it disappeared and we need to delete what we have.
            inheritSharedSampler(SamplerBinding::LANDCOVER);
        }
    }
#endif

    // Other Shared Layers:
    uidsLoaded.clear();

#if 0
    for(unsigned index : model->sharedLayerIndices())
    {
        auto& sharedLayer = model->colorLayers()[index];
        if (sharedLayer.texture())
        {
            // locate the shared binding corresponding to this layer:
            UID uid = sharedLayer.layer()->getUID();
            unsigned bindingIndex = INT_MAX;
            for(unsigned i=SamplerBinding::SHARED; i<bindings.size() && bindingIndex==INT_MAX; ++i)
            {
                if (bindings[i].isActive() && bindings[i].sourceUID().isSetTo(uid))
                {
                    bindingIndex = i;
                }
            }

            if (bindingIndex < INT_MAX)
            {
                _renderModel.setSharedSampler(
                    bindingIndex,
                    sharedLayer.texture(),
                    sharedLayer.revision());
                //osg::Texture* tex = layerModel->getTexture();
                //int revision = layerModel->getRevision();
                //_renderModel.setSharedSampler(bindingIndex, tex, revision);

                uidsLoaded.insert(uid);
            }
        }
    }

    // Look for shared layers we need to remove because we own them,
    // requested them, and didn't get updates for them:
    for(unsigned i=SamplerBinding::SHARED; i<bindings.size(); ++i)
    {
        if (bindings[i].isActive() && 
            manifest.includes(bindings[i].sourceUID().get()) &&
            !uidsLoaded.contains(bindings[i].sourceUID().get()))
        {
            inheritSharedSampler(i);
        }
    }
#endif

    // Propagate changes we made down to this tile's children.
    if (hasChildren())
    {
        for (int i = 0; i < 4; ++i)
        {
            auto child = static_cast<vsg::Group*>(children[1].get())->children[i];
            //TerrainTileNode* child = getSubTile(i);
            if (child)
            {
                //child->refreshInheritedData(this, bindings);
            }
        }
    }

#if 0
    if (newElevationData)
    {
        terrain.notifyTileUpdate(getKey(), this);
    }
#endif

    // Bump the data revision for the tile.
    ++_revision;
}

void TerrainTileNode::inheritSharedSampler(int binding)
{
    TerrainTileNode* parent = getParentTile();
    if (parent)
    {
        Sampler& mySampler = _renderModel._sharedSamplers[binding];
        mySampler = parent->_renderModel._sharedSamplers[binding];
        if (mySampler._texture)
            mySampler._matrix = pre_mult(mySampler._matrix, scaleBias[_key.getQuadrant()]);
            //mySampler._matrix.preMult(scaleBias[_key.getQuadrant()]);
    }
    else
    {
        _renderModel.clearSharedSampler(binding);
    }

    // Bump the data revision for the tile.
    ++_revision;
}

void
TerrainTileNode::refreshSharedSamplers(const RenderBindings& bindings)
{    
    for (unsigned i = 0; i < _renderModel._sharedSamplers.size(); ++i)
    {
        if (bindings[i].isActive() == false)
        {
            _renderModel.clearSharedSampler(i);
        }
    }
}
#endif

void
TerrainTileNode::refreshInheritedData(const TerrainTileNode* parent)
{
#if 0
    // Run through this tile's rendering data and re-inherit textures and matrixes
    // from the parent. When a TerrainTileNode gets new data (via a call to merge), any
    // children of that tile that are inheriting textures or matrixes need to 
    // refresh to inherit that new data. In turn, those tile's children then need
    // to update as well. This method does that.

    // which quadrant is this tile in?
    unsigned quadrant = getKey().getQuadrant();

    // Count the number of changes we make so we can stop early if it's OK.
    unsigned changes = 0;

    RenderingPasses& parentPasses = parent->_renderModel._passes;
    RenderingPasses& myPasses = _renderModel._passes;

    // Delete any inherited pass whose parent pass no longer exists:
    for(int p=0; p<myPasses.size(); ++p)
    {
        RenderingPass& myPass = myPasses[p];
        if (myPass.inheritsTexture())
        {
            RenderingPass* myParentPass = parent->_renderModel.getPass(myPass.sourceUID());
            if (myParentPass == nullptr)
            {
                //OE_WARN << _key.str() << " removing orphaned pass " << myPass.sourceUID() << std::endl;
                myPasses.erase(myPasses.begin()+p);
                --p;
                ++changes;
            }
        }
    }

    // Look for passes in the parent that need to be inherited by this node.
    for(auto& parentPass : parentPasses)
    {
        // the corresponsing pass in this node:
        RenderingPass* myPass = _renderModel.getPass(parentPass.sourceUID());

        // Inherit the samplers for this pass.
        if (myPass)
        {
            // Handle the main color:
            if (bindings[SamplerBinding::COLOR].isActive())
            {
                Sampler& mySampler = myPass->sampler(SamplerBinding::COLOR);
                if (mySampler.inheritsTexture())
                {
                    mySampler.inheritFrom(parentPass.sampler(SamplerBinding::COLOR), scaleBias[quadrant]);
                    ++changes;
                }
            }

            // Handle the parent color. This is special case -- the parent
            // sampler is always set to the parent's color sampler with a
            // one-time scale/bias.
            if (bindings[SamplerBinding::COLOR_PARENT].isActive())
            {
                Sampler& mySampler = myPass->sampler(SamplerBinding::COLOR_PARENT);
                const Sampler& parentColorSampler = parentPass.sampler(SamplerBinding::COLOR);
                dmat4 newMatrix = parentColorSampler._matrix;
                newMatrix = pre_mult(newMatrix, scaleBias[quadrant]);
                //newMatrix.preMult(scaleBias[quadrant]);

                // Did something change?
                if (mySampler._texture != parentColorSampler._texture ||
                    mySampler._matrix != newMatrix ||
                    mySampler._revision != parentColorSampler._revision)
                {
                    if (parentColorSampler._texture && passInLegalRange(parentPass))
                    {
                        // set the parent-color texture to the parent's color texture
                        // and scale/bias the matrix.
                        mySampler._texture = parentColorSampler._texture;
                        mySampler._matrix = newMatrix;
                        mySampler._revision = parentColorSampler._revision;
                    }
                    else
                    {
                        // parent has no color texture? Then set our parent-color
                        // equal to our normal color texture.
                        mySampler = myPass->sampler(SamplerBinding::COLOR);
                    }
                    ++changes;
                }
            }
        }
        else
        {
            // Pass exists in the parent node, but not in this node, so add it now.
            if (passInLegalRange(parentPass))
            {
                myPass = &_renderModel.addPass();
                myPass->inheritFrom(parentPass, scaleBias[quadrant]);
                ++changes;
            }
        }
    }

    // Update all the shared samplers (elevation, normal, etc.)
    const Samplers& parentSharedSamplers = parent->_renderModel._sharedSamplers;
    Samplers& mySharedSamplers = _renderModel._sharedSamplers;

    for (unsigned binding=0; binding<parentSharedSamplers.size(); ++binding)
    {        
        Sampler& mySampler = mySharedSamplers[binding];

        if (mySampler.inheritsTexture())
        {
            mySampler.inheritFrom(parentSharedSamplers[binding], scaleBias[quadrant]);
            ++changes;

            // Update the local elevation raster cache (for culling and intersection testing).
            if (binding == SamplerBinding::ELEVATION)
            {
                //osg::Image* raster = mySampler._texture.valid() ? mySampler._texture->getImage(0) : NULL;
                //this->setElevationRaster(raster, mySampler._matrix);
                updateElevationRaster();
            }
        }
    }

    if (changes > 0)
    {
        // Bump the data revision for the tile.
        ++_revision;

        //dirtyBound(); // only for elev/patch changes maybe?

#if 0
        if (hasChildren())
        {
            for (auto& child : static_cast<vsg::Group*>(children[1].get())->children)
                child->refreshInheritedData(bindings);
        }
#endif
    }
#endif
}

#if 0
bool
TerrainTileNode::passInLegalRange(const RenderingPass& pass) const
{
    return 
        pass.tileLayer() == nullptr ||
        pass.tileLayer()->isKeyInVisualRange(getKey());
}
#endif

#if 0
void
TerrainTileNode::load(
    vsg::State* state,
    TerrainContext& terrain) const
{    
    int lod = getKey().getLOD();
    int numLods = terrain.selectionInfo.getNumLODs();
    
    // LOD priority is in the range [0..numLods]
    float lodPriority = (float)lod;

    // dist priority is in the range [0..1]
    float distance = state->lodDistance(vsg::sphere(bound.center, 0.0f));
    //float distance = culler->getDistanceToViewPoint(getBound().center(), true);
    int nextLOD = std::max(0, lod - 1);
    float maxRange = terrain.selectionInfo.getLOD(nextLOD)._visibilityRange;
    float distPriority = 1.0 - distance/maxRange;

    // add them together, and you get tiles sorted first by lodPriority
    // (because of the biggest range), and second by distance.
    float priority = lodPriority + distPriority;

    //OE_WARN << "key " << _key.str() << " pri=" << priority << std::endl;

    // set atomically
    _loadPriority = priority;

    // Check the status of the load
    util::ScopedLock lock(_loadQueue);

    if (_loadQueue.empty() == false)
    {
        LoadTileDataOperation::Ptr& op = _loadQueue.front();

        if (op->_result.isAbandoned())
        {
            // Actually this means that the task has not yet been dispatched,
            // so assign the priority and do it now.
            op->dispatch();
        }

        else if (op->_result.isAvailable())
        {
            // The task completed, so submit it to the merger.
            // (We can't merge here in the CULL traversal)
            terrain.merger.merge(op); // (op, *culler);
            _loadQueue.pop();
            _loadsInQueue = _loadQueue.size();
            if (!_loadQueue.empty())
                _nextLoadManifestPtr = &_loadQueue.front()->_manifest;
            else
                _nextLoadManifestPtr = nullptr;
        }
    }
}
#endif

void
TerrainTileNode::loadSync(shared_ptr<TerrainContext> terrain)
{
    LoadTileDataOperation::Ptr loadTileData =
        std::make_shared<LoadTileDataOperation>(terrain->map, this);

    loadTileData->setEnableCancelation(false);
    loadTileData->dispatch(false);
    loadTileData->merge(terrain);
}

void
TerrainTileNode::notifyOfArrival(
    TerrainTileNode* that,
    shared_ptr<TerrainContext> terrain)
{
    if (terrain->settings.normalizeEdges)
    {
        if (key.createNeighborKey(1, 0) == that->key)
            _eastNeighbor = vsg::observer_ptr<TerrainTileNode>(that);

        if (key.createNeighborKey(0, 1) == that->key)
            _southNeighbor = vsg::observer_ptr<TerrainTileNode>(that);

        updateNormalMap(terrain->settings);
    }
}

void
TerrainTileNode::updateNormalMap(
    const TerrainSettings& settings)
{
    if (settings.normalizeEdges == false)
        return;

#if 0
    Sampler& thisNormalMap = _renderModel._sharedSamplers[SamplerBinding::NORMAL];
    if (thisNormalMap.inheritsTexture() || !thisNormalMap._texture->osgTexture()->getImage(0))
        return;

    if (!_eastNeighbor.valid() || !_southNeighbor.valid())
        return;

    osg::ref_ptr<TerrainTileNode> east;
    if (_eastNeighbor.lock(east))
    {
        const Sampler& thatNormalMap = east->_renderModel._sharedSamplers[SamplerBinding::NORMAL];
        if (thatNormalMap.inheritsTexture() || !thatNormalMap._texture->osgTexture()->getImage(0))
            return;

        osg::Image* thisImage = thisNormalMap._texture->osgTexture()->getImage(0);
        osg::Image* thatImage = thatNormalMap._texture->osgTexture()->getImage(0);

        int width = thisImage->s();
        int height = thisImage->t();
        if ( width != thatImage->s() || height != thatImage->t() )
            return;

        // Just copy the neighbor's edge normals over to our texture.
        // Averaging them would be more accurate, but then we'd have to
        // re-generate each texture multiple times instead of just once.
        // Besides, there's almost no visual difference anyway.
        osg::Vec4 pixel;
        ImageUtils::PixelReader readThat(thatImage);
        ImageUtils::PixelWriter writeThis(thisImage);
        
        for (int t=0; t<height; ++t)
        {
            readThat(pixel, 0, t);
            writeThis(pixel, width-1, t);
            //writeThis(readThat(0, t), width-1, t);
        }

        thisImage->dirty();
    }

    osg::ref_ptr<TerrainTileNode> south;
    if (_southNeighbor.lock(south))
    {
        const Sampler& thatNormalMap = south->_renderModel._sharedSamplers[SamplerBinding::NORMAL];
        if (thatNormalMap.inheritsTexture() || !thatNormalMap._texture->osgTexture()->getImage(0))
            return;

        osg::Image* thisImage = thisNormalMap._texture->osgTexture()->getImage(0);
        osg::Image* thatImage = thatNormalMap._texture->osgTexture()->getImage(0);

        int width = thisImage->s();
        int height = thisImage->t();
        if ( width != thatImage->s() || height != thatImage->t() )
            return;

        // Just copy the neighbor's edge normals over to our texture.
        // Averaging them would be more accurate, but then we'd have to
        // re-generate each texture multiple times instead of just once.
        // Besides, there's almost no visual difference anyway.
        osg::Vec4 pixel;
        ImageUtils::PixelReader readThat(thatImage);
        ImageUtils::PixelWriter writeThis(thisImage);

        for (int s=0; s<width; ++s)
        {
            readThat(pixel, s, height-1);
            writeThis(pixel, s, 0);
        }

        thisImage->dirty();
    }
#endif
    //OE_INFO << LC << _key.str() << " : updated normal map.\n";
}
