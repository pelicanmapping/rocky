/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/Config.h>
#include <rocky/Status.h>
#include <rocky/IOTypes.h>
#include <rocky/DateTime.h>
#include <rocky/GeoExtent.h>
#include <vector>

namespace rocky
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
    //public:
    //    /** Layer options for serialization */
    //    class ROCKY_EXPORT Options : public ConfigOptions
    //    {
    //    public:
    //        ROCKY_LayerOptions(Options, ConfigOptions);
    //        ROCKY_OPTION(std::string, name);
    //        ROCKY_OPTION(bool, openAutomatically);
    //        ROCKY_OPTION(std::string, cacheId);
    //        ROCKY_OPTION(CachePolicy, cachePolicy);
    //        ROCKY_OPTION(std::string, shaderDefine);
    //        ROCKY_OPTION(bool, terrainPatch);
    //        ROCKY_OPTION(std::string, attribution);
    //        //ROCKY_OPTION(ShaderOptions, shader);
    //        //ROCKY_OPTION_VECTOR(ShaderOptions, shaders);
    //        ROCKY_OPTION(ProxySettings, proxySettings);
    //        ROCKY_OPTION(std::string, osgOptionString);
    //        ROCKY_OPTION(unsigned int, l2CacheSize);
    //        virtual Config getConfig() const;
    //    private:
    //        void fromConfig(const Config& conf);
    //    };

    public:
        //! Hints that a layer can set to influence the operation of
        //! the map engine
        class Hints
        {
        public:
            ROCKY_OPTION(CachePolicy, cachePolicy);
            ROCKY_OPTION(unsigned, L2CacheSize);
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
        UID getUID() const { return _uid; }

        //! Open a layer.
        Status open();

        //! Open a layer. Shortcut for calling setReadOptions() followed by open().
        Status open(const IOOptions* options);

        //! Close this layer.
        Status close();

        //! Whether the layer is open
        bool isOpen() const;

        //! Status of this layer
        const Status& getStatus() const;

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
        const CachePolicy& getCachePolicy() const;

        //! Extent of this layer's data.
        //! This method may return GeoExtent::INVALID which means that the
        //! extent is unavailable (not necessarily that there is no data).
        virtual const GeoExtent& getExtent() const;

        //! Temporal extent of this layer's data.
        virtual DateTimeExtent getDateTimeExtent() const;

        //! Unique caching ID for this layer.
        //! Only set this before opening the layer or adding to a map.
        //! WARNING: You should be Very Careful when using this. The Layer will
        //! automatically generate a cache ID that is sufficient most of the time.
        //! Setting your own cache ID will require manual cache invalidation when
        //! you change certain properties. Use are your own risk!
        void setCacheID(const std::string& value);
        virtual std::string getCacheID() const;

        //! Callbacks that one can use to detect scene graph changes
        //SceneGraphCallbacks* getSceneGraphCallbacks() const;

        //! Hints that a subclass can set to influence the engine
        const Hints& getHints() const;

        //! Options string to pass to OpenSceneGraph reader/writers
        const std::string& getOsgOptionString() const;

        UID onOpen(std::function<void(Layer::ptr)>);
        UID onClose(std::function<void(Layer::ptr)>);
        void removeCallback(UID);

        std::unordered_map<UID, std::function<void(Layer::ptr)>> _onOpen;
        std::unordered_map<UID, std::function<void(Layer::ptr)>> _onClose;

    public:

#if 0
        //! Layer's stateset, creating it is necessary
        osg::StateSet* getOrCreateStateSet();

        //! Layer's stateset, or NULL if none exists
        osg::StateSet* getStateSet() const;

        //! Stateset that should be applied to an entire terrain traversal
        virtual osg::StateSet* getSharedStateSet(osg::NodeVisitor* nv) const { return NULL; }
#endif

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
        RenderType getRenderType() const { return _renderType; }

        //! Rendering type of this layer
        void setRenderType(RenderType value) { _renderType = value; }

        //! Callback that modifies the layer's bounding box for a given tile key
        virtual void modifyTileBoundingBox(const TileKey& key, const Box& box) const;

        //! Class type name without namespace. For example if the leaf class type
        const char* getTypeName() const;

        //! Attribution to be displayed by the application
        virtual std::string getAttribution() const;

        //! Attribution to be displayed by the application
        virtual void setAttribution(const std::string& attribution);

        //! Set a serialized user property
        void setUserProperty(
            const std::string& key, 
            const std::string& value);

        //! Get a serialized user property
        template<typename T>
        inline T getUserProperty(
            const std::string& key,
            T fallback) const;

    public:

#if 0
        //! Traversal callback
        class ROCKY_EXPORT TraversalCallback : public osg::Callback
        {
        public:
            virtual void operator()(osg::Node* node, osg::NodeVisitor* nv) const =0;
        protected:
            void traverse(osg::Node* node, osg::NodeVisitor* nv) const;
        };

        //! Callback invoked by the terrain engine on this layer before applying
        //! @deprecated replace with cull() override
        void setCullCallback(TraversalCallback* tc);
        const TraversalCallback* getCullCallback() const;

        //! Called to traverse this layer
        //! @deprecated Replace with cull() override
        void apply(osg::Node* node, osg::NodeVisitor* nv) const;

        //! Called by the terrain engine during the update traversal
        virtual void update(osg::NodeVisitor& nv) { }
#endif

        //! Map will call this function when this Layer is added to a Map.
        virtual void addedToMap(const class Map*) { }

        //! Map will call this function when this Layer is removed from a Map.
        virtual void removedFromMap(const class Map*) { }

    public: // osg::Object

        virtual void setName(const std::string& name);


    public: // Public internal methods

#if 0
        //! Creates a layer from serialized data - internal use
        static Layer* create(const ConfigOptions& options);

        //! Extracts config options from a DB options - internal use
        static const ConfigOptions& getConfigOptions(const IOOptions*);

        //! Adds a property notification callback to this layer
        void addCallback(LayerCallback* cb);

        //! Removes a property notification callback from this layer
        void removeCallback(LayerCallback* cb);
#endif
        //! Revision number of this layer
        int getRevision() const { return (int)_revision; }

        //! Increment the revision number for this layer, which will
        //! invalidate caches.
        virtual void dirty();

        class Options;

    protected:

        Layer();

        Layer(const Config& conf);

        //! Constructs a map layer by deserializing options.
        //Layer(
        //    Layer::Options* options,
        //    const Layer::Options* options0);

        //! Called by open() to connect to external resources and return a status.
        //! MAKE SURE you call superclass openImplementation() if you override this!
        //! This is where you will connect to back-end data sources if
        //! appropriate. When added to a map, init() is called before open()
        //! and addedToMap() is called after open() if it succeeds.
        //! By default, returns STATUS_OK.
        virtual Status openImplementation(const IOOptions* io);

        //! Called by close() to shut down the resources associated with a layer.
        virtual Status closeImplementation();

#if 0
        //! Prepares the layer for rendering if necessary.
        virtual void prepareForRendering(TerrainEngine*);
#endif

        //! Sets the status for this layer - internal
        const Status& setStatus(const Status& status) const;

        //! Sets the status for this layer with a message - internal
        const Status& setStatus(const Status::Code& statusCode, const std::string& message) const;

        //! mutable layer hints for the subclass to optionally access
        Hints& layerHints();

    private:
        UID _uid;
        //osg::ref_ptr<osg::StateSet> _stateSet;
        RenderType _renderType;
        mutable Status _status;
        //osg::ref_ptr<SceneGraphCallbacks> _sceneGraphCallbacks;
        //osg::ref_ptr<TraversalCallback> _traversalCallback;
        Hints _hints;
        std::atomic_int _revision;
        std::string _runtimeCacheId;
        //shared_ptr<IOOptions> _readOptions;
        //shared_ptr<CacheSettings> _cacheSettings;
        //std::vector<osg::ref_ptr<LayerShader> > _shaders;
        mutable util::ReadWriteMutex* _mutex;
        bool _isClosing;
        bool _isOpening;
        //std::unordered_map<UID, LayerCallback> _callbacks;

        //! Prepares the layer for rendering if necessary.
        //void invoke_prepareForRendering(TerrainEngine*);

        //! post-ctor initialization
        void construct(const Config&);

    protected:

        //shared_ptr<IOOptions> getMutableReadOptions() { return _readOptions; }

        void bumpRevision();

        // subclass can call this to change an option that requires
        // a re-opening of the layer.
        template<typename T, typename V>
        void setOptionThatRequiresReopen(T& target, const V& value);

        // subclass can call this to change an option that requires
        // a re-opening of the layer.
        template<typename T>
        void resetOptionThatRequiresReopen(T& target);

        //! internal cache information
        //CacheSettings* getCacheSettings() { return _cacheSettings.get(); }
        //const CacheSettings* getCacheSettings() const { return _cacheSettings.get(); }

        //! subclass access to a mutex that serializes the 
        //! Layer open and close methods with respect to any asynchronous
        //! functions that require the layer to remain open
        util::ReadWriteMutex& layerMutex() const { return *_mutex; }

        //! are we in the middle of a close() call?
        bool isClosing() const { return _isClosing; }

        //! are we in the middle of a open() call?
        bool isOpening() const { return _isOpening; }

    public:

        //Layer::Options& options() { return *_options; }
        //const Layer::Options& options() const { return *_options; }
        //const Layer::Options& options_original() const { return *_options0; }
        virtual const char* getConfigKey() const { return "layer" ; }

    protected:
        //Layer::Options * _options;
        //Layer::Options   _optionsConcrete;
        //const Layer::Options * _options0;
        //const Layer::Options   _optionsConcrete0;

        // no copying
        //Layer(const Layer& rhs, const osg::CopyOp& op);

        // allow the map access to the addedToMap/removedFromMap methods
        friend class Map;
        //friend class MapNode;

        //optional<std::string> _name;
        optional<bool> _openAutomatically;
        optional<std::string> _cacheid;
        optional<CachePolicy> _cachePolicy;
        optional<std::string> _attribution;
        optional<unsigned> _l2cachesize;

        bool _reopenRequired;
    };

    using LayerVector = std::vector<shared_ptr<Layer>>;


    template<typename T, typename V>
    void Layer::setOptionThatRequiresReopen(T& target, const V& value) {
        if (target != value) {
            bool wasOpen = isOpen();
            if (wasOpen && !isOpening() && !isClosing()) close();
            target = value;
            if (wasOpen && !isOpening() && !isClosing()) open();
        }
    }
    template<typename T>
    void Layer::resetOptionThatRequiresReopen(T& target) {
        if (target.has_value()) {
            bool wasOpen = isOpen();
            if (wasOpen && !isOpening() && !isClosing()) close();
            target.unset();
            if (wasOpen && !isOpening() && !isClosing()) open();
        }
    }
    template<typename T>
    T Layer::getUserProperty(const std::string& key, T fallback) const {
        return options()._internal().value(key, fallback);
    }

#if 0
#define REGISTER_ROCKY_LAYER(NAME,CLASS) \
    extern "C" void osgdb_##NAME(void) {} \
    static rocky::RegisterPluginLoader< rocky::PluginLoader<CLASS, rocky::Layer> > g_proxy_##CLASS_##NAME( #NAME );

#define USE_ROCKY_LAYER(NAME) \
    extern "C" void osgdb_##NAME(void); \
    static osgDB::PluginFunctionProxy proxy_osgearth_layer_##NAME(osgdb_##NAME);

#endif
} // namespace rocky
