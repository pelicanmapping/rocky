#version 450
/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma include "rocky.defines.h.glsl"
#pragma include "rocky.decal.record.h.glsl"
#pragma include "rocky.slug.path.glsl"

// Separate Slug pass: one instanced box per SlugLayer, drawn after the main pass.
// The box spans the layer's band rectangle (its shape's em bounds) inside its owning decal's
// receiving volume, so a pixel only evaluates the layers whose box covers it.

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

// VSG's per-view viewports (x, y, width, height), for the segment boxes' pixel margin.
layout(std430, set = DESCRIPTOR_SET_VDS, binding = BINDING_VDS_VSG_VIEWPORTS) readonly buffer Viewports {
    vec4 viewport[];
} b_viewports;

// Resident segment layers (uvToEmY.w = segment group * 2 + outline bit) and, per segment group, the
// decal placing that group's atlas this frame (0: not drawn).
layout(std430, set = DESCRIPTOR_SET_LOCAL, binding = 1) readonly buffer SlugSegments {
    SlugLayer layer[];
} b_slugSegments;

layout(std430, set = DESCRIPTOR_SET_LOCAL, binding = 2) readonly buffer SlugSegmentDecals {
    uint decal[];
} b_slugSegmentDecals;

layout(location = 0) flat out uint v_layer;
layout(location = 1) flat out uint v_decal;
layout(location = 2) out vec3 v_viewPos;
layout(location = 3) flat out uint v_segment;

// The box as one 14-vertex triangle strip: twelve outward-facing (counter-clockwise) triangles,
// two per face. Corner bit 0 = x, bit 1 = y, bit 2 = z.
const int corners[14] = int[](3, 2, 7, 6, 4, 2, 0, 3, 1, 7, 5, 4, 1, 0);

