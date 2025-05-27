/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/Layer.h>
#include <rocky/Callbacks.h>
#include <rocky/IOTypes.h>
#include <rocky/LayerCollection.h>
#include <shared_mutex>

namespace ROCKY_NAMESPACE
{
    /**
     * Map is the main data model, which holds a collection of layers.
     * Create a Map by calling Map::create().
     */
    class ROCKY_EXPORT Map : public Inherit<Object, Map>
    {
    public:
        //! Construct an empty map
        Map();

        //! Adds a layer to the map
        void add(std::shared_ptr<Layer> layer);

        //! Layers comprising this map
        inline LayerCollection& layers();

        //! Layers comprising this map
        inline const LayerCollection& layers() const;

        //! Open all layers that are marked for openAutomatically.
        //! Note, this method will be called automatically by the MapNode, but you 
        //! are free to call it manually if you want to force all layers to open 
        //! and check for errors.
        Status openAllLayers(const IOOptions& options);

        //! Gets the revision # of the map. The revision # changes every time
        //! you add, remove, or move layers. You can use this to track changes
        //! in the map model (as a alternative to installing a MapCallback).
        Revision revision() const;

        //! Deserialize from JSON data
        //! @param value JSON string to import
        Status from_json(const JSON& value, const IOOptions& io);

        //! Serialize
        std::string to_json() const;

    public:
        //! Callback fired upon added a layer
        using LayerAdded = void(std::shared_ptr<Layer>, unsigned index, Revision);
        Callback<LayerAdded> onLayerAdded;

        //! Callback fired upon removing a layer
        using LayerRemoved = void(std::shared_ptr<Layer>, Revision);
        Callback<LayerRemoved> onLayerRemoved;

        //! Callback fired upon reordering a layer
        using LayerMoved = void(std::shared_ptr<Layer>, unsigned oldIndex, unsigned newIndex, Revision);
        Callback<LayerMoved> onLayerMoved;

        //! Remove a callback added to thie object
        void removeCallback(UID uid);

    protected:
        option<std::string> _profileLayer;

    private:
        mutable std::shared_mutex _mapDataMutex;
        //option<Profile> _profile;
        Revision _dataModelRevision = 0;
        LayerCollection _imageLayers;

        friend class LayerCollection;
    };


    inline LayerCollection& Map::layers() {
        return _imageLayers;
    }

    inline const LayerCollection& Map::layers() const {
        return _imageLayers;
    }
}

