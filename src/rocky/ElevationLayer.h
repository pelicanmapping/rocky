/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/TileLayer.h>
#include <rocky/GeoHeightfield.h>

namespace ROCKY_NAMESPACE
{
    /**
     * A map terrain layer producing elevation grid heightfields.
     */
    class ROCKY_EXPORT ElevationLayer :  public Inherit<TileLayer, ElevationLayer>
    {
    public:
        enum class Encoding
        {
            SingleChannel,
            MapboxRGB,
            TerrariumRGB
        };

        //! Whether this layer contains offsets instead of absolute elevation heights
        option<bool> offset = false;

        //! Value to treat as "absence of data" if the datasource does not specify it
        option<float> noDataValue = NO_DATA_VALUE;

        //! Treat values lesser than this as "no data"
        option<float> minValidValue = -FLT_MAX;

        //! Treat values greater than this as "no data"
        option<float> maxValidValue = FLT_MAX;

        //! Encoding of the elevation data
        option<Encoding> encoding = Encoding::SingleChannel;

        //! Serialize this layer
        std::string to_json() const override;

    public:

        /**
         * Creates a GeoHeightField for this layer that corresponds to the extents and LOD
         * in the specified TileKey. The returned HeightField will always match the geospatial
         * extents of that TileKey.
         *
         * @param key TileKey for which to create a heightfield.
         * @param io IO options
         */
        Result<GeoHeightfield> createHeightfield(const TileKey& key, const IOOptions& io) const;

        /**
        * The resolution in each dimension (x, y) for the data at the given level of detail.
        */
        std::pair<Distance, Distance> resolution(unsigned level) const;

    public: // Layer

        Result<> openImplementation(const IOOptions&) override;

        void closeImplementation() override;

    protected:

        //! Construct (from subclass)
        ElevationLayer();

        //! Deserialize (from subclass)
        explicit ElevationLayer(std::string_view JSON, const IOOptions& io);

        //! Entry point for createHeightfield
        Result<GeoHeightfield> createHeightfieldInKeyProfile(
            const TileKey& key,
            const IOOptions& io) const;

        //! Subclass overrides this to generate image data for the key.
        //! The key will always be in the same profile as the layer.
        virtual Result<GeoHeightfield> createHeightfieldImplementation(
            const TileKey& key,
            const IOOptions& io) const
        {
            return Failure_ResourceUnavailable;
        }

        //! Decodes an image into a heightfield from its native representation
        //! as denoted in the "encoding" option
        std::shared_ptr<Heightfield> decodeRGB(std::shared_ptr<Image> image) const;

    private:
        void construct(std::string_view, const IOOptions& io);

        std::shared_ptr<Heightfield> assembleHeightfield(const TileKey& key, const IOOptions& io) const;

        void normalizeNoDataValues(Heightfield* hf) const;

        util::Gate<TileKey> _sentry;

        Result<GeoHeightfield> createHeightfieldImplementation_internal(
            const TileKey& key,
            const IOOptions& io) const;

        // Checks a cache for an image, and if not found, calls the create function to generate it.
        Result<GeoHeightfield> readCacheOrCreate(const TileKey& key, const IOOptions& io,
            std::function<Result<GeoHeightfield>()>&& create) const;
    };

} // namespace

