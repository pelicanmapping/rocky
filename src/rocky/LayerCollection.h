/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Layer.h>
#include <rocky/Status.h>
#include <mutex>
#include <vector>

namespace ROCKY_NAMESPACE
{
    class Map;

    /**
    * Collection of layers tied to a Map.
    */
    class ROCKY_EXPORT LayerCollection
    {
    public:
        //! Add a layer
        Status add(std::shared_ptr<Layer> layer);

        //! Remove a layer
        void remove(std::shared_ptr<Layer> layer);

        //! Re-order a layer, placing it at a new index
        void move(std::shared_ptr<Layer> layer, unsigned index);

        //! Number of layers in the map
        std::size_t size() const;

        //! Size == 0
        bool empty() const { return size() == 0; }

        //! Vector (snapshot in time) of all layers
        std::vector<std::shared_ptr<Layer>> all() const;

        //! Index of a specific layer
        unsigned indexOf(std::shared_ptr<Layer> layer) const;

        //! Layer at the specified index
        template<class L = Layer> inline std::shared_ptr<L> at(unsigned index) const;

        //! Layer with the given name
        template<class L = Layer> inline std::shared_ptr<L> withName(const std::string& name) const;

        //! Layer with the given unique ID
        template<class L = Layer> inline std::shared_ptr<L> withUID(const UID& uid) const;

        //! First layer in the list of the template type
        template<class L> inline std::shared_ptr<L> firstOfType() const;

        //! A vector of all layers of the templated type
        template<class L> inline std::vector<std::shared_ptr<L>> ofType() const;

        //! Vector of all layers that pass the functor (signature takes a std::shared_ptr<Layer>)
        template<class FUNC> inline std::vector<std::shared_ptr<Layer>> get(FUNC func) const;

    public: // public properties

        //! Whether to call layer->open() when adding a layer to the collection
        bool openOnAdd = false;

        //! Whether to call layer->close() when removing a layer from the collection
        bool closeOnRemove = true;

    private:
        friend class Map;
        LayerCollection(Map* map);
        inline LayerCollection(const LayerCollection&) = delete;
        std::vector<std::shared_ptr<Layer>> _layers;
        std::shared_ptr<Layer> _at(unsigned index) const;
        std::shared_ptr<Layer> _withName(const std::string& name) const;
        std::shared_ptr<Layer> _withUID(const UID& uid) const;
        Map* _map;
        std::shared_mutex& _map_mutex;
    };


    template<class L> std::shared_ptr<L> LayerCollection::at(unsigned index) const {
        return L::cast(_at(index));
    }

    template<class L> std::shared_ptr<L> LayerCollection::withName(const std::string& name) const {
        return L::cast(_withName(name));
    }

    template<class L> std::shared_ptr<L> LayerCollection::withUID(const UID& uid) const {
        return L::cast(_withUID(uid));
    }

    template<class L> std::shared_ptr<L> LayerCollection::firstOfType() const {
        std::shared_lock lock(_map_mutex);
        for (auto& layer : _layers) {
            std::shared_ptr<L> r = L::cast(layer);
            if (r) return r;
        }
        return nullptr;
    }

    template<class L> std::vector<std::shared_ptr<L>> LayerCollection::ofType() const {
        std::vector<std::shared_ptr<L>> output;
        std::shared_lock lock(_map_mutex);
        for (auto& layer : _layers) {
            auto r = L::cast(layer);
            if (r) output.emplace_back(r);
        }
        return output;
    }

    template<class FUNC> std::vector<std::shared_ptr<Layer>> LayerCollection::get(FUNC func) const {
        std::vector<std::shared_ptr<Layer>> output;
        std::shared_lock lock(_map_mutex);
        for (auto& layer : _layers) {
            if (func(layer))
                output.emplace_back(layer);
        }
        return output;
    }
}