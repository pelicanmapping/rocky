#version 450
#pragma import_defines(ROCKY_ATMOSPHERE)

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelview;
} pc;

// see rocky::TerrainTileDescriptors
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
    vec3 vertex_ws;
    vec3 camera_ws;
    vec3 sun_ws;
} vary;

layout(set = 0, binding = 10) uniform sampler2D elevationTex;

// GL built-ins
out gl_PerVertex {
    vec4 gl_Position;
};

#include "rocky.lighting.frag.glsl"


// sample the elevation data at a UV tile coordinate and make a tangent-space position
vec3 compute_point_ts(in vec2 uv)
{
    float size = float(textureSize(elevationTex, 0).x);
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
        return in_up_ts;

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

    // ECEF positions for aerial perspective computation in fragment shader.
    // position_ts is tile-local; tile.modelMatrix transforms to world/ECEF.
    vary.vertex_ws = (tile.modelMatrix * vec4(position_ts, 1.0)).xyz;

    // Camera position in tile-local space, then transformed to ECEF.
    // (push constants are vertex-stage only, so we must do this here)
    mat3 rot = transpose(mat3(pc.modelview));
    vec3 camera_ts = rot * (-pc.modelview[3].xyz);
    vary.camera_ws = (tile.modelMatrix * vec4(camera_ts, 1.0)).xyz;

    // Sun direction in ECEF. vsg_lights stores positions in view space.
    // Transform: view-space -> tile-local -> world/ECEF (direction only, mat3).
    vary.sun_ws = rot * get_sun_position();


    gl_Position = pc.projection * position_vs;
}
