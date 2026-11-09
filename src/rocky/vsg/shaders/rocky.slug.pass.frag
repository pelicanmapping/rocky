#version 450
/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#extension GL_EXT_nonuniform_qualifier : enable
#pragma include "rocky.defines.h.glsl"
#pragma include "rocky.decal.record.h.glsl"

// Separate Slug pass: the main pass's depth gives the scene point under this pixel.
// It is rebuilt in view space (camera-relative) along the eye ray through the box surface, then
// mapped into the owning decal's unit volume exactly as applyDecals() maps terrain fragments.

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelview;
} pc;

layout(std430, set = DESCRIPTOR_SET_VDS, binding = BINDING_VDS_DECALS) readonly buffer Decals {
    Decal decal[];
} b_decals;

layout(std430, set = DESCRIPTOR_SET_VDS, binding = BINDING_VDS_SLUG_LAYERS) readonly buffer SlugLayers {
    SlugLayer layer[];
} b_slugLayers;

layout(set = DESCRIPTOR_SET_LOCAL, binding = 0) uniform sampler2D u_sceneDepth;

layout(set = DESCRIPTOR_SET_GLOBAL, binding = BINDING_SLUG_CURVE_TEXTURE) uniform sampler2D u_slugCurveTexture[MAX_NUM_DECAL_TEXTURES];
layout(set = DESCRIPTOR_SET_GLOBAL, binding = BINDING_SLUG_BAND_TEXTURE) uniform usampler2D u_slugBandTexture[MAX_NUM_DECAL_TEXTURES];

#pragma include "rocky.slug.glsl"
#pragma include "rocky.slug.path.glsl"

// Resident segment layers (see rocky.slug.pass.vert).
layout(std430, set = DESCRIPTOR_SET_LOCAL, binding = 1) readonly buffer SlugSegments {
    SlugLayer layer[];
} b_slugSegments;

layout(location = 0) flat in uint v_layer;
layout(location = 1) flat in uint v_decal;
layout(location = 2) in vec3 v_viewPos;
layout(location = 3) flat in uint v_segment;

layout(location = 0) out vec4 out_color;

void main()
{
    // Perspective depth: ndc.z = (P[2][2] * z + P[3][2]) / -z, for forward or reversed Z.
    float depth = texelFetch(u_sceneDepth, ivec2(gl_FragCoord.xy), 0).r;
    float viewZ = -pc.projection[3][2] / (depth + pc.projection[2][2]);
    vec3 viewPos = v_viewPos * (viewZ / v_viewPos.z);

    mat4 mvmInverse = b_decals.decal[v_decal].mvmInverse;
    vec3 local = (mvmInverse * vec4(viewPos, 1.0)).xyz;
    vec2 uv = local.xy + 0.5;

    SlugLayer record = v_segment != 0u ? b_slugSegments.layer[v_layer] : b_slugLayers.layer[v_layer];
    vec4 uvToEmX = record.uvToEmX;
    vec4 uvToEmY = record.uvToEmY;
    vec3 uvh = vec3(uv, 1.0);
    vec2 renderCoord = vec2(dot(uvh, uvToEmX.xyz), dot(uvh, uvToEmY.xyz));

    // Derivatives come before any discard so every pixel of the quad still runs them.
    vec2 emsPerPixel = fwidth(renderCoord);

    vec4 bandTransform = record.bandTransform;

    // A segment layer (negative half width; rocky.slug.path.glsl): its ends' inside distances and
    // across distance, with their screen rates, also before the discard. At a joint that isn't
    // extended, the two segments' quads meet on the joint's bisector, which also splits the joint's
    // pixels between them: the box a pixel rasterized through need not hold its scene point, so the
    // point's own side of the bisector decides. Past the limit the two overlap, as in osgSlug.
    bool segment = uvToEmX.w < 0.0;
    float halfWidth = -uvToEmX.w;
    slug_PathSample pathSample = slug_PathSample(0.0, 0.0, 0.0);
    vec3 pathRates = vec3(0.0);
    bool pathOwned = true;
    if (segment)
    {
        vec2 a = bandTransform.xy;
        vec2 b = bandTransform.zw;
        uvec4 neighborBits = record.shapeData;
        vec2 prev = uintBitsToFloat(neighborBits.xy);
        vec2 next = uintBitsToFloat(neighborBits.zw);
        bool hasPrev = !isnan(prev.x);
        bool hasNext = !isnan(next.x);

        slug_PathJoint startJoint = hasPrev ?
            slug_PathJointAt(prev, a, b, halfWidth, SLUG_PASS_ROAD_JOIN, SLUG_PASS_ROAD_MITER_LIMIT) :
            SLUG_PATH_NO_JOINT;
        slug_PathJoint endJoint = hasNext ?
            slug_PathJointAt(a, b, next, halfWidth, SLUG_PASS_ROAD_JOIN, SLUG_PASS_ROAD_MITER_LIMIT) :
            SLUG_PATH_NO_JOINT;

        pathSample = slug_PathSampleAt(
            renderCoord,
            a,
            b,
            hasPrev,
            slug_PathEndJoin(hasPrev, startJoint, SLUG_PASS_ROAD_CAP),
            hasNext,
            slug_PathEndJoin(hasNext, endJoint, SLUG_PASS_ROAD_CAP),
            halfWidth,
            SLUG_PASS_ROAD_CAP);
        pathRates = vec3(fwidth(pathSample.startIn), fwidth(pathSample.endIn), fwidth(pathSample.across));

        if (hasPrev && !startJoint.extended)
            pathOwned = pathOwned && dot(renderCoord - a, slug_PathJointAxis(prev, a, b)) >= 0.0;
        if (hasNext && !endJoint.extended)
            pathOwned = pathOwned && dot(renderCoord - b, slug_PathJointAxis(a, b, next)) < 0.0;
    }

    if (!(viewZ < 0.0) || any(greaterThan(abs(local), vec3(0.5))))
        discard;

    vec4 layerColor = b_decals.decal[v_decal].color * record.color;

    if (segment)
    {
        if (!pathOwned)
            discard;
        float segmentCoverage = slug_PathCoverage(pathSample, pathRates, halfWidth);
        out_color = vec4(layerColor.rgb, segmentCoverage * layerColor.a);
        return;
    }

    int payloadFlags = b_decals.decal[v_decal].payloadFlags;
    int textureIndex = b_decals.decal[v_decal].textureIndex;
    int textureWidthLog2 =
        (payloadFlags >> DECAL_SLUG_TEXTURE_WIDTH_LOG2_SHIFT) & DECAL_SLUG_TEXTURE_WIDTH_LOG2_MASK;

    ivec2 glyphLoc = ivec2(record.shapeData.xy);
    ivec2 bandMax = ivec2(record.shapeData.zw);

    // uvToEmX.w > 0 marks a distance-stroke layer and holds its half width (em).
    float coverage = uvToEmX.w > 0.0 ? slug_RenderStroke(
        renderCoord,
        emsPerPixel,
        bandTransform,
        glyphLoc,
        bandMax,
        u_slugCurveTexture[nonuniformEXT(textureIndex)],
        u_slugBandTexture[nonuniformEXT(textureIndex)],
        textureWidthLog2,
        uvToEmX.w
        ) : slug_Render(
        renderCoord,
        emsPerPixel,
        bandTransform,
        glyphLoc,
        bandMax,
        u_slugCurveTexture[nonuniformEXT(textureIndex)],
        u_slugBandTexture[nonuniformEXT(textureIndex)],
        textureWidthLog2
        );

    out_color = vec4(layerColor.rgb, coverage * layerColor.a);
}
