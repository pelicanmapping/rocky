/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky_vsg/Common.h>

#include <rocky/Image.h>
#include <rocky/TileLayer.h>

#include <vsg/state/Sampler.h>
#include <vsg/state/ImageInfo.h>

namespace rocky
{
    class TileRenderModel;

#if 0
    class TextureData
    {
    public:
        vsg::ref_ptr<vsg::ImageInfo> texture;
        fmat4 matrix{ 1 };
    };

    enum LayerType
    {
        ELEVATION,
        NORMAL,
        COLOR,
        COLOR_PARENT,
        NUM_LAYER_TYPES
    };

    class TileRenderData // ... new model???
    {
    public:
        std::vector<TextureData> slots { NUM_LAYER_TYPES };
        vsg::ref_ptr<vsg::DescriptorSet> descriptorSet;
        vsg::ref_ptr<vsg::Geometry> geometry;
    };
#endif

    /**
     * Defines the usage information for a single texture sampler.
     */
    class SamplerBinding
    {
    public:
        enum Usage
        {
            COLOR = 0,
            COLOR_PARENT = 1,
            ELEVATION = 2,
            NORMAL = 3,
            SHARED = 4   // non-core shared layers start at this index
        };

        optional<UID> sourceUID;
        optional<Usage> usage;
        optional<int> unit;
        optional<std::string> samplerName;
        optional<std::string> matrixName;

        //! True if this binding is bound to a teture image unit and therefore active
        bool isActive() const {
            return unit.has_value() || usage.has_value() || sourceUID.has_value();
        }
    };

    /**
     * Array of render bindings. This array is always indexed by the
     * SamplerBinding::USAGE itself. So for example, RenderBindings[0] is always
     * the COLOR usage, [2] is the ELEVATION usage, etc. Shared layer bindings
     * (i.e., custom samplers) start at index SHARED and go up from there.
     */
    using RenderBindings = std::vector<SamplerBinding>;

    /**
     * A single texture and its matrix.
     * This corresponds to a single SamplerBinding. If the texture matrix 
     * is non-identity, that means the sampler inherits the texture from
     * another sampler higher up in the scene graph.
     */
    class Sampler
    {
    public:
        //! Construct
        Sampler() 
            : _revision(0u) { }

        //! Construct
        Sampler(const Sampler& rhs) :
            // no _futuretexture
            _image(rhs._image),
            _texture(rhs._texture),
            _matrix(rhs._matrix),
            _revision(rhs._revision)
        {
            //nop
        }

        shared_ptr<Image> _image;

        // pointer to the sampler's active texture data
        //Texture::Ptr _texture;
        //Texture _texture;
        vsg::ref_ptr<vsg::ImageInfo> _texture;

        // scale and bias matrix for accessing the texture - will be non-identity
        // if the texture is inherited from an ancestor tile
        dmat4 _matrix{ 1 };

        // point to a texture whose image is still being loaded asynchronously
        //vsg::ref_ptr<vsg::ImageInfo> _futureTexture;
        //Texture::Ptr _futureTexture;

        // revision of the data in this sampler (taken from its source layer)
        unsigned _revision;

        //! True is this sampler is the rightful owner of _texture.
        inline bool ownsTexture() const { 
            return _texture && is_identity(_matrix); // .isIdentity();
        }

        //! True is this sampler is NOT the rightful owner of _texture.
        inline bool inheritsTexture() const {
            return !_texture || !is_identity(_matrix);
        }

        //! Revision of the data model use to initialize this sampler.
        inline unsigned revision() const { 
            return _revision;
        }

        //! Set this sampler's matrix to inherit
        inline void inheritFrom(const Sampler& rhs, const dmat4& scaleBias) {
            _texture = rhs._texture;
            _matrix = rhs._matrix;
            _revision = rhs._revision;
            // do NOT copy and overwrite _futureTexture!
            _matrix = pre_mult(_matrix, scaleBias);
            //_matrix *= scaleBIs
            //_matrix.preMult(scaleBias);
        }
    };
    //typedef AutoArray<Sampler> Samplers;
    using Samplers = std::vector<Sampler>;

