#version 450
#pragma import_defines(ROCKY_ATMOSPHERE)

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelview;
} pc;

layout(set = 0, binding = 10) uniform sampler2D elevationTex;

// rocky::TerrainTileDescriptors
layout(set = 0, binding = 13) uniform TileData {
    mat4 elevationMatrix;
    mat4 colorMatrix;
    mat4 modelMatrix;
    float minHeight;
    float maxHeight;
    float span;
    float padding[1];
} tile;

// input vertex attributes
layout(location = 0) in vec3 in_vertex_ts;
layout(location = 1) in vec3 in_up_ts;
layout(location = 2) in vec3 in_uvw;

// inter-stage interface block
layout(location = 0) out Varyings {
    vec2 uv;
    vec3 normal_vs;
    vec3 vertex_vs;
    vec3 viewdir_ecef;
    vec3 camera_ecef;
    vec3 sundir_ecef;
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

// for: get_sunlight_direction()
#include "rocky.lighting.glsl"


// sample the elevation data at a UV tile coordinate and make a tangent-space position
vec3 compute_point_ts(in vec2 uv)
{
    float size = float(textureSize(elevationTex, 0).x);
    if (size <= 1.0)
        return vec3(uv.s * tile.span, uv.t * tile.span, 0.0); // no elevation data, return flat plane

    vec2 coeff = vec2((size - 1.0) / size, 0.5 / size);

    // Texel-level scale and bias allow us to sample the elevation texture
    // on texel center instead of edge.
    vec2 elevc = uv
        * coeff.x * tile.elevationMatrix[0][0] // scale
        + coeff.x * tile.elevationMatrix[3].st // bias
        + coeff.y;

    elevc = clamp(elevc, vec2(0.0), vec2(1.0)); // avoid sampling outside the texture

    float h = texture(elevationTex, elevc).r;

    if (tile.maxHeight >= tile.minHeight)
    {
        if (h == 1.0)
            h = 0.0; // replace no-data with zero elevation
        else
            h = h * (tile.maxHeight - tile.minHeight) + tile.minHeight; // R16_UNORM
    }

    return vec3(uv.s * tile.span, uv.t * tile.span, h);
}

vec3 compute_normal_ts(in vec2 uv)
{
    ivec2 size = textureSize(elevationTex, 0);

    // cannot calc a valid normal with a 1x1 texture
    if (size.x <= 1 || size.y <= 1)
        return vec3(0,0,1);

    vec2 texelSize = 1.0 / (vec2(size) - vec2(1.0));

    vec3 p_east  = compute_point_ts(uv + vec2( texelSize.x, 0.0));
    vec3 p_west  = compute_point_ts(uv + vec2(-texelSize.x, 0.0));
    vec3 p_north = compute_point_ts(uv + vec2(0.0,  texelSize.y));
    vec3 p_south = compute_point_ts(uv + vec2(0.0, -texelSize.y));

    vec3 we = p_east - p_west;
    vec3 ns = p_north - p_south;
    return normalize(cross(we, ns));
}

mat3 compute_tbn_ts(in vec3 n)
{
    vec3 t = normalize(cross(vec3(0,1,0), n));
    vec3 b = normalize(cross(n, t));
    return mat3(t, b, n);
}

void main()
{
    vec3 point = compute_point_ts(in_uvw.st);
    vec3 position_ts = in_vertex_ts + in_up_ts * point.z;
    vec4 position_vs = pc.modelview * vec4(position_ts, 1.0);

    mat3 normalMatrix = mat3(transpose(inverse(pc.modelview)));

    vary.uv = (tile.colorMatrix * vec4(in_uvw.st, 0, 1)).st;
    vary.vertex_vs = position_vs.xyz / position_vs.w;
    vary.normal_vs = normalize(normalMatrix * compute_tbn_ts(in_up_ts) * compute_normal_ts(in_uvw.st));
    // Rotation from view space to ECEF
    mat3 view_to_ecef = mat3(tile.modelMatrix) * transpose(mat3(pc.modelview));

    // View direction in ECEF, computed from view space to avoid catastrophic cancellation
    vary.viewdir_ecef = view_to_ecef * normalize(vary.vertex_vs);

    // Camera ECEF position
    vec3 cam_ts = -transpose(mat3(pc.modelview)) * pc.modelview[3].xyz;
    vary.camera_ecef = (tile.modelMatrix * vec4(cam_ts, 1.0)).xyz;

    // Sun direction in ECEF
    vary.sundir_ecef = view_to_ecef * (-get_sunlight_direction());

    // in an ortho projection, dicard SKIRT verts.
    vary.discardVert =
        pc.projection[3][3] > 0.0 && (int(in_uvw.z) & VERTEX_SKIRT) != 0 ? 1.0 : 0.0;

    gl_Position = pc.projection * position_vs;
}
