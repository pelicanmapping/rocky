/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/Config.h>
#include <rocky/IOTypes.h>
#include <rocky/VisibleLayer.h>
#include <rocky/Profile.h>
#include <rocky/TileKey.h>

namespace ROCKY_NAMESPACE
{
    /**
     * A layer that comprises the terrain skin (image or elevation layer)
     */
    class ROCKY_EXPORT TileLayer : public Inherit<VisibleLayer, TileLayer>
    {
    public:

        //! Minimum of detail for which this layer should generate data.
        void setMinLevel(unsigned value);
        const optional<unsigned>& minLevel() const;

        //! Minimum resolution for which this layer should generate data.
        void setMinResolution(double value);
        const optional<double>& minResolution() const;

        //! Maximum level of detail for which this layer should generate data.
        //! Data from this layer will not appear in map tiles above the maxLevel.
        void setMaxLevel(unsigned value);
        const optional<unsigned>& maxLevel() const;

        //! Maximum level resolution for which this layer should generate data.
        //! The value is in units per pixel, using the base units of the layer's source data.
        void setMaxResolution(double value);
        const optional<double>& maxResolution() const;

        //! Maximum level of detail for which this layer should generate new data.
        //! Data from this layer will be upsampled in map tiles above the maxDataLevel.
        void setMaxDataLevel(unsigned value);
        const optional<unsigned>& maxDataLevel() const;

        //! Whether to algorithmically upsample data to higher resolution
        //! as specified by maxDataLevel.
        //! Warning: don't use this without a cache (performance)
        void setUpsample(bool value);
        const optional<bool>& upsample() const;

        //! Number of samples in each dimension.
        void setTileSize(unsigned value);
        const optional<unsigned>& tileSize() const;

        //! DTOR
        virtual ~TileLayer();

        virtual Config getConfig() const override;

    protected:

        TileLayer();

        TileLayer(const Config&);

        //! Tiling profile of this layer.
        //! Layer implementaions will call this to set the profile based
        //! on information gathered from source metadata. If your Layer
        //! needs the user to expressly set a profile, override this to
        //! make it public.
        virtual void setProfile(const Profile&);

    public: // Layer

        //! Open the layer for writing (calls open)
        const Status& openForWriting();

        //! Does the layer support writing?
        virtual bool isWritingSupported() const { return false; }

        //! Did the user open this layer for writing?
        bool isWritingRequested() const { return _writingRequested; }

        //! Tiling profile for this layer
        const Profile& profile() const;

        //! Whether the layer represents dynamic data, i.e. it generates data
        //! that requires period updates
        virtual bool isDynamic() const;

        //! Whether the data for the specified tile key is in the cache.
        //virtual bool isCached(const TileKey& key) const;

        //! Disable this layer, setting an error status.
        void disable(const std::string& msg);


    public: // Data availability methods

        /**
         * Given a TileKey, returns a TileKey representing the best known available.
         * For example, if the input TileKey exceeds the layer's max LOD, the return
         * value will be an ancestor key at that max LOD.
         *
         * If a setting that effects the visible range of this layer is set (minLevel, maxLevel, minResolution or maxResolution)
         * then any key passed in that falls outside of the valid range for the layer will return TileKey::INVALID.
         *
         * @param key Tile key to check
         * @param considerUpsampling Normally this method will only return a key
         *    corresponding to possible real data. If you set this to true, it may
         *    also return a TileKey that may correspond to upsampled data.
         */
        virtual TileKey getBestAvailableTileKey(
            const TileKey& key,
            bool considerUpsampling =false) const;

        /**
         * Whether the layer possibly has real data for the provided TileKey.
         * Best guess given available information.
         */
        virtual bool mayHaveData(const TileKey& key) const;

        /**
         * Whether the given key falls within the range limits set in the options;
         * i.e. min/maxLevel or min/maxResolution. (This does not mean that the key
         * will result in data.)
         */
        virtual bool isKeyInLegalRange(const TileKey& key) const;

        /**
         * Same as isKeyInLegalRange, but ignores the "maxDataLevel" setting
         * since that does NOT affect visibility of a tile.
         */
        virtual bool isKeyInVisualRange(const TileKey& key) const;

        /**
         * Data Extents reported for this layer are copied into output.
         */
        virtual void getDataExtents(DataExtentList& dataExtents) const;

        /**
        * Gets the number of data extents on the layer.
        */
        unsigned int getDataExtentsSize() const;

        /**
         * Gets an extent that is the union of all the extents in getDataExtents().
         */
        const DataExtent& getDataExtentsUnion() const;

        //! Assign a data extents collection to the layer
        virtual void setDataExtents(const DataExtentList& dataExtents);

        //! Adds a DataExent to this layer.
        void addDataExtent(const DataExtent& dataExtent);

    public: // Layer interface

        //! Extent of this layer's data.
        virtual const GeoExtent& extent() const;

        //! Called by Map when added
        //void addedToMap(const Map*) override;

    public:

        /**
         * Metadata about the terrain layer that is stored in the cache, and read
         * when the cache is opened.
         */
        struct ROCKY_EXPORT CacheBinMetadata
        {
            CacheBinMetadata();

            CacheBinMetadata( const CacheBinMetadata& rhs );

            CacheBinMetadata( const Config& conf );

            bool ok() const { return _valid; }

            Config getConfig() const;

            bool _valid;
            optional<std::string> _cacheBinId;
            optional<std::string> _sourceName;
            optional<std::string> _sourceDriver;
            optional<int> _sourceTileSize;
            optional<Config> _sourceProfile;
            optional<Config> _cacheProfile;
            optional<TimeStamp> _cacheCreateTime;
            DataExtentList _dataExtents;
        };

        //! Access to information about the cache
        CacheBinMetadata* getCacheBinMetadata(const Profile& profile);

        //! Sets up a small data cache if necessary.
        //void setUpL2Cache(unsigned minSize =0u);

    protected: // Layer

        Status openImplementation(const IOOptions&) override;

    protected:

        //! Opportunity for a subclass to alter and/or override components
        //! of the Profile
        virtual void applyProfileOverrides(
            Profile& in_out_profile) const { }

        //! Call this if you call dataExtents() and modify it.
        void dirtyDataExtents();

    protected:

        // cache key for metadata
        std::string getMetadataKey(const Profile&) const;

        optional<unsigned> _minLevel = 0;
        optional<unsigned> _maxLevel = 23;
        optional<double> _minResolution;
        optional<double> _maxResolution;
        optional<unsigned> _maxDataLevel = 99;
        optional<unsigned> _tileSize = 256;
        optional<bool> _upsample = false;

        bool _writingRequested;

        // profile to use
        mutable Profile _profile;

    private:
        // Post-ctor
        void construct(const Config&);

        // Figure out the cache settings for this layer.
        void establishCacheSettings();

        // general purpose data protector
        mutable std::shared_mutex _dataMutex;
        DataExtentList _dataExtents;
        mutable DataExtent _dataExtentsUnion;
        mutable void* _dataExtentsIndex;

        // The cache ID used at runtime. This will either be the cacheId found in
        // the TileLayerOptions, or a dynamic cacheID generated at runtime.
        std::string _runtimeCacheId;

        // cache policy that may be automatically set by the layer and will
        // override the runtime options policy if set.
        optional<CachePolicy> _runtimeCachePolicy;

        //using CacheBinMetadataMap = std::unordered_map<std::string, osg::ref_ptr<CacheBinMetadata>>;
        //CacheBinMetadataMap _cacheBinMetadata;

        // methods accesible by Map:
        friend class Map;
    };



    // inlines methods
    

} // namespace TileLayer
