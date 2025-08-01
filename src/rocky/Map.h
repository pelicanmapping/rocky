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
#include <shared_mutex>
#include <vector>

namespace ROCKY_NAMESPACE
{
    /**
     * Map is the main data model, which holds a collection of layers.
     * Create a Map by calling Map::create().
     */
    class ROCKY_EXPORT Map : public Inherit<Object, Map>
    {
    public:
        //! Ordered collection of map layers.
        using Layers = std::vector<Layer::Ptr>;
        
        //! Construct an empty map
        Map() = default;

        //! Adds a layer to the map.
        void add(Layer::Ptr layer);

        //! Sets the layers collection
        void setLayers(const Layers& layers);

        //! Sets the layers collection
        void setLayers(Layers&& layers) noexcept;

        //! A safe copy of all the layers that match the optional cast type and the optional predicate function.
        //! Examples:
        //!     auto layers = map.layers(); // fetch all layers
        //!     auto layers = map.layers<ImageLayer>(); // fetch all layers of type ImageLayer
        //!     auto layers = map.layers([](auto layer) { return layer->name() == "MyLayer"; }); // fetch all layers with a given name
        template<class T = Layer, class PREDICATE = std::function<bool(typename T::ConstPtr)>>
        inline std::vector<typename T::Ptr> layers(PREDICATE&& pred = [](typename T::ConstPtr) { return true; }) const;

        //! Pointer to the first layer that matches both the optional type cast and the optional predicate function.
        template<class T = Layer, class PREDICATE = std::function<bool(typename T::ConstPtr)>>
        inline typename T::Ptr layer(PREDICATE&& pred = [](typename T::ConstPtr) { return true; }) const;

        //! Iterate safely over all layers, calling the callable.
        template<typename CALLABLE>
        inline void each(CALLABLE&&) const;

        //! Open all layers that are marked for openAutomatically.
        //! Note, this method will be called automatically by the MapNode, but you 
        //! are free to call it manually if you want to force all layers to open 
        //! and check for errors.
        Result<> openAllLayers(const IOOptions& options);

        //! Gets the revision # of the map. The revision # changes every time
        //! you add, remove, or move layers. You can use this to track changes
        //! in the map model (as a alternative to installing a MapCallback).
        Revision revision() const;

        //! Deserialize from JSON data
        //! @param value JSON string to import
        Result<> from_json(std::string_view value, const IOOptions& io);

        //! Serialize
        std::string to_json() const;

    public:
        //! Callback fired when the layers vector changes
        Callback<void(const Map*)> onLayersChanged;

    private:
        mutable std::shared_mutex _mutex;
        Revision _revision = 0;
        std::vector<Layer::Ptr> _layers;
    };



    // inlines
    template<class T, class PREDICATE>
    inline std::vector<typename T::Ptr> Map::layers(PREDICATE&& pred) const
    {
        static_assert(std::is_invocable_r_v<bool, PREDICATE, typename T::Ptr>,
            "Map::layers() requires a predicate matching the signature bool(T::ConstPtr)");

        std::vector<typename T::Ptr> result;
        std::shared_lock lock(_mutex);
        for (auto& layer : _layers)
        {
            typename T::Ptr typed = T::cast(layer);
            if (typed && pred(typed))
            {
                result.emplace_back(typed);
            }
        }
        return result;
    }

    template<class T, class PREDICATE>
    inline typename T::Ptr Map::layer(PREDICATE&& pred) const
    {
        static_assert(std::is_invocable_r_v<bool, PREDICATE, typename T::Ptr>,
            "Map::layer() requires a predicate matching the signature bool(T::ConstPtr)");

        typename T::Ptr result;
        std::shared_lock lock(_mutex);
        for (auto& layer : _layers)
        {
            typename T::Ptr typed = T::cast(layer);
            if (typed && pred(typed))
                return typed;
        }
        return {};
    }

    template<typename CALLABLE>
    inline void Map::each(CALLABLE&& func) const
    {
        static_assert(std::is_invocable_r_v<void, CALLABLE, Layer::ConstPtr>,
            "Map::each() requires a callable that takes a Layer::ConstPtr and returns void");

        std::shared_lock lock(_mutex);
        for (const auto& layer : _layers)
        {
            func(layer.get());
        }
    }
}

