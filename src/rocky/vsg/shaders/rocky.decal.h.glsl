/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
 
// GLSL include file - decal subsystem
#pragma import_defines(ROCKY_HAS_SLUGHORN)
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

#ifdef ROCKY_HAS_SLUGHORN
struct SlugLayer
{
    vec4 color;
    vec4 uvToEmX; // affine row: U coefficient, V coefficient, offset
    vec4 uvToEmY;
    vec4 bandTransform; // bandScaleX/Y, bandOffsetX/Y
    uvec4 shapeData; // bandTexX/Y, bandMaxX/Y
};
#endif

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

#ifdef ROCKY_HAS_SLUGHORN
// Slug shape metadata is separate from cullable logical decals, so an
// outline/core pair consumes one entry in DecalTile instead of two.
layout(std430, set = DESCRIPTOR_SET_VDS, binding = BINDING_VDS_SLUG_LAYERS) readonly buffer SlugLayers
{
    SlugLayer layer[];
}
b_slugLayers;

layout(set = DESCRIPTOR_SET_GLOBAL, binding = BINDING_SLUG_CURVE_TEXTURE) uniform sampler2D u_slugCurveTexture[MAX_NUM_DECAL_TEXTURES];
layout(set = DESCRIPTOR_SET_GLOBAL, binding = BINDING_SLUG_BAND_TEXTURE) uniform usampler2D u_slugBandTexture[MAX_NUM_DECAL_TEXTURES];

#pragma include "rocky.slug.glsl"

void applySlugLayer(
    inout vec3 color,
    in int textureIndex,
    in vec4 decalColor,
    in uint layerIndex,
    in vec2 uv,
    in vec2 localDx,
    in vec2 localDy,
    in int textureWidthLog2
    )
{
    // Address fields individually so the shader does not copy an entire
    // SlugLayer record through function-local storage.
    vec4 uvToEmX = b_slugLayers.layer[layerIndex].uvToEmX;
    vec4 uvToEmY = b_slugLayers.layer[layerIndex].uvToEmY;
    vec3 uvh = vec3(uv, 1.0);
    vec2 renderCoord = vec2(
        dot(uvh, uvToEmX.xyz),
        dot(uvh, uvToEmY.xyz));
    mat2 uvToEm = mat2(
        uvToEmX.xy,
        uvToEmY.xy);
    vec2 uvDx = localDx * uvToEm;
    vec2 uvDy = localDy * uvToEm;
    vec2 emsPerPixel = abs(uvDx) + abs(uvDy);
    float coverage = slug_Render(
        renderCoord,
        emsPerPixel,
        b_slugLayers.layer[layerIndex].bandTransform,
        ivec2(b_slugLayers.layer[layerIndex].shapeData.xy),
        ivec2(b_slugLayers.layer[layerIndex].shapeData.zw),
        u_slugCurveTexture[nonuniformEXT(textureIndex)],
        u_slugBandTexture[nonuniformEXT(textureIndex)],
        textureWidthLog2
        );

    vec4 layerColor = decalColor * b_slugLayers.layer[layerIndex].color;
    color.rgb = mix(
        color.rgb,
        layerColor.rgb,
        coverage * layerColor.a);
}
#endif

layout(set = DESCRIPTOR_SET_GLOBAL, binding = BINDING_DECAL_TEXTURES) uniform sampler2D u_decalTextures[MAX_NUM_DECAL_TEXTURES];

