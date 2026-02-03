/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "NodePager.h"
#include <rocky/vsg/VSGUtils.h>
#include <atomic>

using namespace ROCKY_NAMESPACE;

namespace ROCKY_NAMESPACE
{
    class PagedNode : public vsg::Inherit<vsg::CullNode, PagedNode>
    {
    public:
        TileKey key;
        const NodePager* pager = nullptr;
        mutable void* token = nullptr;
        bool canLoadChild = false;
        mutable float priority = 0.0f;
        int revision = 0;
        mutable std::atomic_bool load_gate = { false };
        vsg::ref_ptr<vsg::Node> payload;
        mutable jobs::future<vsg::ref_ptr<vsg::Node>> child;

        //! Kick off a job to load this node's subtile children.
        void startLoading() const;

        //! Remove this node's subtiles and reset its state.
        void unload(VSGContext vsgcontext);

        void traverse(vsg::Visitor& visitor) override {
            if (payload)
                payload->accept(visitor);
            if (auto temp = child.available() ? child.value() : nullptr)
                temp->accept(visitor);
        }

        void traverse(vsg::ConstVisitor& visitor) const override {
            if (payload)
                payload->accept(visitor);
            if (auto temp = child.available() ? child.value() : nullptr)
                temp->accept(visitor);
        }

        void traverse(vsg::RecordTraversal& record) const override;
    };
}


NodePager::NodePager(const Profile& graphProfile, const SRS& sceneSRS) :
    vsg::Inherit<vsg::Group, NodePager>(), 
    profile(graphProfile),
    _renderingSRS(sceneSRS)
{
    ROCKY_SOFT_ASSERT(profile.valid());
}

void
NodePager::initialize(VSGContext& vsgcontext)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(vsgcontext, void());
    ROCKY_SOFT_ASSERT_AND_RETURN(profile.valid(), void());
    ROCKY_SOFT_ASSERT_AND_RETURN(createPayload != nullptr, void());

    this->_vsgcontext = vsgcontext;

    for (auto& child : children)
        vsgcontext->dispose(child);

    children.clear();

    // build the root nodes of the profile graph:
    auto rootKeys = profile.rootKeys();
    for (auto& key : rootKeys)
    {
        auto node = createNode(key, vsgcontext->io);
        if (node)
            addChild(node);
    }

    // install an update operation that will flush the culling sentry each frame,
    // removing invisible nodes from the scene graph.
    _sentryUpdate = vsgcontext->onUpdate([this, vsgcontext]()
        {
            auto frame = vsgcontext->viewer()->getFrameStamp()->frameCount;

            // only if the frame advanced:
            if (frame > _lastUpdateFrame)
            {
                std::scoped_lock lock(_sentry_mutex);

                _sentry.flush(~0u, 0u, [vsgcontext](vsg::ref_ptr<vsg::Node> node) mutable
                    {
                        if (node)
                        {
                            auto* paged = node->cast<PagedNode>();
                            paged->unload(vsgcontext);
                        }
                        return true;
                    });
            }

            _lastUpdateFrame = frame;
        });

    active = true;
}

NodePager::SubtileLoader
NodePager::createSubtileLoader(const TileKey& key) const
{
    if (!active)
        return {};

    // If the user installed their own factory function, call it.
    if (subtileLoaderFactory)
    {
        return subtileLoaderFactory(key);
    }

    // By default, return a subtile loader that creates quadtile children
    // of the provided key.
    vsg::observer_ptr<NodePager> weak_pager(const_cast<NodePager*>(this));

    return [weak_pager, key](const IOOptions& io) -> vsg::ref_ptr<vsg::Node>
        {
            vsg::ref_ptr<vsg::Group> result;

            if (auto pager = weak_pager.ref_ptr())
            {
                // create four quadtree children of the tile key.
                for (unsigned i = 0; i < 4; ++i)
                {
                    if (io.canceled())
                        return {};

                    auto child_key = key.createChildKey(i);
                    auto child = pager->createNode(child_key, io);
                    if (child)
                    {
                        if (!result)
                            result = vsg::Group::create();

                        result->addChild(child);
                    }
                }

                if (result)
                {
                    pager->_vsgcontext->compile(result);
                }
            }

            return result;
        };
}

