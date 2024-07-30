/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/TileLayer.h>
#include <rocky/GeoHeightfield.h>
#include <rocky/Threading.h>

namespace ROCKY_NAMESPACE
{
    /**
     * A map terrain layer producing elevation grid heightfields.
     */
    class ROCKY_EXPORT ElevationLayer :  public Inherit<TileLayer, ElevationLayer>
    {
    public:
        enum class Encoding {
            SingleChannel,
            MapboxRGB
        };

        //! Whether this layer contains offsets instead of absolute elevation heights
        void setOffset(bool value);
        const optional<bool>& offset() const;

        //! Value to treat as "absence of data"
        void setNoDataValue(float value);
        const optional<float>& noDataValue() const;

        //! Treat values lesser than this as "no data"
        void setMinValidValue(float value);
        const optional<float>& minValidValue() const;

        //! Treat values greater than this as "no data"
        void setMaxValidValue(float value);
        const optional<float>& maxValidValue() const;

        //! Encoding of the elevation data
        void setEncoding(Encoding evalue);
        const optional<Encoding>& encoding() const;

        //! Override from VisibleLayer
        void setVisible(bool value) override;

        //! Serialize this layer
        JSON to_json() const override;

    public: // methods

        /**
         * Creates a GeoHeightField for this layer that corresponds to the extents and LOD
         * in the specified TileKey. The returned HeightField will always match the geospatial
         * extents of that TileKey.
         *
         * @param key TileKey for which to create a heightfield.
         */
        Result<GeoHeightfield> createHeightfield(
            const TileKey& key) const;

        /**
         * Creates a GeoHeightField for this layer that corresponds to the extents and LOD
         * in the specified TileKey. The returned HeightField will always match the geospatial
         * extents of that TileKey.
         *
         * @param key TileKey for which to create a heightfield.
         * @param progress Callback for tracking progress and cancelation
         */
        Result<GeoHeightfield> createHeightfield(
            const TileKey& key,
            const IOOptions& io) const;

        /**
         * Writes a height field for the specified key, if writing is
         * supported and the layer was opened with openForWriting.
         */
        Status writeHeightfield(
            const TileKey& key,
            shared_ptr<Heightfield> hf,
            const IOOptions& io) const;

    protected: // ElevationLayer

        //! Construct (from subclass)
        ElevationLayer();

        //! Deserialize (from subclass)
        explicit ElevationLayer(const JSON&);

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

        //! Subalss can override this to enable writing heightfields.
        virtual Status writeHeightfieldImplementation(
            const TileKey& key,
            shared_ptr<Heightfield> hf,
            const IOOptions& io) const;

        //! Decodes a mapbox RGB encoded heightfield image into a heightfield.
        shared_ptr<Heightfield> decodeMapboxRGB(shared_ptr<Image> image) const;

        virtual ~ElevationLayer() { }

        optional<Encoding> _encoding = Encoding::SingleChannel;
        optional<bool> _offset = false;
        optional<float> _noDataValue = NO_DATA_VALUE;
        optional<float> _minValidValue = -FLT_MAX;
        optional<float> _maxValidValue = FLT_MAX;

    private:
        void construct(const JSON&);

        shared_ptr<Heightfield> assembleHeightfield(
            const TileKey& key,
            const IOOptions& io) const;

        void normalizeNoDataValues(
            Heightfield* hf) const;

        util::Gate<TileKey> _sentry;

        mutable util::LRUCache<TileKey, Result<GeoHeightfield>> _L2cache;

        Result<GeoHeightfield> createHeightfieldImplementation_internal(
            const TileKey& key,
            const IOOptions& io) const;

        std::shared_ptr<DependencyCache<TileKey, Heightfield>> _dependencyCache;
    };


    /**
     * Vector of elevation layers, with added methods.
     */
    class ROCKY_EXPORT ElevationLayerVector : 
        public std::vector<shared_ptr<ElevationLayer>>
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
            shared_ptr<Heightfield> in_out_hf,
            std::vector<float>* resolutions,
            const TileKey& key,
            const Profile& hae_profile,
            Image::Interpolation interpolation,
            const IOOptions& io ) const;
    };

} // namespace

