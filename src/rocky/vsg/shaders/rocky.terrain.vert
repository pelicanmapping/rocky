#version 450
/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma import_defines(ROCKY_ATMOSPHERE)

#pragma include "rocky.defines.h.glsl"
#pragma include "rocky.viewdependentstate.glsl"
#pragma include "rocky.lighting.glsl"
#pragma include "rocky.projection.glsl"

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelview;
} pc;

// input vertex attributes
layout(location = 0) in vec3 in_vertex_ts;
layout(location = 1) in vec3 in_up_ts;
layout(location = 2) in vec3 in_uvw;

layout(set = 0, binding = BINDING_TERRAIN_ELEVATION) uniform sampler2D u_elevationTex;

// uniforms (TerrainState.h)
layout(set = 0, binding = BINDING_TERRAIN_SETTINGS) uniform TerrainSettings {
    vec4 backgroundColor;
    float atmosphere;
    float lighting;
    float debugTriangles;
    float debugNormals;
} u_terrain;

// rocky::TerrainTileDescriptors
layout(set = 0, binding = BINDING_TERRAIN_TILE) uniform TileData {
    mat4 elevationMatrix;
    mat4 colorMatrix;
    mat4 modelMatrix;
    float minHeight;
    float maxHeight;
    float span;
    float padding[1];
} u_tile;

// inter-stage interface block
layout(location = 0) out Varyings {
    vec2 uv;
    vec3 normalVs;
    vec3 vertexVs;
    vec3 lookWs;
    vec3 cameraWs;
    vec3 sunDirEcef;
    float discardVert;
} vary;

// GL built-ins
out gl_PerVertex {
    vec4 gl_Position;
};


// in_uvw.w marker bits:
#define VERTEX_VISIBLE       1 // draw it
#define VERTEX_BOUNDARY      2 // vertex lies on a skirt boundary
#define VERTEX_HAS_ELEVATION 4 // not subject to elevation texture
#define VERTEX_SKIRT         8 // it's a skirt vertex (bitmask)
#define VERTEX_CONSTRAINT   16 // part of a non-morphable constraint


// sample the elevation data at a UV tile coordinate and make a tangent-space position
vec3 computePointTs(in vec2 uv)
{
    float size = float(textureSize(u_elevationTex, 0).x);
    if (size <= 1.0)
        return vec3(uv.s * u_tile.span, uv.t * u_tile.span, 0.0); // no elevation data, return flat plane

    vec2 coeff = vec2((size - 1.0) / size, 0.5 / size);

    // Texel-level scale and bias allow us to sample the elevation texture
    // on texel center instead of edge.
    vec2 elevc = uv
        * coeff.x * u_tile.elevationMatrix[0][0] // scale
        + coeff.x * u_tile.elevationMatrix[3].st // bias
        + coeff.y;

    elevc = clamp(elevc, vec2(0.0), vec2(1.0)); // avoid sampling outside the texture

    float h = texture(u_elevationTex, elevc).r;

    if (u_tile.maxHeight >= u_tile.minHeight)
    {
        if (h == 1.0)
            h = 0.0; // replace no-data with zero elevation
        else
            h = h * (u_tile.maxHeight - u_tile.minHeight) + u_tile.minHeight; // R16_UNORM
    }

    return vec3(uv.s * u_tile.span, uv.t * u_tile.span, h);
}

vec3 computeNormal_ts(in vec2 uv)
{
    ivec2 size = textureSize(u_elevationTex, 0);

    // cannot calc a valid normal with a 1x1 texture
    if (size.x <= 1 || size.y <= 1)
        return vec3(0,0,1);

    vec2 texelSize = 1.0 / (vec2(size) - vec2(1.0));

    vec3 p_east  = computePointTs(uv + vec2( texelSize.x, 0.0));
    vec3 p_west  = computePointTs(uv + vec2(-texelSize.x, 0.0));
    vec3 p_north = computePointTs(uv + vec2(0.0,  texelSize.y));
    vec3 p_south = computePointTs(uv + vec2(0.0, -texelSize.y));

    vec3 we = p_east - p_west;
    vec3 ns = p_north - p_south;
    return normalize(cross(we, ns));
}

mat3 computeTBN_ts(in vec3 n)
{
    vec3 t = normalize(cross(vec3(0,1,0), n));
    vec3 b = normalize(cross(n, t));
    return mat3(t, b, n);
}

void main()
{
    vec3 point = computePointTs(in_uvw.st);
    vec3 position_ts = in_vertex_ts + in_up_ts * point.z;
    vec4 position_vs = pc.modelview * vec4(position_ts, 1.0);

    mat3 normalMatrix = mat3(transpose(inverse(pc.modelview)));

    vary.uv = (u_tile.colorMatrix * vec4(in_uvw.st, 0, 1)).st;
    vary.normalVs = normalize(normalMatrix * computeTBN_ts(in_up_ts) * computeNormal_ts(in_uvw.st));

    // Rotation from view space to world space
    mat3 rotate_vs_to_ws = getRotateVsToWs(pc.modelview);

    // Sun direction in ECEF
    vary.sunDirEcef = rotate_vs_to_ws * (-getSunlightDirection());

    // in an ortho projection, discard SKIRT verts.
    vary.discardVert = pc.projection[3][3] > 0.0 && (int(in_uvw.z) & VERTEX_SKIRT) != 0 ? 1.0 : 0.0;

    // apply an optional screen projection
    position_vs = applyProjection(position_vs, pc.projection);

    vary.vertexVs = position_vs.xyz / position_vs.w;

    // For lighting:
    vec3 camera_ts = -transpose(mat3(pc.modelview)) * pc.modelview[3].xyz;
    vary.cameraWs = (u_vds.inverseViewMatrix * pc.modelview * vec4(camera_ts, 1.0)).xyz;
    vary.lookWs = rotate_vs_to_ws * normalize(vary.vertexVs);

    gl_Position = pc.projection * position_vs;
}
