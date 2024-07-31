/**
 * rocky c++
 * Copyright 2023-2024 Pelican Mapping, Chris Djali
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/TileLayer.h>
#include <rocky/GeoImage.h>
#include <rocky/Color.h>

namespace ROCKY_NAMESPACE
{
    /**
     * A map terrain layer containing bitmap image data.
     */
    class ROCKY_EXPORT ImageLayer : public Inherit<TileLayer, ImageLayer>
    {
    public:
        virtual ~ImageLayer() { }

        shared_ptr<Image> getEmptyImage() const {
            return _emptyImage;
        }
        
        //! serialize
        JSON to_json() const override;

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

    protected:

        ImageLayer();

        ImageLayer(const std::string& JSON, const IOOptions& io);

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

        shared_ptr<Image> _emptyImage;
        optional<std::string> _noDataImageLocation = { };
        optional<Color> _transparentColor = Color(0, 0, 0, 0);
        optional<std::string> _textureCompression;
        optional<float> _sharpness;

        Result<GeoImage> createImageImplementation_internal(
            const TileKey& key,
            const IOOptions& io) const;

    private:

        void construct(const std::string& JSON, const IOOptions& io);

        // Creates an image that's in the same profile as the provided key.
        Result<GeoImage> createImageInKeyProfile(
            const TileKey& key,
            const IOOptions& io) const;

        // Fetches multiple images from the TileSource; mosaics/reprojects/crops as necessary, and
        // returns a single tile. This is called by createImageFromTileSource() if the key profile
        // doesn't match the layer profile.
        shared_ptr<Image> assembleImage(
            const TileKey& key,
            const IOOptions& io) const;

        std::shared_ptr<DependencyCache<TileKey, Image>> _dependencyCache;
    };

} // namespace ROCKY_NAMESPACE

