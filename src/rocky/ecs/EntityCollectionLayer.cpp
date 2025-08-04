/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#include "EntityCollectionLayer.h"
#include "Visibility.h"

using namespace ROCKY_NAMESPACE;

EntityCollectionLayer::EntityCollectionLayer(Registry registry) :
    super(),
    _registry(registry)
{
    construct({});
}

EntityCollectionLayer::EntityCollectionLayer(Registry registry, std::string_view JSON) :
    super(JSON),
    _registry(registry)
{
    construct({});
}

void
EntityCollectionLayer::construct(std::string_view JSON)
{
    setLayerTypeName("EntityCollectionLayer");
}

Result<> EntityCollectionLayer::openImplementation(const IOOptions& io)
{
    auto s = super::openImplementation(io);
    if (s.failed())
        return s.error();

    auto [lock, r] = _registry.write();

    for (auto& e : entities)
    {
        r.emplace_or_replace<ActiveState>(e);
    }

    return s;
}

void EntityCollectionLayer::closeImplementation()
{
    auto [lock, r] = _registry.write();
    for (auto& e : entities)
    {
        r.remove<ActiveState>(e);
    }

    super::closeImplementation();
}