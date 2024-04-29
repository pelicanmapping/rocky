/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/Instance.h>
#include <rocky/Profile.h>
#include <rocky/Layer.h>
#include <rocky/Threading.h>
#include <rocky/Callbacks.h>
#include <rocky/IOTypes.h>
#include <rocky/LayerCollection.h>
#include <functional>
#include <set>
#include <shared_mutex>
#include <typeindex>

namespace ROCKY_NAMESPACE
{
    /**
     * Map is the main data model, which holds a collection of layers.
     * Create a Map by calling Map::create().
     */
    class ROCKY_EXPORT Map : public Inherit<Object, Map>
    {
    public:
        //! This Map's unique ID
        UID uid() const { return _uid; }

        //! The map's master tiling profile, which defines its SRS and tiling structure
        void setProfile(const Profile&);
        const Profile& profile() const;

        //! Spatial reference system of the map's profile (convenience)
        const SRS& srs() const;

        //! Layers comprising this map
        inline LayerCollection& layers();

        //! Layers comprising this map
        inline const LayerCollection& layers() const;

        //! Gets the revision # of the map. The revision # changes every time
        //! you add, remove, or move layers. You can use this to track changes
        //! in the map model (as a alternative to installing a MapCallback).
        Revision revision() const;

        //! List of attribution strings to be displayed by the application
        std::set<std::string> attributions() const;

        //! Global application instance
        Instance& instance() { return _instance; }
        const Instance& instance() const { return _instance; }

        //! Import map properties and layers from JSON data
        //! @param value JSON string to import
        void from_json(const JSON& value);

    public:

        //! Construct
        explicit Map(const Instance& instance);

        //! Construct with custom options
        explicit Map(const Instance& instance, const IOOptions& io);

        //! Deserialize
        explicit Map(const Instance& instance, const JSON& conf);

        //! Deserialize
        explicit Map(const Instance& instance, const JSON& conf, const IOOptions& io);

     public:

        //! Serialize
        JSON to_json() const;

    public:
        //! Callback fired upon added a layer
        using LayerAdded = void(shared_ptr<Layer>, unsigned index, Revision);
        Callback<LayerAdded> onLayerAdded;

        //! Callback fired upon removing a layer
        using LayerRemoved = void(shared_ptr<Layer>, Revision);
        Callback<LayerRemoved> onLayerRemoved;

        //! Callback fired upon reordering a layer
        using LayerMoved = void(shared_ptr<Layer>, unsigned oldIndex, unsigned newIndex, Revision);
        Callback<LayerMoved> onLayerMoved;

        //! Remove a callback added to thie object
        void removeCallback(UID uid);

    protected:
        optional<std::string> _name;
        optional<std::string> _profileLayer;

    private:
        Instance _instance;
        UID _uid;
        std::vector<shared_ptr<Layer>> _layers;
        mutable std::shared_mutex _mapDataMutex;
        optional<Profile> _profile;
        Revision _dataModelRevision;

        LayerCollection _imageLayers;
        LayerCollection _elevationLayers;

        void construct(const JSON&, const IOOptions& io);

        friend class LayerCollection;
    };


    inline LayerCollection& Map::layers() {
        return _imageLayers;
    }

    inline const LayerCollection& Map::layers() const {
        return _imageLayers;
    }
}

