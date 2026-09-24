/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#ifndef ROCKY_DECAL_RECORD
#define ROCKY_DECAL_RECORD

// Shared std430 record for decal culling, shading, and volume diagnostics.
// Keep synchronized with rocky::DecalGPU. This header deliberately needs no textures or Slug evaluator.
struct Decal
{
    mat4 mvm;
    mat4 mvmInverse;
    vec4 color;
    int textureIndex; // used by element 0 as total decal count
    float distance; // > 0 = perspective
    float zMin; // perspective: -far
    float zMax; // perspective: -near
    float cullingRadius;
    float tanHalfFovY;
    float aspect;
    int payloadFlags;
    uvec4 slugLayerRange; // first layer, outline count, total count, reserved
};

#endif
