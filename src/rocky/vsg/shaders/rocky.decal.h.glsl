/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
 
// GLSL include file - decal subsystem
#include "rocky.defines.h.glsl"
#ifdef ROCKY_HAS_DECALS

#extension GL_EXT_nonuniform_qualifier : enable

#ifdef SSBOS_ARE_WRITABLE
#define DECALS_ACCESS
#else
#define DECALS_ACCESS readonly
#endif

struct DecalTile
{
    uint count;
    uint indices[MAX_DECALS_PER_TILE];
};

struct Decal
{
    mat4 mvm;
    mat4 mvmInverse; // computed in cull shader
    float a; // halfX extent (ortho) or zmin (persp)
    float b; // halfY extent (ortho) or zmax (persp)
    float c; // halfZ extent (ortho) or culling radius (persp)
    int textureIndex; // used by element 0 as total decal count
    float distance; // > 0 = persp
    float tanHalfFovY;
    float aspect;
    float _padding;
};

// SSBO containing the output tiles that pass cull (GPU only)
layout(std430, set = DESCRIPTOR_SET_VDS, binding = BINDING_VDS_DECAL_TILES) DECALS_ACCESS buffer DecalTiles
{
    DecalTile tile[];
}
b_decalTiles;

// SSBO containing the input decals to be culled
layout(std430, set = DESCRIPTOR_SET_GLOBAL, binding = BINDING_DECALS) DECALS_ACCESS buffer Decals
{
    Decal decal[];
}
b_decals;

layout(set = DESCRIPTOR_SET_GLOBAL, binding = BINDING_DECAL_TEXTURES) uniform sampler2D u_decalTextures[MAX_NUM_DECAL_TEXTURES];


void applyDecals(inout vec3 color, in vec3 vertexVs, in vec2 fragCoord)
{
    int index = frustumIndex(fragCoord);
    if (index < 0)
        return;

    //if (index >= decalTiles.length()) {
    //    color = vec3(1,0,0);
    //    return;
    //}

    DecalTile tile = b_decalTiles.tile[index];

    for(int i=0; i < tile.count; ++i)
    {
        Decal decal = b_decals.decal[tile.indices[i]];

        // tranform the view-space vertex into unit-decal-space
        vec3 local = (decal.mvmInverse * vec4(vertexVs, 1.0)).xyz;

        vec2 uv;
        bool inside = false;

        if (any(isnan(local)) || any(isinf(local))) {
            color.rgb = vec3(1, 0, 0);
            return;
        }

        if (decal.distance > 0.0) // perspective
        {
            // decal.a = zmin (far), decal.b = zmax (near)
            if (local.z >= decal.a && local.z <= decal.b)
            {
                float halfW = (decal.distance - local.z) * decal.tanHalfFovY * decal.aspect;
                float halfH = (decal.distance - local.z) * decal.tanHalfFovY;

                if (abs(local.x) <= halfW && abs(local.y) <= halfH)
                {
                    uv = vec2(local.x / halfW, local.y / halfH) * 0.5 + 0.5;
                    inside = true;
                }
            }
        }
        else // orthographic
        {
            vec3 bbox = vec3(0.5);
            if (abs(local.x) <= bbox.x && abs(local.y) <= bbox.y && abs(local.z) <= bbox.z)
            {
                //uv = (local.xy / bbox.xy + vec2(1.0)) * vec2(0.5);
                uv = local.xy + vec2(0.5);
                inside = true;
            }
        }

        if (inside)
        {
            int ti = decal.textureIndex;
            if (ti >= 0) 
            {
                //vec4 tex = texture(u_decalTextures[ti], uv);
                vec4 tex = texture(u_decalTextures[nonuniformEXT(ti)], uv);
                color.rgb = mix(color.rgb, tex.rgb, tex.a * (1.0 - u_debugTiles));
            }
            else
            {
                vec2 p = fract(uv * 10.0);
                float grid = step(0.05, p.x) * step(0.05, p.y);
                vec3 decalColor = vec3(fract(uv), 0.0) * grid;
                color.rgb = mix(color.rgb, decalColor, 0.75);
            }
        }
    }

    // debugging overlay to show tile density
    float ramp = clamp(float(tile.count) / 5.0, 0.0, 1.0);
    vec3 debugColor = vec3(0, ramp, ramp);
    color.rgb = mix(color.rgb, debugColor, clamp(float(tile.count), 0, 1) * u_debugTiles * 0.5);
}

#endif // ROCKY_HAS_DECALS
