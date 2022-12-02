/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/TileLayer.h>
#include <rocky/GeoHeightfield.h>

namespace rocky
{
    /**
     * A map terrain layer containing elevation grid heightfields.
     */
    class ROCKY_EXPORT ElevationLayer : 
        public Inherit<TileLayer, ElevationLayer>
    {
    public:

        //! Vertical data identifier (options)
        void setVerticalDatum(const std::string& value);
        const optional<std::string>& verticalDatum() const;

        //! Whether this layer contains offsets instead of absolute elevation heights
        void setOffset(bool value);
        const optional<bool>& offset() const;

        ////! Policy for handling "no-data" elevation values
        //void setNoDataPolicy(const ElevationNoDataPolicy& value);
        //const ElevationNoDataPolicy& getNoDataPolicy() const;

        //! Value to treat as "absence of data"
        void setNoDataValue(float value);
        const optional<float>& noDataValue() const;

        //! Treat values lesser than this as "no data"
        void setMinValidValue(float value);
        const optional<float>& minValidValue() const;

        //! Treat values greater than this as "no data"
        void setMaxValidValue(float value);
        const optional<float>& maxValidValue() const;

        //! Override from VisibleLayer
        virtual void setVisible(bool value);

        //! Serialize this layer
        virtual Config getConfig() const override;

    public: // methods

        /**
         * Creates a GeoHeightField for this layer that corresponds to the extents and LOD
         * in the specified TileKey. The returned HeightField will always match the geospatial
         * extents of that TileKey.
         *
         * @param key TileKey for which to create a heightfield.
         */
        Result<GeoHeightfield> createHeightfield(
            const TileKey& key);

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
            IOControl* progress);

        /**
         * Writes a height field for the specified key, if writing is
         * supported and the layer was opened with openForWriting.
         */
        Status writeHeightfield(
            const TileKey& key,
            const Heightfield* hf,
            IOControl* progress) const;

    protected: // TileLayer

        //! Override aspects of the layer Profile as needed
        virtual void applyProfileOverrides(
            shared_ptr<Profile>& in_out_profile) const override;

    protected: // ElevationLayer

        //! Construct (from subclass)
        ElevationLayer();

        //! Deserialize (from subclass)
        ElevationLayer(const Config&);

        //! Entry point for createHeightfield
        Result<GeoHeightfield> createHeightfieldInKeyProfile(
            const TileKey& key,
            IOControl* progress);

        //! Subclass overrides this to generate image data for the key.
        //! The key will always be in the same profile as the layer.
        virtual Result<GeoHeightfield> createHeightfieldImplementation(
            const TileKey&,
            IOControl* progress) const
        {
            return GeoHeightfield::INVALID;
        }

        //! Subalss can override this to enable writing heightfields.
        virtual Status writeHeightfieldImplementation(
            const TileKey& key,
            const Heightfield* hf,
            IOControl* progress) const;

        virtual ~ElevationLayer() { }


        optional<std::string> _verticalDatum;
        optional<bool> _offset;
        //optional<ElevationNoDataPolicy> _noDataPolicy;
        optional<float> _noDataValue;
        optional<float> _minValidValue;
        optional<float> _maxValidValue;

    private:
        void construct(const Config&);

        void assembleHeightfield(
            const TileKey& key,
            shared_ptr<Heightfield> out_hf,
            IOControl* progress) const;

        void normalizeNoDataValues(
            Heightfield* hf) const;

        util::Gate<TileKey> _sentry;
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
            shared_ptr<Profile> hae_profile,
            Heightfield::Interpolation interpolation,
            IOControl* progress ) const;
    };

} // namespace

