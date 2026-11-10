#version 450
/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#extension GL_EXT_nonuniform_qualifier : enable
// Zero = generic coverage, one = segments, two = diagnostic original-order reference.
layout(constant_id = 0) const uint SLUG_PASS_MATERIAL = 0u;
#pragma import_defines(ROCKY_SLUG_DISTANCE_STROKES)
#pragma include "rocky.defines.h.glsl"
#pragma include "rocky.decal.record.h.glsl"

// Separate Slug pass: the main pass's depth gives the scene point under this pixel.
// It is rebuilt in view space (camera-relative) along the eye ray through the box surface, then
// mapped into the owning decal's unit volume exactly as applyDecals() maps terrain fragments.

struct SlugLayer
{
    vec4 color;
    vec4 uvToEmX;
    vec4 uvToEmY;
    vec4 bandTransform;
    uvec4 shapeData;
};
layout(std430, set = DESCRIPTOR_SET_LOCAL, binding = 1) readonly buffer SlugSegments {
    SlugLayer layer[];
} b_slugSegments[MAX_NUM_DECAL_TEXTURES];
struct PassSpan { uvec4 range; vec4 highlight; };
layout(std430, set = DESCRIPTOR_SET_LOCAL, binding = 2) readonly buffer PassSpans {
    PassSpan span[];
} b_slugSegmentDecals;
layout(std140, set = DESCRIPTOR_SET_LOCAL, binding = 3) uniform PassCamera {
    mat4 inverseProjection;
    vec4 viewport;
} u_passCamera;

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelview;
} pc;

layout(std430, set = DESCRIPTOR_SET_VDS, binding = BINDING_VDS_DECALS) readonly buffer Decals {
    Decal decal[];
} b_decals;

layout(set = DESCRIPTOR_SET_LOCAL, binding = 0) uniform sampler2D u_sceneDepth;

layout(set = DESCRIPTOR_SET_GLOBAL, binding = BINDING_SLUG_CURVE_TEXTURE)
uniform sampler2D u_slugCurveTexture[MAX_NUM_DECAL_TEXTURES];
layout(set = DESCRIPTOR_SET_GLOBAL, binding = BINDING_SLUG_BAND_TEXTURE)
uniform usampler2D u_slugBandTexture[MAX_NUM_DECAL_TEXTURES];

#pragma include "rocky.slug.glsl"
#pragma include "rocky.slug.path.glsl"
#define SLUG_PASS_ROAD_JOIN 2
#define SLUG_PASS_ROAD_CAP 1
#define SLUG_PASS_ROAD_MITER_LIMIT 4.0

layout(location = 0) flat in uint v_layer;
layout(location = 1) flat in uint v_decal;
layout(location = 2) flat in uint v_span;

struct DecalTile { uint count; uint indices[MAX_DECALS_PER_TILE]; };
layout(std430, set = DESCRIPTOR_SET_VDS, binding = BINDING_VDS_DECAL_TILES) readonly buffer DecalTiles {
    DecalTile tile[];
} b_decalTiles;

layout(location = 0) out vec4 out_color;

// Evaluates one generic layer with derivatives supplied before divergent receiver tests.
float passCoverage(SlugLayer record, vec2 coord, vec2 pixelSize, Decal decal)
{
    int payloadFlags = decal.payloadFlags;
    int textureIndex = decal.textureIndex;
    int textureWidthLog2 =
        (payloadFlags >> DECAL_SLUG_TEXTURE_WIDTH_LOG2_SHIFT) & DECAL_SLUG_TEXTURE_WIDTH_LOG2_MASK;

    ivec2 glyphLoc = ivec2(record.shapeData.xy);
    ivec2 bandMax = ivec2(record.shapeData.zw);

    // record.uvToEmX.w > 0 marks a distance-stroke layer and holds its half width (em).
    return
#ifdef ROCKY_SLUG_DISTANCE_STROKES
        record.uvToEmX.w > 0.0 ? slug_RenderStroke(
        coord,
        pixelSize,
        record.bandTransform,
        glyphLoc,
        bandMax,
        u_slugCurveTexture[nonuniformEXT(textureIndex)],
        u_slugBandTexture[nonuniformEXT(textureIndex)],
        textureWidthLog2,
        record.uvToEmX.w
        ) :
#endif
        slug_Render(
        coord,
        pixelSize,
        record.bandTransform,
        glyphLoc,
        bandMax,
        u_slugCurveTexture[nonuniformEXT(textureIndex)],
        u_slugBandTexture[nonuniformEXT(textureIndex)],
        textureWidthLog2
        );

}

// Evaluates coverage at the opaque terrain point reconstructed from this view's depth.
void main()
{
    float depth = texelFetch(u_sceneDepth, ivec2(gl_FragCoord.xy), 0).r;
    vec2 ndc = ((gl_FragCoord.xy - u_passCamera.viewport.xy) / u_passCamera.viewport.zw) * 2.0 - 1.0;
    vec4 reconstructed = u_passCamera.inverseProjection * vec4(ndc, depth, 1.0);
    vec3 viewPos = reconstructed.xyz / reconstructed.w;
    float viewZ = viewPos.z;

    mat4 mvmInverse = b_decals.decal[v_decal].mvmInverse;
    vec3 local = (mvmInverse * vec4(viewPos, 1.0)).xyz;
    vec2 uv = local.xy + 0.5;

    PassSpan span = b_slugSegmentDecals.span[v_span];
    SlugLayer record = b_slugSegments[nonuniformEXT(span.range.z)].layer[v_layer];
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
    bool segment = SLUG_PASS_MATERIAL == 2u ? uvToEmX.w < 0.0 : SLUG_PASS_MATERIAL == 1u;
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

    ivec2 pixel = ivec2(gl_FragCoord.xy - u_passCamera.viewport.xy);
    uvec2 tileXY = uvec2(pixel) / uint(FRUSTUM_GRID_TILE_SIZE_PIXELS);
    uint tilesX = (uint(u_passCamera.viewport.z) + FRUSTUM_GRID_TILE_SIZE_PIXELS - 1u) /
        FRUSTUM_GRID_TILE_SIZE_PIXELS;
    uint tileIndex = tileXY.y * tilesX + tileXY.x;
    bool candidate = false;
    if (tileIndex < uint(b_decalTiles.tile.length()))
        for (uint i = 0u; i < min(b_decalTiles.tile[tileIndex].count, uint(MAX_DECALS_PER_TILE)); ++i)
            candidate = candidate ||
                (b_decalTiles.tile[tileIndex].indices[i] & DECAL_TILE_INDEX_MASK) == v_decal;
    if (!candidate) discard;

    if (!(depth > 0.0) || !(viewZ < 0.0) || any(greaterThan(abs(local), vec3(0.5))))
        discard;

    // Match highlight modulation before multiplying the projector color.
    record.color.rgb = mix(record.color.rgb, span.highlight.rgb,
        clamp(span.highlight.a, 0.0, 1.0));
    vec4 layerColor = b_decals.decal[v_decal].color * record.color;

    if (segment)
    {
        if (!pathOwned)
            discard;
        float segmentCoverage = slug_PathCoverage(pathSample, pathRates, halfWidth);
        out_color = vec4(layerColor.rgb, segmentCoverage * layerColor.a);
        return;
    }

    float coverage = passCoverage(record, renderCoord, emsPerPixel, b_decals.decal[v_decal]);
    out_color = vec4(layerColor.rgb, coverage * layerColor.a);
}
