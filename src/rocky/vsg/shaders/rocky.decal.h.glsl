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
    mat4 mvmInverse;
    vec4 color; // modulation color
    int textureIndex; // used by element 0 as total decal count
    float distance; // > 0 = persp
    float zMin; // persp: -near
    float zMax; // persp: -far
    float cullingRadius; // persp
    float tanHalfFovY; // persp
    float aspect; // persp
    int payloadFlags;
    uvec4 slugLayerRange; // first layer, outline count, total count, reserved
};

struct SlugLayer
{
    vec4 color;
    vec4 uvToEmX; // affine row: U coefficient, V coefficient, offset
    vec4 uvToEmY;
    vec4 bandTransform; // bandScaleX/Y, bandOffsetX/Y
    uvec4 shapeData; // bandTexX/Y, bandMaxX/Y
};

// SSBO containing the output tiles that pass cull (GPU only)
layout(std430, set = DESCRIPTOR_SET_VDS, binding = BINDING_VDS_DECAL_TILES) DECALS_ACCESS buffer DecalTiles
{
    DecalTile tile[];
}
b_decalTiles;

// SSBO containing the input decals to be culled
layout(std430, set = DESCRIPTOR_SET_VDS, binding = BINDING_VDS_DECALS) DECALS_ACCESS buffer Decals
{
    Decal decal[];
}
b_decals;

// Slug shape metadata is separate from cullable logical decals, so an
// outline/core pair consumes one entry in DecalTile instead of two.
layout(std430, set = DESCRIPTOR_SET_VDS, binding = BINDING_VDS_SLUG_LAYERS) readonly buffer SlugLayers
{
    SlugLayer layer[];
}
b_slugLayers;

layout(set = DESCRIPTOR_SET_GLOBAL, binding = BINDING_DECAL_TEXTURES) uniform sampler2D u_decalTextures[MAX_NUM_DECAL_TEXTURES];
layout(set = DESCRIPTOR_SET_GLOBAL, binding = BINDING_SLUG_CURVE_TEXTURE) uniform sampler2D u_slugCurveTexture[MAX_NUM_DECAL_TEXTURES];
layout(set = DESCRIPTOR_SET_GLOBAL, binding = BINDING_SLUG_BAND_TEXTURE) uniform usampler2D u_slugBandTexture[MAX_NUM_DECAL_TEXTURES];

#pragma include "rocky.slug.glsl"

void applySlugLayer(
    inout vec3 color,
    in Decal decal,
    in SlugLayer layer,
    in vec2 uv,
    in vec2 localDx,
    in vec2 localDy)
{
    vec3 uvh = vec3(uv, 1.0);
    vec2 renderCoord = vec2(
        dot(uvh, layer.uvToEmX.xyz),
        dot(uvh, layer.uvToEmY.xyz));
    mat2 uvToEm = mat2(
        layer.uvToEmX.xy,
        layer.uvToEmY.xy);
    vec2 uvDx = localDx * uvToEm;
    vec2 uvDy = localDy * uvToEm;
    vec2 emsPerPixel = abs(uvDx) + abs(uvDy);
    int textureWidthLog2 =
        (decal.payloadFlags >> DECAL_SLUG_TEXTURE_WIDTH_LOG2_SHIFT) &
        DECAL_SLUG_TEXTURE_WIDTH_LOG2_MASK;

    float coverage = slug_Render(
        renderCoord,
        emsPerPixel,
        layer.bandTransform,
        ivec2(layer.shapeData.xy),
        ivec2(layer.shapeData.zw),
        decal.textureIndex,
        textureWidthLog2);

    vec4 layerColor = decal.color * layer.color;
    color.rgb = mix(
        color.rgb,
        layerColor.rgb,
        coverage * layerColor.a);
}

