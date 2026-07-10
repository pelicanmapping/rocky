/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma import_defines(ROCKY_HAS_FRUSTUM_GRID)
#ifdef ROCKY_HAS_FRUSTUM_GRID

#pragma include "rocky.defines.h.glsl"

#ifdef FRUSTUM_GRID_WRITES_ENABLED
#define READ_ONLY
#else
#define READ_ONLY readonly
#endif

struct Frustum
{
    vec4 planes[4];
};

layout(std140, set = VDS_DESCRIPTOR_SET_INDEX, binding = BINDING_VDS_FRUSTUM_GRID_PARAMS) uniform FrustumGridParams
{
    mat4 u_invProjMatrix;
    ivec4 u_viewport;
    ivec2 u_numTiles;
    uint u_pixelsPerTile;
    float u_debugTiles;
};

layout(std140, set = VDS_DESCRIPTOR_SET_INDEX, binding = BINDING_VDS_FRUSTUMS) READ_ONLY buffer Frustums
{
    Frustum frustums[];
};

int frustumIndex(in vec2 fragCoord)
{
    // u_viewport = (x, y, width, height)
    ivec2 pixel = ivec2(fragCoord) - u_viewport.xy;
    if (pixel.x < 0 || pixel.y < 0)
        return -1;

    ivec2 tileCoord = pixel / int(u_pixelsPerTile);
    if (tileCoord.x < 0 || tileCoord.y < 0 ||
        tileCoord.x >= u_numTiles.x || tileCoord.y >= u_numTiles.y)
        return -1;

    return tileCoord.y * u_numTiles.x + tileCoord.x;
}

bool intersectsFrustum(in Frustum f, in vec3 posVs, in float radius)
{
    for (int i = 0; i < 4; ++i)
    {
        if (dot(f.planes[i].xyz, posVs) - f.planes[i].w < -radius)
            return false;
    }
    return true;
}

vec3 frustumTileTestColor(in vec2 fragCoord, in vec3 posVs)
{
    int tile = frustumIndex(fragCoord);
    if (tile < 0)
        return vec3(1, 0, 1);

    Frustum f = frustums[tile];

    // fails if either the frustum or the intersection test is incorrect:
    if (!intersectsFrustum(f, posVs, 0.0))
        return vec3(1,0,0);

    float t = float(tile + 1);
    return vec3(fract(t * 0.1031), fract(t * 0.11369), fract(t * 0.13787));
}

#endif // ROCKY_USE_FRUSTUM_GRID_SYSTEM