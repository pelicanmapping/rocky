/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "RenderTextureParticipant.h"
#include <rocky/ecs/Transform.h>

using namespace ROCKY_NAMESPACE;

void ROCKY_NAMESPACE::expandRenderTextureSourcePoint(
    entt::registry& registry,
    entt::entity source,
    RenderTextureBounds& bounds,
    const SRS& inputSRS,
    const glm::dvec3& input,
    const SRS& worldSRS,
    bool applySourceTransform)
{
    auto* transform = applySourceTransform ? registry.try_get<Transform>(source) : nullptr;
    if (!transform || !transform->position.valid() || !worldSRS.valid())
    {
        bounds.expand(inputSRS, input);
        return;
    }

    glm::dvec3 renderPoint = input;
    if (inputSRS.valid())
    {
        auto point = GeoPoint(inputSRS, input.x, input.y, input.z).transform(worldSRS);
        if (!point.valid())
            return;
        renderPoint = { point.x, point.y, point.z };
    }

    auto position = transform->position.transform(worldSRS);
    if (!position.valid())
        return;

    glm::dmat4 matrix = transform->topocentric ?
        worldSRS.topocentricToWorldMatrix({ position.x, position.y, position.z }) :
        glm::translate(glm::dmat4(1.0), glm::dvec3(position.x, position.y, position.z));
    matrix *= transform->localMatrix;

    auto transformed = matrix * glm::dvec4(renderPoint, 1.0);
    if (transformed.w != 0.0)
        renderPoint = glm::dvec3(transformed) / transformed.w;
    else
        renderPoint = glm::dvec3(transformed);

    bounds.expand(worldSRS, renderPoint);
}
