/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma include "rocky.defines.h.glsl"

#ifdef SSBOS_ARE_WRITABLE
#define FRUSTUM_GRID_ACCESS
#else
#define FRUSTUM_GRID_ACCESS readonly
#endif

struct Frustum
{
    // Ortho:       (minX, maxX, minY, maxY)
    // Perspective: (minX/Z, maxX/Z, minY/Z, maxY/Z)
    vec4 bounds;
};

layout(std140, set = DESCRIPTOR_SET_VDS, binding = BINDING_VDS_FRUSTUM_GRID_PARAMS) uniform FrustumGridParams
{
    mat4 u_invProjMatrix;
    ivec4 u_viewport;
    ivec2 u_numTiles;
    uint u_pixelsPerTile;
    uint u_projIsOrtho;
    float u_debugTiles;
};

layout(std430, set = DESCRIPTOR_SET_VDS, binding = BINDING_VDS_FRUSTUMS) FRUSTUM_GRID_ACCESS buffer Frustums
{
    Frustum frustum[];
}
b_frustums;

int frustumIndex(in vec2 fragCoord)
{
    // u_viewport = (x, y, width, height)
    ivec2 pixel = ivec2(fragCoord) - u_viewport.xy;
    if (pixel.x < 0 || pixel.y < 0)
        return -1;

    // Rocky's grid size is a compile-time constant. Keeping it out of the UBO
    // division lets the fragment compiler strength-reduce this operation.
    ivec2 tileCoord = pixel / int(FRUSTUM_GRID_TILE_SIZE_PIXELS);
    if (tileCoord.x < 0 || tileCoord.y < 0 ||
        tileCoord.x >= u_numTiles.x || tileCoord.y >= u_numTiles.y)
        return -1;

    return tileCoord.y * u_numTiles.x + tileCoord.x;
}

bool intersectsFrustum(in Frustum f, in vec3 posVs, in float radius)
{
    if (u_projIsOrtho == 1u)
    {
        if (posVs.x < f.bounds.x - radius) return false;
        if (posVs.x > f.bounds.y + radius) return false;
        if (posVs.y < f.bounds.z - radius) return false;
        if (posVs.y > f.bounds.w + radius) return false;
        return true;
    }

    // Perspective bounds are stored as:
    // (minXOverNegZ, maxXOverNegZ, minYOverNegZ, maxYOverNegZ)
    float minXOverNegZ = f.bounds.x;
    float maxXOverNegZ = f.bounds.y;
    float minYOverNegZ = f.bounds.z;
    float maxYOverNegZ = f.bounds.w;

    // left:   x + minXOverNegZ * z >= 0
    float d0 = posVs.x + minXOverNegZ * posVs.z;
    if (d0 < -radius * sqrt(1.0 + minXOverNegZ * minXOverNegZ)) return false;

    // right: -x - maxXOverNegZ * z >= 0
    float d1 = -posVs.x - maxXOverNegZ * posVs.z;
    if (d1 < -radius * sqrt(1.0 + maxXOverNegZ * maxXOverNegZ)) return false;

    // bottom: y + minYOverNegZ * z >= 0
    float d2 = posVs.y + minYOverNegZ * posVs.z;
    if (d2 < -radius * sqrt(1.0 + minYOverNegZ * minYOverNegZ)) return false;

    // top:   -y - maxYOverNegZ * z >= 0
    float d3 = -posVs.y - maxYOverNegZ * posVs.z;
    if (d3 < -radius * sqrt(1.0 + maxYOverNegZ * maxYOverNegZ)) return false;

    return true;
}

vec3 frustumTileTestColor(in vec2 fragCoord, in vec3 posVs)
{
    int tile = frustumIndex(fragCoord);
    if (tile < 0)
        return vec3(1, 0, 1);

    Frustum f = b_frustums.frustum[tile];

    // fails if either the frustum or the intersection test is incorrect:
    if (!intersectsFrustum(f, posVs, 0.0))
        return vec3(1, 0, 0);

    float t = float(tile + 1);
    return vec3(fract(t * 0.1031), fract(t * 0.11369), fract(t * 0.13787));
}
