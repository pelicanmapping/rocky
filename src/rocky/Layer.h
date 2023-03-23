/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/Callbacks.h>
#include <rocky/Config.h>
#include <rocky/DateTime.h>
#include <rocky/IOTypes.h>
#include <rocky/Status.h>
#include <vector>
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
        //! Hints that a layer can set to influence the operation of
        //! the map engine
        class Hints
        {
        public:
            ROCKY_OPTION(CachePolicy, cachePolicy);
            //ROCKY_OPTION(unsigned, L2CacheSize);
            ROCKY_OPTION(bool, dynamic);
        };

    public:

        //! Destructor
        virtual ~Layer();

        //! Attempt to create a layer from a serialization string.
        static shared_ptr<Layer> materialize(const ConfigOptions&);

        //! This layer's unique ID.
        //! This value is generated automatically at runtime and is not
        //! guaranteed to be the same from one run to the next.
        UID uid() const { return _uid; }

        //! Status of this layer
        const Status& status() const;

        //! Open a layer.
        Status open();

        //! Open a layer.
        Status open(const IOOptions& options);

        //! Close this layer.
        void close();

        //! Whether the layer is open
        bool isOpen() const;

        //! Serialize this layer into a Config object (if applicable)
        virtual Config getConfig() const;

        //! Whether to automatically open this layer (by calling open) when
        //! adding the layer to a Map or when opening a map containing this Layer.
        virtual void setOpenAutomatically(bool value);

        //! Whether to automatically open this layer (by calling open) when
        //! adding the layer to a Map or when opening a map containing this Layer.
        virtual bool getOpenAutomatically() const;

        //! Cacheing policy. Only set this before opening the layer or adding to a map.
        void setCachePolicy(const CachePolicy& value);
        const CachePolicy& cachePolicy() const;

        //! Extent of this layer's data.
        //! This method may return GeoExtent::INVALID which means that the
        //! extent is unavailable (not necessarily that there is no data).
        virtual const GeoExtent& extent() const;

        //! Temporal extent of this layer's data.
        virtual DateTimeExtent dateTimeExtent() const;

        //! Hints that a subclass can set to influence the engine
        const Hints& hints() const;

        //! Register a callback for layer open
        using LayerOpened = std::function<void(shared_ptr<Layer>)>;
        Callback<LayerOpened> onLayerOpened;

        //! Register a callback for layer close
        using LayerClosed = std::function<void(shared_ptr<Layer>)>;
        Callback<LayerClosed> onLayerClosed;

        void removeCallback(UID);

    public:

        //! How (and if) to use this layer when rendering terrain tiles.
        enum RenderType
        {
            //! Layer does not draw anything (directly)
            RENDERTYPE_NONE,

            //! Layer requires a terrain rendering pass that draws terrain tiles with texturing
            RENDERTYPE_TERRAIN_SURFACE,

            //! Layer requires a terrain rendering pass that emits terrian patches or
            //! invokes a custom drawing function
            RENDERTYPE_TERRAIN_PATCH,

            //! Layer that renders its own node graph or other geometry (no terrain)
            RENDERTYPE_CUSTOM = RENDERTYPE_NONE
        };

        //! Rendering type of this layer
        RenderType renderType() const { return _renderType; }

        //! Rendering type of this layer
        void setRenderType(RenderType value) { _renderType = value; }

        //! Callback that modifies the layer's bounding box for a given tile key
        virtual void modifyTileBoundingBox(const TileKey& key, const Box& box) const;

        //! Class type name without namespace. For example if the leaf class type
        const char* typeName() const;

        //! Attribution to be displayed by the application
        virtual std::string attribution() const;

        //! Attribution to be displayed by the application
        virtual void setAttribution(const std::string& attribution);

        //! Set a serialized user property
        void setUserProperty(
            const std::string& key, 
            const std::string& value);

        //! Get a serialized user property
        template<typename T>
        inline T userProperty(
            const std::string& key,
            T fallback) const;

    public:

        //! Map will call this function when this Layer is added to a Map.
        //virtual void addedToMap(const class Map*) { }

        //! Map will call this function when this Layer is removed from a Map.
        //virtual void removedFromMap(const class Map*) { }

        //! Revision number of this layer
        Revision revision() const { return _revision; }

        //! Increment the revision number for this layer, which will
        //! invalidate caches.
        virtual void dirty();

        //! Name of the layer type for serialization use
        const std::string& getConfigKey() const {
            return _configKey;
        }

        class Options;

    protected:

        Layer();

        Layer(const Config& conf);

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

        //! Sets the config name to use for serialization
        void setConfigKey(const std::string&);

    private:
        UID _uid;
        RenderType _renderType;
        mutable Status _status;
        Hints _hints;
        std::atomic<Revision> _revision;
        std::string _runtimeCacheId;
        mutable std::shared_mutex _state_mutex;
        bool _isClosing;
        bool _isOpening;
        std::string _configKey;

        //! post-ctor initialization
        void construct(const Config&);

    protected:

        void bumpRevision();

        //! subclass access to a mutex that serializes the 
        //! Layer open and close methods with respect to any asynchronous
        //! functions that require the layer to remain open
        std::shared_mutex& layerStateMutex() const { return _state_mutex; }

    public:

    protected:

        // allow the map access to the addedToMap/removedFromMap methods
        friend class Map;

        optional<bool> _openAutomatically = true;
        optional<std::string> _cacheid = { };
        optional<CachePolicy> _cachePolicy = CachePolicy::DEFAULT;
        optional<std::string> _attribution = { };
        optional<unsigned> _l2cachesize = 0;

        bool _reopenRequired;
    };

    using LayerVector = std::vector<shared_ptr<Layer>>;

    template<typename T>
    T Layer::userProperty(const std::string& key, T fallback) const {
        return options()._internal().value(key, fallback);
    }

} // namespace ROCKY_NAMESPACE
