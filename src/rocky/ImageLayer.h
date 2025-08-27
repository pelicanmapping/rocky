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
        option<float> sharpness = 0.0f;

        //! Color that represents a "no data" image. Matches the first and
        //! last pixels.
        option<Color> noDataColor;

    public:
        //! Creates an image for the given tile key.
        //! @param key TileKey for which to create an image
        //! @param io IO options
        //! @return A GeoImage object containing the image data.
        Result<GeoImage> createTile(const TileKey& key, const IOOptions& io) const;

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
        virtual Result<GeoImage> createTileImplementation(const TileKey& key, const IOOptions& io) const
        {
            return Failure_ResourceUnavailable;
        }

    private:

        // internal construction method
        void construct(std::string_view, const IOOptions& io);

        // Creates an image that's in the same profile as the provided key.
        Result<GeoImage> createTileInKeyProfile(const TileKey& key, const IOOptions& io) const;

        // Fetches multiple images from the TileSource; mosaics/reprojects/crops as necessary, and
        // returns a single tile. This is called by createImageFromTileSource() if the key profile
        // doesn't match the layer profile.
        std::shared_ptr<Image> assembleTile(const TileKey& key, const IOOptions& io) const;

        Result<GeoImage> invokeCreateTileImplementation(const TileKey& key, const IOOptions& io) const;
    };

} // namespace ROCKY_NAMESPACE

