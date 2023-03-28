/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
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
    public:
        virtual ~ImageLayer() { }

        //! Sets whether this layer should allow draped overlays
        //! to render on it. This is most applicable to layers with a
        //! non-zero altitude (setAltitude). Default is true.
        void setAcceptDraping(bool value);
        bool getAcceptDraping() const;

        //! Whether the layer contains coverage data
        void setCoverage(bool value) { _coverage = value; }
        const optional<bool>& coverage() const { return _coverage; }

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

        //! Stores an image in this layer (if writing is enabled).
        //! Returns a status value indicating whether the store succeeded.
        Status writeImage(
            const TileKey& key,
            shared_ptr<Image> image,
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

    protected:

        ImageLayer();

        ImageLayer(const JSON&);

        //! Subclass can override this to write data for a tile key.
        virtual Status writeImageImplementation(
            const TileKey& key,
            shared_ptr<Image> image,
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

        shared_ptr<Image> _emptyImage;
        optional<std::string> _noDataImageLocation = { };
        optional<Color> _transparentColor = Color(0, 0, 0, 0);
        optional<std::string> _textureCompression;
        optional<bool> _acceptDraping = false;

    private:

        void construct(const JSON&);

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

        optional<bool> _coverage = false;
    };

} // namespace ROCKY_NAMESPACE

