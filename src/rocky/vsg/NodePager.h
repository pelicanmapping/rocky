/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/VSGContext.h>
#include <rocky/Profile.h>
#include <rocky/TileKey.h>
#include <rocky/IOTypes.h>
#include <rocky/SentryTracker.h>
#include <rocky/Callbacks.h>

namespace ROCKY_NAMESPACE
{
    /**
    * Node that manages a dynamically paged scene graph.
    * The graph's structure is based on a Profile and each represents a TileKey in 
    * that Profile.
    *
    * To use a NodePager you must set the context, profile, and createPayload function.
    *
    * TODO: support the concept of sampling elevation to get a bounding sphere at
    * the correct Z for a tile.
    */
    class ROCKY_EXPORT NodePager : public vsg::Inherit<vsg::Group, NodePager>
    {
    public:
        //! Policy for refining leveld of detail
        enum class RefinePolicy
        {
            //! Replace the lower level of detail payload with the higher one
            Replace,

            //! Render all loaded levels of detail at once
            Accumulate
        };

        using BoundCalculator = std::function<vsg::dsphere(const TileKey& key, const IOOptions& io)>;

        using PayloadCreator = std::function<vsg::ref_ptr<vsg::Node>(const TileKey& key, const IOOptions& io)>;

        using SubtileLoader = std::function<vsg::ref_ptr<vsg::Node>(const IOOptions& io)>;

        using SubtileLoaderFactory = std::function<SubtileLoader(const TileKey& key)>;


    public:
        //! Construct a new node pager whose tiles will correspond to
        //! a tiling profile.
        //! \param graphProfile The profile to use for tiling. Must be valid.
        //! \param sceneSRS The SRS of the rendered map node (i.e. MapNode::srs)
        NodePager(const Profile& graphProfile, const SRS& sceneSRS);

        //! Call this after configuring the pager's settings.
        void initialize(VSGContext);

        //! Whether this pager is paging
        bool active = false;

        //! Tiling profile this pager will use to create tiles
        Profile profile;

        //! Function that creates the payload for a tile key.
        PayloadCreator createPayload;

        //! Function that calculates a bounding sphere for a tile key.
        BoundCalculator calculateBound;
       
        //! Fired when expired data is about to be removed from the scene graph
        Callback<void(vsg::ref_ptr<vsg::Object>)> onExpire;

        //! Min level at which to create payloads
        unsigned minLevel = 0;

        //! Max level to which to subdivide
        unsigned maxLevel = 18;

        //! Whether payloads accumulate as the level increases (Add), or whether
        //! they replace the lower-LOD payload (Replace)
        RefinePolicy refinePolicy = RefinePolicy::Replace;

        //! LOD switching metric (size of tile on screen)
        float pixelError = 512.0f; // pixels 

        //! Name of the job pool to use for node paging
        std::string poolName = "rocky::nodepager";

        //! Custom factory that will creaet a subtile loader function.
        SubtileLoaderFactory subtileLoaderFactory = nullptr;

    public:

        //! Number of tiles under management (snapshot in time; for debugging)
        unsigned tiles() const;

        //! Tiles resident (for debugging)
        std::vector<TileKey> tileKeys() const;

        TileKey debugKey;

    protected:

        VSGContext _vsgcontext;
        mutable detail::SentryTracker<vsg::ref_ptr<vsg::Node>> _sentry;
        mutable std::mutex _sentry_mutex;
        CallbackSub _sentryUpdate;
        std::uint64_t _lastUpdateFrame = 0u;
        SRS _renderingSRS;

        friend class PagedNode;

        //! Creates a node for a TileKey.
        vsg::ref_ptr<vsg::Node> createNode(const TileKey& key, const IOOptions& io) const;

        //! Creates a function that loads the subtiles of a key.
        SubtileLoader createSubtileLoader(const TileKey& key) const;

        //! Called internally to notify the pager that a tile is still alive.
        void* touch(vsg::Node*, void*) const;

    };
}
