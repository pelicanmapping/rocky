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

// Keep synchronized with rocky::SlugLayerGPU.
struct SlugLayer
{
    vec4 color;
    // affine row: U coefficient, V coefficient, offset; w = distance-stroke half width (em), negated
    // for a separate-pass segment layer, whose bandTransform holds its em endpoints (x0, y0, x1, y1)
    // and shapeData the float bits of its road's points before and after them (NaN at a road end)
    vec4 uvToEmX;
    vec4 uvToEmY; // w = owning decal index * 2 + outline bit, as a float (Slug pass only)
    vec4 bandTransform; // bandScaleX/Y, bandOffsetX/Y
    uvec4 shapeData; // bandTexX/Y, bandMaxX/Y
};

#endif