void main()
{
    // Each draw covers its own range of outlines or cores: per-frame SlugLayers instances, or
    // resident segment instances from SLUG_PASS_SEGMENT_INSTANCE_BASE on.
    bool segmentLayer = uint(gl_InstanceIndex) >= SLUG_PASS_SEGMENT_INSTANCE_BASE;
    uint layerIndex = uint(gl_InstanceIndex) - (segmentLayer ? SLUG_PASS_SEGMENT_INSTANCE_BASE : 0u);
    SlugLayer record = segmentLayer ? b_slugSegments.layer[layerIndex] : b_slugLayers.layer[layerIndex];
    uint code = uint(record.uvToEmY.w) >> 1;
    uint decalIndex = segmentLayer ? b_slugSegmentDecals.decal[code] : code;

    v_layer = layerIndex;
    v_decal = decalIndex;
    v_segment = segmentLayer ? 1u : 0u;
    v_viewPos = vec3(0.0);

    // A layer without a decal collapses to a degenerate point.
    if (decalIndex == 0u)
    {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        return;
    }

    // em = A * uv + offset (see applySlugLayer).
    vec4 uvToEmX = record.uvToEmX;
    vec4 uvToEmY = record.uvToEmY;
    mat2 emToUv = inverse(mat2(
        vec2(uvToEmX.x, uvToEmY.x),
        vec2(uvToEmX.y, uvToEmY.y)));
    vec2 emOffset = vec2(uvToEmX.z, uvToEmY.z);
    vec4 bandTransform = record.bandTransform;

    int corner = corners[gl_VertexIndex];
    vec3 p = vec3(float(corner & 1), float((corner >> 1) & 1), float((corner >> 2) & 1));
    mat4 mvm = b_decals.decal[decalIndex].mvm;

    if (uvToEmX.w < 0.0)
    {
        // A segment layer: its mitered quad (rocky.slug.path.glsl, as osgSlug's Sluggit builds it),
        // extruded through the receiving volume. Each corner is pushed outward by an AA margin of
        // 1.5 pixels, measured at the corner's own centerline point: view units per pixel there,
        // over view units per em (the em x axis's length through emToUv and mvm). Scaling the
        // corner's offset keeps a joint's shared corner on the same point from both segments.
        float halfWidth = -uvToEmX.w;
        vec2 a = bandTransform.xy;
        vec2 b = bandTransform.zw;
        uvec4 neighborBits = record.shapeData;
        vec2 prev = uintBitsToFloat(neighborBits.xy);
        vec2 next = uintBitsToFloat(neighborBits.zw);
        bool hasPrev = !isnan(prev.x);
        bool hasNext = !isnan(next.x);

        // A zero-length segment draws nothing; its neighbors meet in a round joint.
        if (slug_PathIsShort(b - a, halfWidth))
        {
            gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
            return;
        }

        slug_PathJoint startJoint = hasPrev ?
            slug_PathJointAt(prev, a, b, halfWidth, SLUG_PASS_ROAD_JOIN, SLUG_PASS_ROAD_MITER_LIMIT) :
            SLUG_PATH_NO_JOINT;
        slug_PathJoint endJoint = hasNext ?
            slug_PathJointAt(a, b, next, halfWidth, SLUG_PASS_ROAD_JOIN, SLUG_PASS_ROAD_MITER_LIMIT) :
            SLUG_PATH_NO_JOINT;

        vec2 s = vec2(p.x, p.y * 2.0 - 1.0);
        vec2 offset = slug_PathCornerOffset(a, b, hasPrev, startJoint, hasNext, endJoint, s, halfWidth, SLUG_PASS_ROAD_CAP);
        vec2 base = mix(a, b, s.x);

        vec4 baseView = mvm * vec4(emToUv * (base - emOffset) - 0.5, 0.0, 1.0);
        float viewPerPixel = 2.0 * abs(baseView.z) / (abs(pc.projection[1][1]) * b_viewports.viewport[0].w);
        float viewPerEm = length(mat3(mvm) * vec3(emToUv[0], 0.0));
        float margin = 1.5 * viewPerPixel / max(viewPerEm, 1e-30);

        vec2 em = base + offset * (1.0 + margin / halfWidth);
        // A butt cap's end also moves past its point (other ends already reach past it).
        if (SLUG_PASS_ROAD_CAP == SLUG_CAP_BUTT && !(s.x > 0.5 ? hasNext : hasPrev))
            em += normalize(b - a) * (s.x > 0.5 ? margin : -margin);

        vec3 local = vec3(emToUv * (em - emOffset) - 0.5, p.z - 0.5);
        vec4 viewPos = mvm * vec4(local, 1.0);
        v_viewPos = viewPos.xyz;
        gl_Position = pc.projection * viewPos;
        return;
    }

    // bandCoord = em * scale + offset spans [0, SLUG_INDIRECTION_SIZE] over the shape's bounds.
    vec2 emA = -bandTransform.zw / bandTransform.xy;
    vec2 emB = (vec2(float(SLUG_INDIRECTION_SIZE)) - bandTransform.zw) / bandTransform.xy;

    // Invert em = A * uv + offset at the four em corners.

    vec2 uv0 = emToUv * (vec2(emA.x, emA.y) - emOffset);
    vec2 uv1 = emToUv * (vec2(emB.x, emA.y) - emOffset);
    vec2 uv2 = emToUv * (vec2(emA.x, emB.y) - emOffset);
    vec2 uv3 = emToUv * (vec2(emB.x, emB.y) - emOffset);
    vec2 uvMin = clamp(min(min(uv0, uv1), min(uv2, uv3)), 0.0, 1.0);
    vec2 uvMax = clamp(max(max(uv0, uv1), max(uv2, uv3)), 0.0, 1.0);

    vec3 local = vec3(mix(uvMin, uvMax, p.xy) - 0.5, p.z - 0.5);

    // mvm already maps to this view, including terrain-fitted receiving depth.
    vec4 viewPos = mvm * vec4(local, 1.0);
    v_viewPos = viewPos.xyz;
    gl_Position = pc.projection * viewPos;
}
