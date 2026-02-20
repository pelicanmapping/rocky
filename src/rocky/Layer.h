/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
/**
 * @file Layer.h
 * @defgroup core_layers Layer Classes
 * @brief Base classes and layer types for map data
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/Callbacks.h>
#include <rocky/DateTime.h>
#include <rocky/IOTypes.h>
#include <rocky/Result.h>
#include <rocky/URI.h>
#include <shared_mutex>

namespace ROCKY_NAMESPACE
{
    class GeoExtent;
    class TileKey;

    /**
     * @ingroup core_layers
     * @brief Base class for all Map layer types
     */
    class ROCKY_EXPORT Layer : public Inherit<Object, Layer>
    {
    public:
        //! Whether to automatically open this layer when added to a map.
        option<bool> openAutomatically = true;

        //! Information about the source of this layer's data
        option<Hyperlink> attribution = { };

    public:
        //! Destructor
        virtual ~Layer();

        //! This layer's unique ID.
        //! This value is generated automatically at runtime and is not
        //! guaranteed to be the same from one run to the next.
        UID uid() const { return _uid; }

        //! Status of this layer
        inline const Status& status() const {
            return _status;
        }

        //! Open a layer.
        Result<> open(const IOOptions& options);

        //! Close this layer.
        void close();

        //! Whether the layer is open without error
        bool isOpen() const;

        //! Serialize this layer into a JSON string
        virtual std::string to_json() const;

        //! Extent of this layer's data.
        //! This method may return GeoExtent::INVALID which means that the
        //! extent is unavailable (not necessarily that there is no data).
        virtual const GeoExtent& extent() const;

        //! Temporal extent of this layer's data.
        virtual DateTimeExtent dateTimeExtent() const;

        //! Register a callback for layer open
        Callback<void(const Layer*)> onLayerOpened;

        //! Register a callback for layer close
        Callback<void(const Layer*)> onLayerClosed;

    public:

        //! Revision number of this layer
        Revision revision() const { return _revision; }

        //! Increment the revision number for this layer, which will invalidate caches.
        virtual void dirty();

        //! Name of the layer type for serialization use
        const std::string& getLayerTypeName() const {
            return _layerTypeName;
        }

        class Options;

    protected:

        //! Construct the layer (from a subclass)
        Layer();

        //! Construct a layer (from serialized JSON)
        Layer(std::string_view conf);

        //! Called by open() to connect to external resources and return a status.
        //! MAKE SURE you call superclass openImplementation() if you override this!
        //! This is where you will connect to back-end data sources if
        //! appropriate. When added to a map, init() is called before open()
        //! and addedToMap() is called after open() if it succeeds.
        //! By default, returns STATUS_OK.
        virtual Result<> openImplementation(const IOOptions& io);

        //! Called by close() to shut down the resources associated with a layer.
        virtual void closeImplementation();

        //! Sets the status for this layer - internal
        const Failure& fail(const Failure& failure) const;

        //! Sets the status for this layer with a message - internal
        const Failure& fail(const Failure::Type type, std::string_view message) const;

        //! Sets the name to use for serialization
        void setLayerTypeName(const std::string&);

    private:
        UID _uid = -1;
        mutable Status _status;
        std::atomic<Revision> _revision = { 1 };
        mutable std::shared_mutex _state_mutex;
        std::string _layerTypeName;

        //! post-ctor initialization
        void construct(std::string_view);

    protected:

        void bumpRevision();

        //! subclass access to a mutex that serializes the 
        //! Layer open and close methods with respect to any asynchronous
        //! functions that require the layer to remain open
        std::shared_mutex& layerStateMutex() const { return _state_mutex; }

        // allow the map access
        friend class Map;
    };

} // namespace ROCKY_NAMESPACE