    /**
     * A single rendering pass for color data.
     * Samplers (one per RenderBinding) specific to one rendering pass of a tile.
     * These are just the COLOR and COLOR_PARENT samplers.
     */
    class RenderingPass
    {
    public:
        RenderingPass() :
            _sourceUID(-1),
            _samplers(SamplerBinding::COLOR_PARENT+1),
            _visibleLayer(0L),
            _tileLayer(0L)
            { }

        RenderingPass(const RenderingPass& rhs) :
            _sourceUID(rhs._sourceUID),
            _samplers(rhs._samplers),
            _layer(rhs._layer),
            _visibleLayer(rhs._visibleLayer),
            _tileLayer(rhs._tileLayer)
        {
            //nop
        }

        ~RenderingPass()
        {
            //releaseGLObjects(nullptr);
        }
        
        // UID of the layer corresponding to this rendering pass
        UID sourceUID() const { return _sourceUID; }

        // the COLOR and COLOR_PARENT (optional) samplers for this rendering pass
        Samplers& samplers() { return _samplers; }
        const Samplers& samplers() const  { return _samplers; }
        
        Sampler& sampler(int binding) { return _samplers[binding]; }
        const Sampler& sampler(int binding) const { return _samplers[binding]; }

        Layer::constptr layer() const {
            return _layer;
        }

        VisibleLayer::constptr visibleLayer() const {
            return _visibleLayer;
        }

        TileLayer::constptr tileLayer() const {
            return _tileLayer;
        }

        // whether the color sampler in this rendering pass are native
        // or inherited from another tile
        bool ownsTexture() const { return _samplers[SamplerBinding::COLOR].ownsTexture(); }
        bool inheritsTexture() const { return !ownsTexture(); }

        void inheritFrom(const RenderingPass& rhs, const dmat4& scaleBias) {
            _sourceUID = rhs._sourceUID;
            _samplers = rhs._samplers;
            _layer = rhs._layer;
            _visibleLayer = rhs._visibleLayer;
            _tileLayer = rhs._tileLayer;

            for (unsigned s = 0; s < samplers().size(); ++s)
                sampler(s)._matrix = pre_mult(sampler(s)._matrix, scaleBias);
                //sampler(s)._matrix.preMult(scaleBias);
           
        }

        //void releaseGLObjects(osg::State* state) const
        //{
        //    for (unsigned s = 0; s < _samplers.size(); ++s)
        //    {
        //        const Sampler& sampler = _samplers[s];
        //        if (sampler.ownsTexture())
        //            sampler._texture->releaseGLObjects(state);
        //        if (sampler._futureTexture)
        //            sampler._futureTexture->releaseGLObjects(state);
        //    }
        //}

        //void resizeGLObjectBuffers(unsigned size)
        //{
        //    for (unsigned s = 0; s < _samplers.size(); ++s)
        //    {
        //        const Sampler& sampler = _samplers[s];
        //        if (sampler.ownsTexture())
        //            sampler._texture->resizeGLObjectBuffers(size);
        //        if (sampler._futureTexture)
        //            sampler._futureTexture->resizeGLObjectBuffers(size);
        //    }
        //}

        void setLayer(Layer::constptr layer) {
            _layer = layer;
            if (layer) {
                _visibleLayer = VisibleLayer::cast(layer); // dynamic_cast<const VisibleLayer*>(layer);
                _tileLayer = TileLayer::cast(layer); // dynamic_cast<const TileLayer*>(layer);
                _sourceUID = layer->getUID();
                for (unsigned s = 0; s<_samplers.size(); ++s) {
                    _samplers[s]._revision = layer->getRevision();
                }
            }
        }

        void setSampler(
            SamplerBinding::Usage binding,
            shared_ptr<Image> image,
            //vsg::ref_ptr<vsg::ImageInfo> texture,
            const dmat4& matrix,
            int sourceRevision)
        {
            Sampler& sampler = _samplers[binding];
            sampler._image = image;
            //sampler._texture = texture;
            sampler._matrix = matrix;
            sampler._revision = sourceRevision;
        }

