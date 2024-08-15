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
        //! Creates an image for the given tile key.
        //! @param key TileKey for which to create an image
        //! @param io IO options
        Result<GeoImage> createImage(
            const TileKey& key,
            const IOOptions& io) const;

        //! serialize
        JSON to_json() const override;

    protected:

        //! Default construtor
        ImageLayer();

        //! Deserialization constructor
        ImageLayer(const std::string& JSON, const IOOptions& io);

        //! Subclass overrides this to generate image data for the key.
        //! The key will always be in the same profile as the layer.
        virtual Result<GeoImage> createImageImplementation(
            const TileKey& key,
            const IOOptions& io) const
        {
            return Result(GeoImage::INVALID);
        }

    protected: // Layer

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

        std::shared_ptr<TileMosaicWeakCache<Image>> _dependencyCache;
    };

} // namespace ROCKY_NAMESPACE