vsg::ref_ptr<vsg::Node>
NodePager::createNode(const TileKey& key, const IOOptions& io) const
{
    vsg::ref_ptr<vsg::Node> result;

    auto mapExtent = key.extent().transform(_renderingSRS);

    vsg::dsphere tileBound =
        calculateBound ? calculateBound(key, io) :
        to_vsg(mapExtent.createWorldBoundingSphere(0, 0));

    bool haveChildren = key.level < maxLevel;
    bool mayHavePayload = key.level >= minLevel;

    // Create the actual drawable data for this tile.
    vsg::ref_ptr<vsg::Node> payload;

    // payload, which may or may not exist at this level:
    if (mayHavePayload)
    {
        payload = createPayload(key, io);
    }

    if (io.canceled())
    {
        return result;
    }

    if (haveChildren)
    {
        auto p = PagedNode::create();
        p->key = key;
        p->bound = tileBound;
        p->priority = (float)key.level;
        p->pager = this;
        p->canLoadChild = true;

        if (payload)
            p->payload = payload;

        result = p;
    }

    else if (payload)
    {
        result = payload;
    }

    return result;
}

void*
NodePager::touch(vsg::Node* node, void* token) const
{
    if (!active)
        return nullptr;

    std::scoped_lock lock(_sentry_mutex);

    if (token)
        token = _sentry.update(token);
    else
        token = _sentry.emplace(vsg::ref_ptr<vsg::Node>(node));

    return token;
}

unsigned
NodePager::tiles() const
{
    return _sentry._size;
}

std::vector<TileKey>
NodePager::tileKeys() const
{
    std::vector<TileKey> result;
    result.reserve(_sentry._size);
    std::scoped_lock lock(_sentry_mutex);
    for (const auto& entry : _sentry._list)
    {
        if (entry._data)
        {
            if (auto paged = entry._data->cast<PagedNode>())
            {
                result.emplace_back(paged->key);
            }
        }
    }
    return result;
}



void
PagedNode::startLoading() const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(pager, void());

    jobs::context jc;
    jc.name = key.str();
    jc.pool = jobs::get_pool(pager->poolName, 4);
    jc.priority = [&]() { return priority; };

    auto load = pager->createSubtileLoader(key);
    ROCKY_SOFT_ASSERT_AND_RETURN(load, void());

    vsg::observer_ptr<PagedNode> parent_weak(const_cast<PagedNode*>(this));

    auto load_job = [load, parent_weak, vsgcontext(pager->_vsgcontext), orig_revision(revision), io(pager->_vsgcontext->io)](Cancelable& c)
        {
            vsg::ref_ptr<vsg::Node> result = load(io.with(c));
            return result;
        };

    child = jobs::dispatch(load_job, jc);
}

void
PagedNode::traverse(vsg::RecordTraversal& record) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(pager, void());

    if (canLoadChild)
    {
        // check whether the subtiles are in range.
        auto& vp = record.getCommandBuffer()->viewDependentState->viewportData->at(0);
        auto min_screen_height_ratio = pager->pixelError / vp[3];
        auto d = record.getState()->lodDistance(bound);
        bool child_in_range = (d > 0.0) && (bound.r > (d * min_screen_height_ratio));

        priority = -d;

        if (key == pager->debugKey)
        {
            Log()->debug("Debugging {}", key.str());
        }

        // access once for atomicness
        vsg::ref_ptr<vsg::Node> child_value = child.available() ? child.value() : nullptr;

        if (payload)
        {
            if (pager->refinePolicy == NodePager::RefinePolicy::Accumulate || !child_in_range || !child_value)
            {
                payload->accept(record);
            }
        }

        if (child_in_range)
        {
            if (!load_gate.exchange(true))
            {
                startLoading();
            }
            else if (child.working())
            {
                pager->_vsgcontext->requestFrame();
            }
            else if (child.canceled())
            {
                Log()->warn("subtile load for {} was canceled, no subtiles available", key.str());
            }

            if (child_value)
            {
                child_value->accept(record);
            }
        }
    }

    // no children allowed (leaf node), just take the payload.
    else if (payload)
    {
        payload->accept(record);
    }

    // let the pager know that this node was visited.
    token = pager->touch(const_cast<PagedNode*>(this), token);
}

void
PagedNode::unload(VSGContext vsgcontext)
{    
    // expire and dispose of the data
    if (child.available() && child.value())
    {
        pager->onExpire.fire(child.value());
        vsgcontext->dispose(child.value());
    }

    // reset everything to the initial state.
    child.reset();
    load_gate.exchange(false);
    token = nullptr;

    // bump the revision.
    revision++;
}
