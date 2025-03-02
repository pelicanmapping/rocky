/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/Common.h>
#include <rocky/vsg/ecs/Registry.h>
#include <rocky/vsg/ecs/TransformData.h>

namespace ROCKY_NAMESPACE
{
    /**
    * ECS System that processes Transform and TransformData components.
    */
    class ROCKY_EXPORT TransformSystem : public vsg::Inherit<vsg::Node, TransformSystem>, public ecs::System
    {
    public:
        TransformSystem(ecs::Registry& r);

        //! Called periodically to update the transforms
        void traverse(vsg::RecordTraversal& record) const override
        {
            auto [lock, registry] = _registry.read();

            registry.view<TransformData>().each([&](auto& transformData)
                {
                    auto& view = transformData[record.getState()->_commandBuffer->viewID];
                    view.update(record);
                });
        }
    };
}
