/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/Callbacks.h>
#include <rocky/DateTime.h>
#include <rocky/IOTypes.h>
#include <rocky/Status.h>
#include <rocky/URI.h>
#include <shared_mutex>

namespace ROCKY_NAMESPACE
{
    class GeoExtent;
    class TileKey;

    /**
     * Base class for all Map layers.
     *
     * Subclass Layer to create a new layer type. Use the META_Layer macro
     * to establish the standard options framework.
     *
     * When you create a Layer, init() is called. Do all one-time construction
     * activity where.
     *
     * When you add a Layer to a Map, the follow methods are called in order:
     *
     *   setReadOptions() sets OSG options for IO activity;
     *   open() to initialize any underlying data sources;
     *   addedToMap() to signal to the layer that it is now a member of a Map.
     */
    class ROCKY_EXPORT Layer : public Inherit<Object, Layer>
    {
    public:
        //! Whether to automatically open this layer when added to a map.
        option<bool> openAutomatically = true;

        //! Information about the source of this layer's data
        option<Hyperlink> attribution = { };

        //! Size of the L2 (memory) cache
        option<unsigned> l2CacheSize = 0;

    public:
        //! Destructor
        virtual ~Layer();

        //! This layer's unique ID.
        //! This value is generated automatically at runtime and is not
        //! guaranteed to be the same from one run to the next.
        UID uid() const { return _uid; }

        //! Status of this layer
        const Status& status() const;

        //! Open a layer.
        Status open(const IOOptions& options);

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
        Callback<void(std::shared_ptr<Layer>)> onLayerOpened;

        //! Register a callback for layer close
        Callback<void(std::shared_ptr<Layer>)> onLayerClosed;

        void removeCallback(UID);

    public:

        //! How (and if) to use this layer when rendering terrain tiles.
        enum class RenderType
        {
            //! Layer does not draw anything (directly)
            NONE,

            //! Layer requires a terrain rendering pass that draws terrain tiles with texturing
            TERRAIN_SURFACE
        };

        //! Rendering type of this layer
        RenderType renderType() const { return _renderType; }

        //! Rendering type of this layer
        void setRenderType(RenderType value) { _renderType = value; }


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

        Layer();

        Layer(std::string_view conf);

        //! Called by open() to connect to external resources and return a status.
        //! MAKE SURE you call superclass openImplementation() if you override this!
        //! This is where you will connect to back-end data sources if
        //! appropriate. When added to a map, init() is called before open()
        //! and addedToMap() is called after open() if it succeeds.
        //! By default, returns STATUS_OK.
        virtual Status openImplementation(const IOOptions& io);

        //! Called by close() to shut down the resources associated with a layer.
        virtual void closeImplementation();

        //! Sets the status for this layer - internal
        const Status& setStatus(const Status& status) const;

        //! Sets the status for this layer with a message - internal
        const Status& setStatus(const Status::Code& statusCode, const std::string& message) const;

        //! Sets the name to use for serialization
        void setLayerTypeName(const std::string&);

    private:
        UID _uid = -1;
        RenderType _renderType = RenderType::NONE;
        mutable Status _status = Status_OK;
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

        // allow the map access to the addedToMap/removedFromMap methods
        friend class Map;
    };

} // namespace ROCKY_NAMESPACE
