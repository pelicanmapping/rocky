/**
 * rocky c++
 * Copyright 2023-2024 Pelican Mapping, Chris Djali
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/TileLayer.h>
#include <rocky/GeoImage.h>

namespace ROCKY_NAMESPACE
{
    /**
     * A map terrain layer containing bitmap image data.
     */
    class ROCKY_EXPORT ImageLayer : public Inherit<TileLayer, ImageLayer>
    {
    public:
        //! Sharpness filter strength to apply to the image
        option<float> sharpness = 0.0f;

        //! Creates an image for the given tile key.
        //! @param key TileKey for which to create an image
        //! @param io IO options
        //! @return A GeoImage object containing the image data.
        Result<GeoImage> createImage(const TileKey& key, const IOOptions& io) const;

        //! serialize
        std::string to_json() const override;

    public: // Layer

        Result<> openImplementation(const IOOptions&) override;

        void closeImplementation() override;

    protected:

        //! Default construtor
        ImageLayer();

        //! Deserialization constructor
        //! @param JSON JSON string to deserialize
        //! @param io IO options
        ImageLayer(std::string_view JSON, const IOOptions& io);

        //! Subclass overrides this to generate image data for the key.
        //! The key will always be in the same profile as the layer.
        //! @param key TileKey for which to create an image
        //! @param io IO options
        //! @return A GeoImage object containing the image data.
        virtual Result<GeoImage> createImageImplementation(const TileKey& key, const IOOptions& io) const
        {
            return Failure_ResourceUnavailable;
        }

    protected:

        Result<GeoImage> createImageImplementation_internal(const TileKey& key, const IOOptions& io) const;

    private:

        // internal construction method
        void construct(std::string_view, const IOOptions& io);

        // Creates an image that's in the same profile as the provided key.
        Result<GeoImage> createImageInKeyProfile(const TileKey& key, const IOOptions& io) const;

        // Fetches multiple images from the TileSource; mosaics/reprojects/crops as necessary, and
        // returns a single tile. This is called by createImageFromTileSource() if the key profile
        // doesn't match the layer profile.
        std::shared_ptr<Image> assembleImage(const TileKey& key, const IOOptions& io) const;

        // Checks a cache for an image, and if not found, calls the create function to generate it.
        Result<GeoImage> getOrCreate(const TileKey& key, const IOOptions& io,
            std::function<Result<GeoImage>()>&& create) const;
    };

} // namespace ROCKY_NAMESPACE

