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
        //! Sharpness filter strength to apply to the image
        optional<float> sharpness = 0.0f;

        //! Creates an image for the given tile key.
        //! @param key TileKey for which to create an image
        //! @param io IO options
        //! @return A GeoImage object containing the image data.
        Result<GeoImage> createImage(const TileKey& key, const IOOptions& io) const;

        //! serialize
        std::string to_json() const override;

    protected:

        //! Default construtor
        ImageLayer();

        //! Deserialization constructor
        //! @param JSON JSON string to deserialize
        //! @param io IO options
        ImageLayer(const std::string& JSON, const IOOptions& io);

        //! Subclass overrides this to generate image data for the key.
        //! The key will always be in the same profile as the layer.
        //! @param key TileKey for which to create an image
        //! @param io IO options
        //! @return A GeoImage object containing the image data.
        virtual Result<GeoImage> createImageImplementation(const TileKey& key, const IOOptions& io) const
        {
            return Result(GeoImage::INVALID);
        }

    protected: // Layer

        Result<GeoImage> createImageImplementation_internal(
            const TileKey& key,
            const IOOptions& io) const;

    private:

        // internal construction method
        void construct(const std::string& JSON, const IOOptions& io);

        // Creates an image that's in the same profile as the provided key.
        Result<GeoImage> createImageInKeyProfile(
            const TileKey& key,
            const IOOptions& io) const;

        // Fetches multiple images from the TileSource; mosaics/reprojects/crops as necessary, and
        // returns a single tile. This is called by createImageFromTileSource() if the key profile
        // doesn't match the layer profile.
        std::shared_ptr<Image> assembleImage(
            const TileKey& key,
            const IOOptions& io) const;

        // a weak cache that helps us avoid re-fetching dependent images in a mosaic
        std::shared_ptr<TileMosaicWeakCache<Image>> _dependencyCache;
    };

} // namespace ROCKY_NAMESPACE