void applyDecals(
    inout vec3 color,
    in vec3 vertexVs,
#ifdef ROCKY_HAS_SLUGHORN
    in vec3 vertexVsDx,
    in vec3 vertexVsDy,
#endif
    in vec3 normalVs,
    in vec2 fragCoord)
{
    int index = frustumIndex(fragCoord);
    if (index < 0)
        return;

    uint tileCount = min(
        b_decalTiles.tile[index].count,
        uint(MAX_DECALS_PER_TILE));
    if (tileCount == 0u)
        return;

#ifdef ROCKY_HAS_SLUGHORN
    // Slug outlines from every logical decal must composite before any cores.
    // Non-Slug decals participate only in the regular (second) pass.
    for (int pass = 0; pass < 2; ++pass)
#else
    for (int pass = 1; pass < 2; ++pass)
#endif
    {
        for (uint i = 0u; i < tileCount; ++i)
        {
            uint packedIndex = b_decalTiles.tile[index].indices[i];
            if (pass == 0 &&
                (packedIndex & DECAL_TILE_HAS_OUTLINE_BIT) == 0u)
                continue;

            uint decalIndex = packedIndex & DECAL_TILE_INDEX_MASK;
            // Do not copy the complete 192-byte Decal record into function
            // storage. Load only the fields that survive each early reject.
            int payloadFlags = b_decals.decal[decalIndex].payloadFlags;
#ifdef ROCKY_HAS_SLUGHORN
            bool slug = (payloadFlags & DECAL_FLAG_SLUG) != 0;
#endif
            float distance = b_decals.decal[decalIndex].distance;

#ifdef ROCKY_HAS_SLUGHORN
            uint firstLayer = 0u;
            uint layerCount = 0u;
            int textureWidthLog2 = 0;
            if (slug)
            {
                // Slug currently supports orthographic projectors only. Resolve
                // and validate its layer range before doing any matrix work.
                if (distance > 0.0)
                    continue;

                uvec3 slugLayerRange =
                    b_decals.decal[decalIndex].slugLayerRange.xyz;
                firstLayer = slugLayerRange.x;
                uint outlineCount = min(
                    slugLayerRange.y,
                    slugLayerRange.z);
                layerCount = pass == 0 ?
                    outlineCount : slugLayerRange.z - outlineCount;
                firstLayer += pass == 0 ? 0u : outlineCount;

                uint available = uint(b_slugLayers.layer.length());
                if (firstLayer >= available)
                    continue;
                layerCount = min(layerCount, available - firstLayer);
                if (layerCount == 0u)
                    continue;

                textureWidthLog2 =
                    (payloadFlags >> DECAL_SLUG_TEXTURE_WIDTH_LOG2_SHIFT) &
                    DECAL_SLUG_TEXTURE_WIDTH_LOG2_MASK;
            }
            else if (pass == 0)
            {
                continue;
            }
#endif

            // transform the view-space vertex into unit-decal-space
            mat4 mvmInverse = b_decals.decal[decalIndex].mvmInverse;
            vec3 local = (mvmInverse * vec4(vertexVs, 1.0)).xyz;

            vec2 uv;
            bool inside = false;

            if (any(isnan(local)) || any(isinf(local))) {
                color.rgb = vec3(1, 0, 0);
                return;
            }

            if (distance > 0.0) // perspective
            {
                // decal.a = -far, decal.b = -near (projector looks down local -Z)
                float zMin = b_decals.decal[decalIndex].zMin;
                float zMax = b_decals.decal[decalIndex].zMax;
                if (local.z >= zMin && local.z <= zMax)
                {
                    float depth = -local.z; // positive forward distance from projector
                    float tanHalfFovY =
                        b_decals.decal[decalIndex].tanHalfFovY;
                    float halfH = depth * tanHalfFovY;
                    float halfW = halfH *
                        b_decals.decal[decalIndex].aspect;

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

#ifdef ROCKY_HAS_SLUGHORN
            if (slug)
            {
                // The initial Slug path supports orthographic projectors. Since
                // uv = local.xy + 0.5, transforming the screen-space view
                // derivatives as vectors gives the same value as fwidth(uv).
                vec2 localDx = (mvmInverse * vec4(vertexVsDx, 0.0)).xy;
                vec2 localDy = (mvmInverse * vec4(vertexVsDy, 0.0)).xy;
                int textureIndex =
                    b_decals.decal[decalIndex].textureIndex;
                vec4 decalColor = b_decals.decal[decalIndex].color;
                for (uint j = 0u; j < layerCount; ++j)
                {
                    applySlugLayer(
                        color,
                        textureIndex,
                        decalColor,
                        firstLayer + j,
                        uv,
                        localDx,
                        localDy,
                        textureWidthLog2
                    );
                }
            }
            else
#endif
            if (pass == 1)
            {
                // bit 0: upper-left texture origin
                // bit 1: premultiplied alpha
                if ((payloadFlags & DECAL_FLAG_UPPER_LEFT_TEXTURE_ORIGIN) != 0)
                {
                    uv.y = 1.0 - uv.y;
                }

                int ti = b_decals.decal[decalIndex].textureIndex;
                vec4 decalColor = b_decals.decal[decalIndex].color;
                if (ti >= 0)
                {
                    vec4 tex = texture(u_decalTextures[nonuniformEXT(ti)], uv);
                    if ((payloadFlags & DECAL_FLAG_PREMULTIPLIED_ALPHA) != 0)
                    {
                        // Modulate premultiplied content without multiplying source alpha twice.
                        float alpha = tex.a * decalColor.a;
                        vec3 premultiplied = tex.rgb * decalColor.rgb * decalColor.a;
                        color.rgb = color.rgb * (1.0 - alpha) + premultiplied;
                    }
                    else
                    {
                        color.rgb = mix(color.rgb, tex.rgb * decalColor.rgb, tex.a * decalColor.a);
                    }
                }
                else
                {
                    vec2 p = fract(uv * 10.0);
                    float grid = step(0.05, p.x) * step(0.05, p.y);
                    vec3 testPattern = vec3(fract(uv), 0.0) * grid;
                    color.rgb = mix(color.rgb, testPattern *
                        b_decals.decal[decalIndex].color.rgb,
                        b_decals.decal[decalIndex].color.a);
                }
            }
        }
    }

    // debugging overlay to show tile density
    float ramp = clamp(float(tileCount) / 5.0, 0.0, 1.0);
    vec3 debugColor = vec3(0, ramp, ramp);
    color.rgb = mix(color.rgb, debugColor, u_debugTiles * 0.75);
}

#endif // ROCKY_HAS_DECALS