    private:
        // UID of the layer responsible for this rendering pass (usually an ImageLayer)
        UID _sourceUID;

        /** Samplers specific to this rendering pass (COLOR, COLOR_PARENT) */
        Samplers _samplers;

        /** Layer respsonible for this rendering pass */
        shared_ptr<const Layer> _layer;

        /** VisibleLayer responsible for this rendering pass (is _layer is a VisibleLayer) */
        shared_ptr<const VisibleLayer> _visibleLayer;
        
        /** VisibleLayer responsible for this rendering pass (is _layer is a TileLayer) */
        shared_ptr<const TileLayer> _tileLayer;

        friend class TileRenderModel;
    };

    /**
     * Unordered collection of rendering passes.
     */
    typedef std::vector<RenderingPass> RenderingPasses;

    /**
     * Everything necessary to render a single terrain tile.
     * REX renders the terrain in multiple passes, one pass for each visible layer.
     */
    class TileRenderModel
    {
    public:
        /** Samplers that are bound for every rendering pass (elevation, normal map, etc.) */
        Samplers _sharedSamplers;

        /** Samplers bound for each visible layer (color) */
        RenderingPasses _passes;

        //DrawElementsIndirectBindlessCommandNV _command;

        /** Add a new rendering pass to the end of the list. */
        RenderingPass& addPass()
        {
            _passes.resize(_passes.size()+1);
            return _passes.back();
        }

        RenderingPass& copyPass(const RenderingPass& rhs)
        {
            _passes.push_back(rhs);
            return _passes.back();
        }

        /** Look up a rendering pass by the corresponding layer ID */
        const RenderingPass* getPass(UID uid) const
        {
            for (unsigned i = 0; i < _passes.size(); ++i) {
                if (_passes[i].sourceUID() == uid)
                    return &_passes[i];
            }
            return 0L;
        }

        /** Look up a rendering pass by the corresponding layer ID */
        RenderingPass* getPass(UID uid)
        {
            for (unsigned i = 0; i < _passes.size(); ++i) {
                if (_passes[i].sourceUID() == uid)
                    return &_passes[i];
            }
            return 0L;
        }

        void setSharedSampler(
            unsigned binding,
            shared_ptr<Image> image,
            //vsg::ref_ptr<vsg::ImageInfo> texture,
            int sourceRevision)
        {
            Sampler& sampler = _sharedSamplers[binding];
            sampler._image = image;
            //sampler._texture = texture;
            sampler._matrix = dmat4(1.0); // .makeIdentity();
            sampler._revision = sourceRevision;
        }

        void clearSharedSampler(unsigned binding)
        {
            Sampler& sampler = _sharedSamplers[binding];
            sampler._image = nullptr;
            sampler._texture = nullptr;
            sampler._matrix = dmat4(1.0); //.makeIdentity();
            sampler._revision = 0;
        }

        /** Deallocate GPU objects associated with this model */
        //void releaseGLObjects(osg::State* state) const
        //{
        //    for (unsigned s = 0; s<_sharedSamplers.size(); ++s)
        //        if (_sharedSamplers[s].ownsTexture())
        //            _sharedSamplers[s]._texture->releaseGLObjects(state);

        //    for (unsigned p = 0; p<_passes.size(); ++p)
        //        _passes[p].releaseGLObjects(state);
        //}

        ///** Resize GL buffers associated with this model */
        //void resizeGLObjectBuffers(unsigned size)
        //{
        //    for (unsigned s = 0; s<_sharedSamplers.size(); ++s)
        //        if (_sharedSamplers[s].ownsTexture())
        //            _sharedSamplers[s]._texture->resizeGLObjectBuffers(size);

        //    for (unsigned p = 0; p<_passes.size(); ++p)
        //        _passes[p].resizeGLObjectBuffers(size);
        //}
    };

}