void applyDecals(
    inout vec3 color,
    in vec3 vertexVs,
    in vec3 vertexVsDx,
    in vec3 vertexVsDy,
    in vec3 normalVs,
    in vec2 fragCoord)
{
    int index = frustumIndex(fragCoord);
    if (index < 0)
        return;

    DecalTile tile = b_decalTiles.tile[index];

    // Slug outlines from every logical decal must composite before any cores.
    // Non-Slug decals participate only in the regular (second) pass.
    for (int pass = 0; pass < 2; ++pass)
    {
        for (int i = 0; i < tile.count; ++i)
        {
            Decal decal = b_decals.decal[tile.indices[i]];
            bool slug = (decal.payloadFlags & DECAL_FLAG_SLUG) != 0;
            if (pass == 0 && !slug)
                continue;

            // transform the view-space vertex into unit-decal-space
            vec3 local = (decal.mvmInverse * vec4(vertexVs, 1.0)).xyz;

            vec2 uv;
            bool inside = false;

            if (any(isnan(local)) || any(isinf(local))) {
                color.rgb = vec3(1, 0, 0);
                return;
            }

            if (decal.distance > 0.0) // perspective
            {
                // decal.a = -far, decal.b = -near (projector looks down local -Z)
                if (local.z >= decal.zMin && local.z <= decal.zMax)
                {
                    float depth = -local.z; // positive forward distance from projector
                    float halfW = depth * decal.tanHalfFovY * decal.aspect;
                    float halfH = depth * decal.tanHalfFovY;

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
                    uv = local.xy + vec2(0.5);
                    inside = true;
                }
            }

#if 0
            // Backface rejection relative to projector direction.
            if (inside)
            {
                vec3 N = normalize(normalVs);
                vec3 projDirVs; // projector -> fragment direction in view space

                if (decal.distance > 0.0) // perspective projector
                {
                    vec3 projectorPosVs = vec3(decal.mvm[3]);
                    projDirVs = normalize(vertexVs - projectorPosVs);
                }
                else // orthographic projector
                {
                    // projector forward is local -Z
                    projDirVs = normalize(-vec3(decal.mvm[2]));
                }

                // front-facing to projector means normal opposes projector->fragment direction
                if (dot(N, projDirVs) >= 0.0)
                {
                    inside = false;
                }
            }
#endif

            if (!inside)
                continue;

            if (slug)
            {
                // The initial Slug path supports orthographic projectors. Since
                // uv = local.xy + 0.5, transforming the screen-space view
                // derivatives as vectors gives the same value as fwidth(uv).
                if (decal.distance <= 0.0)
                {
                    vec2 localDx = (decal.mvmInverse * vec4(vertexVsDx, 0.0)).xy;
                    vec2 localDy = (decal.mvmInverse * vec4(vertexVsDy, 0.0)).xy;
                    uint firstLayer = decal.slugLayerRange.x;
                    uint outlineCount = min(
                        decal.slugLayerRange.y,
                        decal.slugLayerRange.z);
                    uint layerCount = pass == 0 ?
                        outlineCount : decal.slugLayerRange.z - outlineCount;
                    firstLayer += pass == 0 ? 0u : outlineCount;

                    uint available = uint(b_slugLayers.layer.length());
                    if (firstLayer < available)
                    {
                        layerCount = min(layerCount, available - firstLayer);
                        for (uint j = 0u; j < layerCount; ++j)
                        {
                            applySlugLayer(
                                color,
                                decal,
                                b_slugLayers.layer[firstLayer + j],
                                uv,
                                localDx,
                                localDy);
                        }
                    }
                }
            }
            else if (pass == 1)
            {
                // bit 0: upper-left texture origin
                // bit 1: premultiplied alpha
                if ((decal.payloadFlags & DECAL_FLAG_UPPER_LEFT_TEXTURE_ORIGIN) != 0)
                {
                    uv.y = 1.0 - uv.y;
                }

                int ti = decal.textureIndex;
                if (ti >= 0)
                {
                    vec4 tex = texture(u_decalTextures[nonuniformEXT(ti)], uv);
                    if ((decal.payloadFlags & DECAL_FLAG_PREMULTIPLIED_ALPHA) != 0)
                    {
                        // Modulate premultiplied content without multiplying source alpha twice.
                        float alpha = tex.a * decal.color.a;
                        vec3 premultiplied = tex.rgb * decal.color.rgb * decal.color.a;
                        color.rgb = color.rgb * (1.0 - alpha) + premultiplied;
                    }
                    else
                    {
                        color.rgb = mix(color.rgb, tex.rgb * decal.color.rgb, tex.a * decal.color.a);
                    }
                }
                else
                {
                    vec2 p = fract(uv * 10.0);
                    float grid = step(0.05, p.x) * step(0.05, p.y);
                    vec3 decalColor = vec3(fract(uv), 0.0) * grid;
                    color.rgb = mix(color.rgb, decalColor * decal.color.rgb, decal.color.a);
                }
            }
        }
    }

    // debugging overlay to show tile density
    float ramp = clamp(float(tile.count) / 5.0, 0.0, 1.0);
    vec3 debugColor = vec3(0, ramp, ramp);
    color.rgb = mix(color.rgb, debugColor, clamp(float(tile.count), 0, 1) * u_debugTiles * 0.75);
}

#endif // ROCKY_HAS_DECALS
