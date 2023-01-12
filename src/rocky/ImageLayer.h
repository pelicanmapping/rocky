/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/Config.h>
#include <rocky/TileLayer.h>
#include <rocky/GeoImage.h>
#include <rocky/Color.h>

namespace ROCKY_NAMESPACE
{
    class Color;
    class Profile;

    /**
     * A map terrain layer containing bitmap image data.
     */
    class ROCKY_EXPORT ImageLayer : public Inherit<TileLayer, ImageLayer>
    {
    //public: // Serialization
    //    class ROCKY_EXPORT Options : public TileLayer::Options {
    //    public:
    //        ROCKY_LayerOptions(Options, TileLayer::Options);
    //        ROCKY_OPTION(std::string, noDataImageLocation);
    //        ROCKY_OPTION(Color, transparentColor);
    //        //ROCKY_OPTION(ColorFilterChain, colorFilters);
    //        ROCKY_OPTION(bool, shared);
    //        ROCKY_OPTION(bool, coverage);
    //        //ROCKY_OPTION(osg::Texture::FilterMode, minFilter);
    //        //ROCKY_OPTION(osg::Texture::FilterMode, magFilter);
    //        ROCKY_OPTION(std::string, textureCompression);
    //        ROCKY_OPTION(double, edgeBufferRatio);
    //        ROCKY_OPTION(unsigned, reprojectedTileSize);
    //        ROCKY_OPTION(Distance, altitude);
    //        ROCKY_OPTION(bool, acceptDraping);
    //        ROCKY_OPTION(bool, async);
    //        ROCKY_OPTION(std::string, shareTexUniformName);
    //        ROCKY_OPTION(std::string, shareTexMatUniformName);
    //        virtual Config getConfig() const;
    //    private:
    //        void fromConfig( const Config& conf );
    //    };

    //public:
    //    ROCKY_Layer_Abstract(ImageLayer, Options);

#if 0
        //! Layer callbacks
        class ROCKY_EXPORT Callback
        {
        public:
            //! Called when a new data is created. This callback fires
            //! before the data is cached, and does NOT fire if the data
            //! was read from a cache.
            //! NOTE: This may be invoked from a worker thread. Use caution.
            virtual void onCreate(const TileKey&, GeoImage&) { }
        };
#endif

    public:
        /** dtor */
        virtual ~ImageLayer() { }

        //! Convenience function to create an ImageLayer from a ConfigOptions.
        //static shared_ptr<ImageLayer> create(
        //    const ConfigOptions& conf);

#if 0
        //! Sets the altitude
        void setAltitude(const Distance& value);
        const Distance& getAltitude() const;
#endif

        //! Sets whether this layer should allow draped overlays
        //! to render on it. This is most applicable to layers with a
        //! non-zero altitude (setAltitude). Default is true.
        void setAcceptDraping(bool value);
        bool getAcceptDraping() const;

        //! Marks this layer for asynchronous loading.
        //! Usually all layers participating in a tile must load before the
        //! tile is displayed. This flag defers the current layer so it can 
        //! load asynchronously and display when it is available. This can
        //! help keep a slow-loading layer from blocking the rest of the tile
        //! from displaying. The trade-off is possible visual artifacts
        //! (flashing, no mipmaps/compression) when the new data appears.
        void setAsyncLoading(bool value);
        bool getAsyncLoading() const;

        //! Whether this layer is marked for render sharing.
        //! Only set this before opening the layer or adding it to a map.
        void setShared(bool value);
        bool getShared() const;
        bool isShared() const { return getShared(); }

        //! Whether this layer represents coverage data that should not be subject
        //! to color-space filtering, interpolation, or compression.
        //! Only set this before opening the layer or adding it to a map.
        void setCoverage(bool value);
        bool getCoverage() const;
        bool isCoverage() const { return getCoverage(); }

        //! When isShared() == true, this will return the name of the uniform holding the
        //! image's texture.
        void setSharedTextureUniformName(const std::string& value);
        const std::string& getSharedTextureUniformName() const;

        //! When isShared() == true, this will return the name of the uniform holding the
        //! image's texture matrix.
        void setSharedTextureMatrixUniformName(const std::string& value);
        const std::string& getSharedTextureMatrixUniformName() const;

        //! When isShared() == true, the engine will call this function to bind the
        //! shared layer to a texture image unit.
        optional<int>& sharedImageUnit() { return _shareImageUnit; }
        const optional<int>& sharedImageUnit() const { return _shareImageUnit; }

        shared_ptr<Image> getEmptyImage() const {
            return _emptyImage;
        }

        virtual Config getConfig() const override;

    public: // methods

