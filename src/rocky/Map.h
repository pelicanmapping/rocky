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
//#include <rocky/ElevationPool.h>
#include <functional>
#include <set>

namespace ROCKY_NAMESPACE
{
    /**
     * Map is the main data model. A Map hold a collection of
     * Layers, each of which provides data of some kind to the
     * overall Map. Use a MapNode to render the contents of a
     * Map.
     */
    class ROCKY_EXPORT Map : public Inherit<Object, Map>
    {
    public:

        //! Global application instance
        Instance::ptr instance() const { return _instance; }

        //! This Map's unique ID
        UID uid() const { return _uid; }

        //! The map's master tiling profile, which defines its SRS and tiling structure
        void setProfile(const Profile&);
        const Profile& profile() const;

        //! Spatial reference system of the map's profile (convenience)
        const SRS& srs() const;

        //! Adds a Layer to the map.
        void addLayer(shared_ptr<Layer> layer);
        void addLayer(shared_ptr<Layer> layer, const IOOptions& io);

        //! Adds a collection of layers to the map.
        void addLayers(const std::vector<shared_ptr<Layer>>& layers);
        void addLayers(const std::vector<shared_ptr<Layer>>& layers, const IOOptions& io);

        //! Insert a Layer at a specific index in the Map.
        void insertLayer(
            shared_ptr<Layer> layer,
            unsigned index);

        void insertLayer(
            shared_ptr<Layer> layer,
            unsigned index,
            const IOOptions& io);

        //! Removes a layer from the map.
        void removeLayer(shared_ptr<Layer> layer);

        //! Moves a layer to another position in the Map.
        void moveLayer(shared_ptr<Layer> layer, unsigned index);

        //! Index of the specified layer, or returns getNumLayers() if the layer is not found.
        unsigned getIndexOfLayer(const Layer* layer) const;

        //! Fills the vector with references to all layers of the specified type
        //! and returns the corresponding revision number.
        template<typename T>
        std::vector<shared_ptr<const T>> getLayers(
            Revision* out_revision = nullptr) const;

        template<typename T>
        std::vector<shared_ptr<T>> getLayers(
            Revision* out_revision = nullptr);

        //! Number of layers in the map.
        unsigned getNumLayers() const;

        //! Gets a layer by name.
        shared_ptr<Layer> getLayerByName(const std::string& name) const;
        template<typename T> shared_ptr<T> getLayerByName(const std::string& name) const;

        //! Gets an image layer by its unique ID.
        shared_ptr<Layer> getLayerByUID(UID layerUID) const;
        template<typename T> shared_ptr<T> getLayerByUID(UID layerUID) const;

        //! Gets the layer at the specified index.
        shared_ptr<Layer> getLayerAt(unsigned index) const;
        template<typename T> shared_ptr<T> getLayerAt(unsigned index) const;

        //! Fills the vector with references to all layers of the specified type
        //! and returns the corresponding revision number.
        template<typename T>
        Revision getOpenLayers(std::vector<shared_ptr<T>>& output) const;


        //! Copies references of the map layers into the output list for which
        //! the predicate function returns true.
        //! This method is thread safe. It returns the map revision that was
        //! in effect when the data was copied.
        inline std::vector<shared_ptr<Layer>> getLayers(
            const std::function<bool(const Layer*)>& accept,
            Revision* out_revision =nullptr) const;

        //! Fills the vector with references to all layers satisflying the predicate
        //! and returns the corresponding revision number.
        template<typename T>
        std::vector<shared_ptr<T>> getLayers(
            const std::function<bool(const T*)>& predicate,
            Revision* out_revision =nullptr) const;

        //! The first layer of the specified type. This is useful when you
        //! know there in only one layer of the type you are looking for.
        template<typename T> shared_ptr<T> getLayer() const;

        //! Removes all layers from this map.
        void clear();

        //! Gets the revision # of the map. The revision # changes every time
        //! you add, remove, or move layers. You can use this to track changes
        //! in the map model (as a alternative to installing a MapCallback).
        Revision getDataModelRevision() const;

        //! Gets a version of the map profile without any vertical datum
        const Profile& getProfileNoVDatum() const { return _profileNoVDatum; }

        //! Access to an elevation sampling service tied to this map
        //ElevationPool* getElevationPool() const;

        //! List of attribution strings to be displayed by the application
        void getAttributions(std::set<std::string>& attributions) const;

        //! Number of layers marked as terrain pathes
        //int getNumTerrainPatchLayers() const;

