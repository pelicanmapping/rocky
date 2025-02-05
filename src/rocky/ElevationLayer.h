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
            MapboxRGB
        };

        //! Whether this layer contains offsets instead of absolute elevation heights
        optional<bool> offset = false;

        //! Value to treat as "absence of data" if the datasource does not specify it
        optional<float> noDataValue = NO_DATA_VALUE;

        //! Treat values lesser than this as "no data"
        optional<float> minValidValue = -FLT_MAX;

        //! Treat values greater than this as "no data"
        optional<float> maxValidValue = FLT_MAX;

        //! Encoding of the elevation data
        optional<Encoding> encoding = Encoding::SingleChannel;

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

    public: // Layer

        Status openImplementation(const IOOptions&) override;

        void closeImplementation() override;

    protected:

        //! Construct (from subclass)
        ElevationLayer();

        //! Deserialize (from subclass)
        explicit ElevationLayer(const std::string& JSON, const IOOptions& io);

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
            return Result(GeoHeightfield::INVALID);
        }

        //! Decodes a mapbox RGB encoded heightfield image into a heightfield.
        std::shared_ptr<Heightfield> decodeMapboxRGB(std::shared_ptr<Image> image) const;

    private:
        void construct(const std::string& JSON, const IOOptions& io);

        std::shared_ptr<Heightfield> assembleHeightfield(const TileKey& key, const IOOptions& io) const;

        void normalizeNoDataValues(Heightfield* hf) const;

        util::Gate<TileKey> _sentry;

        mutable util::LRUCache<TileKey, Result<GeoHeightfield>> _L2cache;

        Result<GeoHeightfield> createHeightfieldImplementation_internal(
            const TileKey& key,
            const IOOptions& io) const;

        std::shared_ptr<TileMosaicWeakCache<Heightfield>> _dependencyCache;
    };


    /**
     * Vector of elevation layers, with added methods.
     */
    class ROCKY_EXPORT ElevationLayerVector : public std::vector<std::shared_ptr<ElevationLayer>>
    {
    public:
        /**
         * Populates an existing height field (hf must already exist) with height
         * values from the elevation layers.
         *
         * @param hf Heightfield object to populate; must be pre-allocated
         * @param resolutions If non-null, populate with resolution of each sample
         * @param key Tilekey for which to populate
         * @param haeProfile Optional geodetic (no vdatum) tiling profile to use
         * @param interpolation Elevation interpolation technique
         * @param progress Optional progress callback for cancelation
         * @return True if "hf" was populated, false if no real data was available for key
         */
        bool populateHeightfield(
            std::shared_ptr<Heightfield> in_out_hf,
            std::vector<float>* resolutions,
            const TileKey& key,
            const Profile& hae_profile,
            Interpolation interpolation,
            const IOOptions& io ) const;
    };

} // namespace

