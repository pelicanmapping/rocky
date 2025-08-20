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
#include <map>

namespace ROCKY_NAMESPACE
{
    /**
     * A layer that comprises the terrain skin (image or elevation layer)
     */
    class ROCKY_EXPORT TileLayer : public Inherit<VisibleLayer, TileLayer>
    {
    public:

        //! Minimum of detail for which this layer should generate data.
        option<unsigned> minLevel = 0u;

        //! Maximum level of detail for which this layer should generate data.
        //! Data from this layer will not appear in map tiles above the maxLevel.
        option<unsigned> maxLevel = 99u;

        //! Maximum level of detail for which this layer should generate new data.
        //! Data from this layer will be upsampled in map tiles above the maxDataLevel.
        option<unsigned> maxDataLevel = 99u;

        //! Number of samples in each dimension.
        option<unsigned> tileSize = 256u;

        //! The extent to which the layer should be cropped.
        option<GeoExtent> crop;

        //! Tiling profile and SRS or the layer.
        Profile profile;

        //! seriailize
        std::string to_json() const override;

    protected:

        TileLayer();

        TileLayer(std::string_view);

    public:

        //! Tiling profile of this layer.
        //! Layer implementaions will call this to set the profile based
        //! on information gathered from source metadata.
        void setPermanentProfile(const Profile&);

        //! True is the tile key intersects the data extents of this layer.
        bool intersects(const TileKey& key) const;

        //! Given a TileKey, returns a TileKey representing the best potentially available data
        //! at that location. This will return either:
        //!   (a) the input key, meaning data is available for that exact key
        //!   (b) an ancestor key, meaning a lower resolution tile is available
        //!   (c) TileKey::INVALID, meaning data is NOT available for the input key's location.
        //! Properties affecting the result include dataExtents, minLevel, maxLevel, minResolution, and maxResolution.
        //! Note: there is never a guarantee that data will be available anywhere, especially for network
        //! resources. This method is a best guess based on the information available to the layer.
        //! @param key Tile key to check
        //! @return Best available tile key
        virtual TileKey bestAvailableTileKey(const TileKey& key) const;

        //! Whether the layer possibly has real data for the provided TileKey.
        //! Best guess given available information.
        virtual bool mayHaveData(const TileKey& key) const;

        //! Whether the given key falls within the range limits set in the options;
        //! i.e. min/maxLevel or min/maxResolution. (This does not mean that the key
        //! will result in data.)
        virtual bool isKeyInConfiguredRange(const TileKey& key) const;

        //! Data extents reported for this layer
        virtual const DataExtentList& dataExtents() const;

        //! Extent that is the union of all the extents in dataExtents().
        const DataExtent& dataExtentsUnion() const;

    public: // Layer

        //! Extent of this layer
        const GeoExtent& extent() const override;

    protected: // Layer

        Result<> openImplementation(const IOOptions&) override;
        
        void closeImplementation() override;

    protected:

        //! Assign a data extents collection to the layer.
        //! A subclass should only call this during openImplementation().
        void setDataExtents(const DataExtentList& dataExtents);

    protected:

        option<Profile> _originalProfile; // profile specified in the options       
        option<Profile> _runtimeProfile; // profile set at runtime by an implementation

    private:
        // Post-ctor
        void construct(std::string_view);

        // available data extents.
        DataExtentList _dataExtents;
        DataExtent _dataExtentsUnion;
        struct DataExtentsIndex;
        std::shared_ptr<DataExtentsIndex> _dataExtentsIndex;

        // methods accesible by Map:
        friend class Map;
    };
}
