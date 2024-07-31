/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
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

        //! Number of samples in each dimension.
        void setTileSize(unsigned value);
        const optional<unsigned>& tileSize() const;

        //! DTOR
        virtual ~TileLayer();

        //! seriailize
        JSON to_json() const override;

    protected:

        TileLayer();

        TileLayer(const JSON&);

    public: // Layer
            
        //! Tiling profile of this layer.
        //! Layer implementaions will call this to set the profile based
        //! on information gathered from source metadata. If your Layer
        //! needs the user to expressly set a profile, override this to
        //! make it public.
        void setProfile(const Profile&);

        //! Tiling profile for this layer
        const Profile& profile() const;

        //! Whether the layer represents dynamic data, i.e. it generates data
        //! that requires period updates
        virtual bool dynamic() const;

        //! Disable this layer, setting an error status.
        void disable(const std::string& msg);


    public: // Data availability methods

        //! True is the tile key intersects the data extents of this layer.
        bool intersects(const TileKey& key) const;

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
        virtual TileKey bestAvailableTileKey(
            const TileKey& key,
            bool considerUpsampling =false) const;

        //! Whether the layer possibly has real data for the provided TileKey.
        //! Best guess given available information.
        virtual bool mayHaveData(const TileKey& key) const;

        //! Whether the given key falls within the range limits set in the options;
        //! i.e. min/maxLevel or min/maxResolution. (This does not mean that the key
        //! will result in data.)
        virtual bool isKeyInLegalRange(const TileKey& key) const;

        //! Same as isKeyInLegalRange, but ignores the "maxDataLevel" setting
        //! since that does NOT affect visibility of a tile.
        virtual bool isKeyInVisualRange(const TileKey& key) const;

        //! Data Extents reported for this layer are copied into output
        virtual DataExtentList dataExtents() const;

        //! Number of data extents on the layer
        std::size_t dataExtentsSize() const;

        //! Extent that is the union of all the extents in getDataExtents().
        const DataExtent& dataExtentsUnion() const;

        //! Assign a data extents collection to the layer
        virtual void setDataExtents(const DataExtentList& dataExtents);

        //! Adds a DataExent to this layer.
        void addDataExtent(const DataExtent& dataExtent);

    public: // Layer

        //! Extent of this layer
        const GeoExtent& extent() const override;

    protected: // Layer

        Status openImplementation(const IOOptions&) override;

    protected:

        //! Call this if you call dataExtents() and modify it.
        void dirtyDataExtents();

        //! Sets the layer profile as a default value (won't be serialized).
        void setProfileDefault(const Profile&);

    protected:

        // cache key for metadata
        std::string getMetadataKey(const Profile&) const;

        optional<unsigned> _minLevel = 0;
        optional<unsigned> _maxLevel = 23;
        optional<double> _minResolution;
        optional<double> _maxResolution;
        optional<unsigned> _maxDataLevel = 99;
        optional<unsigned> _tileSize = 256;

        bool _writingRequested;

        // profile to use
        mutable optional<Profile> _profile;

    private:
        // Post-ctor
        void construct(const JSON&);

        void buildDataExtentsIfNeeded() const;

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

        // methods accesible by Map:
        friend class Map;
    };

    /**
    * A "cache" of weak pointers to values, keyed by tile key. This will keep
    * references to data that are still in use elsewhere in the system in
    * order to prevent re-fetching or re-mosacing the same data over and over.
    */
    template<class Key, class Value>
    class DependencyCache
    {
    public:
        //! Fetch a value from teh cache or nullptr if it's not there
        std::shared_ptr<Value> operator[](const Key& key)
        {
            const std::lock_guard lock{ _mutex };
            ++_gets;
            auto itr = _map.find(key);
            if (itr != _map.end())
            {
                auto result = itr->second.lock();
                if (result) ++_hits;
                return result;
            }
            return nullptr;
        }

        //! Enter a value into the cache, returning an existing value if there is one
        std::shared_ptr<Value> put(const Key& key, const std::shared_ptr<Value>& value)
        {
            const std::lock_guard lock{ _mutex };
            auto itr = _map.find(key);
            if (itr != _map.end())
            {
                std::shared_ptr<Value> preexisting = itr->second.lock();
                if (preexisting)
                    return preexisting;
            }
            _map[key] = value;
            return value;
        }

        //! Clean the cache by purging entries whose weak pointers have expired
        void clean()
        {
            const std::lock_guard lock{ _mutex };
            for (auto itr = _map.begin(), end = _map.end(); itr != end;)
            {
                if (!itr->second.lock())
                    itr = _map.erase(itr);
                else
                    ++itr;
            }
        }

        float hitRatio() const
        {
            return _gets > 0.0f ? _hits / _gets : 0.0f;
        }

    private:
        std::unordered_map<Key, std::weak_ptr<Value>> _map;
        float _gets = 0.0f;
        float _hits = 0.0f;
        std::mutex _mutex;
    };


} // namespace TileLayer
