/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Math.h>

 // include the shader header file, which will work from both GLSL and C++
#include "shaders/rocky.defines.h.glsl"


namespace ROCKY_NAMESPACE
{
    struct RenderParamsGPU
    {
        glm::fmat4 viewMatrix;
        glm::fmat4 inverseViewMatrix;
        glm::fvec2 ellipsoidAxes;
        glm::uint32_t stereographic; // bool
        glm::float32_t renderDomain; // 0 = normal, 1 = overlay bake
    };
    //static_assert(sizeof(RenderParams) % 16 == 0, "RenderParams must be 16-byte aligned");

    struct FrustumGridParamsGPU
    {
        glm::fmat4 invProjMatrix;
        glm::ivec4 viewport = { 0, 0, 0, 0 };
        glm::uvec2 numTiles = { 1u, 1u };
        glm::uint32_t pixelsPerTile = 16u;
        glm::uint32_t projIsOrtho = 0;
        glm::float32_t debugTiles = 0.0f;
    };
    //static_assert(sizeof(FrustumGridParams) % 16 == 0, "FrustumGridParams must be 16-byte aligned");

    struct FrustumGPU
    {
        glm::fvec4 bounds;
    };
    static_assert(sizeof(FrustumGPU) % 16 == 0, "FrustumGPU must be 16-byte aligned");

    struct DecalGPU
    {
        glm::fmat4 mvm;
        glm::fmat4 mvmInverse; // GPU only
        glm::fvec4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
        union {
            std::int32_t textureIndex = -1; // negative means no texture
            std::int32_t count; // used by entry 0 as total decal count
        };
        glm::float32 distance = 0.0f; // > 0 = persp
        glm::float32 zMin = 1.0f;
        glm::float32 zMax = 10.0f;
        glm::float32 cullingRadius = 1.0f;
        glm::float32 tanHalfFovY = 0.0f;
        glm::float32 aspect = 1.0f;
        glm::int32_t _padding[1] = { 0 };
    };
    static_assert(sizeof(DecalGPU) % 16 == 0, "DecalGPU must be 16-byte aligned");

    struct DecalTileGPU
    {
        glm::uint32_t count = 0;
        glm::uint32_t indices[MAX_DECALS_PER_TILE];
    };
    static_assert(sizeof(DecalTileGPU) % 16 == 0, "DecalTileGPU must be 16-byte aligned");
}