        //! Creates an image for the given tile key.
        //! @param key TileKey for which to create an image
        //! @param progress Optional progress/cancelation callback
        Result<GeoImage> createImage(
            const TileKey& key) const;

        //! Creates an image for the given tile key.
        //! @param key TileKey for which to create an image
        //! @param progress Optional progress/cancelation callback
        Result<GeoImage> createImage(
            const TileKey& key,
            const IOOptions& io) const;

        //! Stores an image in this layer (if writing is enabled).
        //! Returns a status value indicating whether the store succeeded.
        Status writeImage(
            const TileKey& key,
            const Image* image,
            const IOOptions& io);

        //! Returns the compression method prefered by this layer
        //! that you can pass to ImageUtils::compressImage.
        const std::string getCompressionMethod() const;

    public:

        //! Subclass overrides this to generate image data for the key.
        //! The key will always be in the same profile as the layer.
        virtual Result<GeoImage> createImageImplementation(
            const TileKey& key,
            const IOOptions& io) const
        {
            return Result(GeoImage::INVALID);
        }

#if 0
        //! Install a user callback
        void addCallback(Callback* callback);

        //! Remove a user callback
        void removeCallback(Callback* callback);
#endif

#if 0
        //! Add a post-processing layer
        void addPostLayer(ImageLayer* layer);
#endif

    protected:

        ImageLayer();

        ImageLayer(const Config&);

        //! Subclass can override this to write data for a tile key.
        virtual Status writeImageImplementation(
            const TileKey& key,
            const Image* image,
            const IOOptions& io) const;

        //! Modify the bbox if an altitude is set (for culling)
        virtual void modifyTileBoundingBox(
            const TileKey& key,
            Box& box) const;

        //! Post processing image creation entry points
        Result<GeoImage> createImage(
            const GeoImage& canvas,
            const TileKey& key,
            const IOOptions& io);

        //! Override to write an image over top of an existing image
        virtual Result<GeoImage> createImageImplementation(
            const GeoImage& canvas,
            const TileKey& key,
            const IOOptions& io) const { return Result(canvas); }

        //! Override to do something to an image before returning
        //! it from createImage (including a GeoImage read from the cache)
        virtual void postCreateImageImplementation(
            GeoImage& createdImage,
            const TileKey& key,
            const IOOptions& io) const { }

    protected: // Layer

        //! Open the layer for reading.
        virtual Status openImplementation(
            const IOOptions& io) override;


        //! Configure the layer to create textures via createTexture instead of
        //! using a createImage driver
        void setUseCreateTexture();

        shared_ptr<Image> _emptyImage;

        optional<std::string> _noDataImageLocation;
        optional<Color> _transparentColor;
        //ROCKY_OPTION(ColorFilterChain, colorFilters);
        optional<bool> _shared;
        optional<bool> _coverage;
        //ROCKY_OPTION(osg::Texture::FilterMode, minFilter);
        //ROCKY_OPTION(osg::Texture::FilterMode, magFilter);
        optional<std::string> _textureCompression;
        //optional<double> _edgeBufferRatio;
        //optional<unsigned> _reprojectedTileSize;
        optional<Distance> _altitude;
        optional<bool> _acceptDraping;
        optional<bool> _async;
        //optional<std::string> _shareTexUniformName;
        //optional<std::string> _shareTexMatUniformName;

    private:

        void construct(const Config&);

        // Creates an image that's in the same profile as the provided key.
        Result<GeoImage> createImageInKeyProfile(
            const TileKey& key,
            const IOOptions& io) const;

        // Fetches multiple images from the TileSource; mosaics/reprojects/crops as necessary, and
        // returns a single tile. This is called by createImageFromTileSource() if the key profile
        // doesn't match the layer profile.
        Result<GeoImage> assembleImage(
            const TileKey& key,
            const IOOptions& io) const;

#if 0
        // Creates an image that enhances the previous LOD's image
        // using a fractal algorithm.
        GeoImage createFractalUpsampledImage(
            const TileKey& key,
            IOControl* p);
#endif

        optional<int> _shareImageUnit;
      
        //bool _useCreateTexture;

#if 0
        void invoke_onCreate(const TileKey&, GeoImage&);
        using Callbacks = std::vector<shared_ptr<Callback>>;
        util::Mutexed<Callbacks> _callbacks;
#endif

        //Mutexed<std::vector<osg::ref_ptr<ImageLayer>>> _postLayers;

        util::Gate<TileKey> _sentry;
    };

    typedef std::vector<shared_ptr<ImageLayer>> ImageLayerVector;

} // namespace ROCKY_NAMESPACE

