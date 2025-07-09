/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/Common.h>
#include <rocky/vsg/ecs.h>
#include <vector>

namespace ROCKY_NAMESPACE
{
    /**
     * A scene graph node that holds a collection of ECS entities.
     *
     * Each entity must have a Visibility component.
     * The node will update the Visibility component with the current frame count
     * during each record traveral that hits it.
     */
    class /*ROCKY_EXPORT*/ EntityNode : public vsg::Inherit<vsg::Node, EntityNode>
    {
    public:
        //! Entities in this node.
        //! Each entity must have a Visibility component.
        std::vector<entt::entity> entities;

        //! Whether to destroy all entities in the destructor
        bool autoDestroy = true;

        //! Construct a new entity node (with EntityNode::create)
        //! \param reg The ECS registry to use when updating or destorying
        //!     the entities this node manages
        EntityNode(ecs::Registry& reg) :
            _registry(reg)
        {
            //nop
        }

        //! Destruct the node and optionally destroy all entities
        ~EntityNode()
        {
            if (autoDestroy)
            {
                _registry.write()->destroy(entities.begin(), entities.end());
            }
        }

        //! Record traversal - update all entities with the frame count
        void traverse(vsg::RecordTraversal& record) const override
        {
            auto viewID = record.getCommandBuffer()->viewID;
            auto frame = record.getFrameStamp()->frameCount;

            auto [lock, registry] = _registry.read();
            for (auto& entity : entities)
            {
                auto& v = registry.get<Visibility>(entity);
                v.frame[viewID] = frame;
            }
        }

    private:

        ecs::Registry _registry;
    };
}