        //void setName(const std::string& value) { options().name() = value; }
        //const std::string& getName() const { return options().name().get(); }

        //shared_ptr<IOOptions> getReadOptions() const { return _readOptions; }

        void removeCallback(UID uid);

    public:

        //! Construct (please use Map::create)
        Map();

        //! Construct
        Map(Instance::ptr instance);

        //! Construct with custom options
        Map(Instance::ptr, const IOOptions& io);

        //! Deserialize
        Map(const Config& conf, Instance::ptr, const IOOptions& io);

     public:

        //! Serialize
        Config getConfig() const;

    public:
        //! Callbacks
        using LayerAdded = std::function<void(shared_ptr<Layer>, unsigned index, Revision)>;
        Callback<LayerAdded> onLayerAdded;

        using LayerRemoved = std::function<void(shared_ptr<Layer>, Revision)>;
        Callback<LayerRemoved> onLayerRemoved;

        using LayerMoved = std::function<void(shared_ptr<Layer>, unsigned oldIndex, unsigned newIndex, Revision)>;
        Callback<LayerMoved> onLayerMoved;

    protected:
        optional<std::string> _name;
        optional<std::string> _profileLayer;

    private:
        Instance::ptr _instance;
        UID _uid;
        std::vector<shared_ptr<Layer>> _layers;
        mutable util::ReadWriteMutex _mapDataMutex;
        Profile _profile;
        Profile _profileNoVDatum;
        //shared_ptr<ElevationPool> _elevationPool;
        Revision _dataModelRevision;

        void construct(const Config&, const IOOptions& io);
    };


    // Templated inline methods

    template<typename T> shared_ptr<T> Map::getLayerByName(const std::string& name) const {
        return T::cast(getLayerByName(name));
    }

    template<typename T> shared_ptr<T> Map::getLayerByUID(UID layerUID) const {
        return T::cast(getLayerByUID(layerUID));
    }

    template<typename T> shared_ptr<T> Map::getLayerAt(unsigned index) const {
        return T::cast(getLayerAt(index));
    }

    template<typename T>
    std::vector<shared_ptr<const T>>
    Map::getLayers(Revision* out_rev) const
    {
        util::ScopedReadLock lock(_mapDataMutex);
        std::vector<shared_ptr<const T>> output;
        for (auto& i : _layers) {
            auto obj = T::cast(i);
            if (obj) output.push_back(obj);
        }
        if (out_rev) *out_rev = _dataModelRevision;
        return std::move(output);
    }

    template<typename T>
    std::vector<shared_ptr<T>>
    Map::getLayers(Revision* out_rev)
    {
        util::ScopedReadLock lock(_mapDataMutex);
        std::vector<shared_ptr<T>> output;
        for (auto& i : _layers) {
            auto obj = T::cast(i);
            if (obj) output.push_back(obj);
        }
        if (out_rev) *out_rev = _dataModelRevision;
        return std::move(output);
    }

    template<typename T>
    Revision Map::getOpenLayers(std::vector<shared_ptr<T>>& output) const {
        util::ScopedReadLock lock(_mapDataMutex);
        for (auto& i : _layers) {
            if (i->isOpen()) {
                auto obj = T::cast(i);
                if (obj) output.push_back(obj);
            }
        }
        return _dataModelRevision;
    }

    template<typename T> shared_ptr<T> Map::getLayer() const {
        util::ScopedReadLock lock(_mapDataMutex);
        for (auto& i : _layers) {
            auto obj = T::cast(i);
            if (obj) return obj;
        }
        return 0L;
    }

    std::vector<shared_ptr<Layer>> Map::getLayers(
        const std::function<bool(const Layer*)>& accept,
        Revision* out_revision) const
    {
        util::ScopedReadLock lock(_mapDataMutex);
        std::vector<shared_ptr<Layer>> output;
        for (auto& layer : _layers) {
            if (accept(layer.get()))
                output.push_back(layer);
        }
        if (out_revision)
            *out_revision = _dataModelRevision;
        return std::move(output);
    }

    template<typename T>
    std::vector<shared_ptr<T>> Map::getLayers(
        const std::function<bool(const T*)>& accept,
        Revision* out_revision) const
    {
        util::ScopedReadLock lock(_mapDataMutex);
        std::vector<shared_ptr<T>> output;
        for (auto& i : _layers) {
            auto obj = T::cast(i);
            if (obj != nullptr && accept(obj.get()))
                output.push_back(obj);
        }
        if (out_revision)
            *out_revision = _dataModelRevision;
        return std::move(output);
    }
}
