/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/VisibleLayer.h>
#include <rocky/ecs/Registry.h>

namespace ROCKY_NAMESPACE
{
    /**
    * Layer that houses a collection of ECS entities.
    */
    class ROCKY_EXPORT EntityCollectionLayer : public Inherit<VisibleLayer, EntityCollectionLayer>
    {
    public:
        //! Entities managed by this layer
        std::vector<entt::entity> entities;

    public:
        //! Construct a new layer
        //! @param registry ECS Registry
        EntityCollectionLayer(Registry registry);

        //! Construct a new layer from serialized JSON
        //! @param registry ECS Registry
        EntityCollectionLayer(Registry registry, std::string_view JSON);

    protected:

        Result<> openImplementation(const IOOptions& io) override;

        void closeImplementation() override;

    protected:

        Registry _registry;

    private:

        void construct(std::string_view JSON);
    };
}
