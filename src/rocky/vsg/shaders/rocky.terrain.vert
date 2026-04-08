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
layout(location = 0) in vec3 in_vertex;
layout(location = 1) in vec3 in_upVector_TS;
layout(location = 2) in vec3 in_uvw;

// inter-stage interface block
layout(location = 0) out Varyings {
    vec2 uv;
    vec3 normal_VS;
    vec3 vertex_VS;
} vary;

layout(set = 0, binding = 10) uniform sampler2D elevationTex;

#if defined(ROCKY_ATMOSPHERE)
#include "rocky.atmo.ground.vert.glsl"
#endif

// GL built-ins
out gl_PerVertex {
    vec4 gl_Position;
};

// sample the elevation data at a UV tile coordinate and make a tangent-space position
vec3 compute_point_TS(in vec2 uv)
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

vec3 compute_normal_TS(in vec2 uv)
{
    ivec2 size = textureSize(elevationTex, 0);

    // cannot calc a valid normal with a 1x1 texture
    if (size.x <= 1 || size.y <= 1) return in_upVector_TS;

    vec2 texelSize = 1.0 / (vec2(size) - vec2(1.0));

    vec3 p_east  = compute_point_TS(uv + vec2( texelSize.x, 0.0));
    vec3 p_west  = compute_point_TS(uv + vec2(-texelSize.x, 0.0));
    vec3 p_north = compute_point_TS(uv + vec2(0.0,  texelSize.y));
    vec3 p_south = compute_point_TS(uv + vec2(0.0, -texelSize.y));

    vec3 we = p_east - p_west;
    vec3 ns = p_north - p_south;
    return normalize(cross(we, ns));
}

mat3 compute_tbn_TS(in vec3 n)
{
    vec3 t = normalize(cross(vec3(0,1,0), n));
    vec3 b = normalize(cross(n, t));
    return mat3(t, b, n);
}

void main()
{
    vec3 point = compute_point_TS(in_uvw.st);
    vec3 position_TS = in_vertex + in_upVector_TS * point.z;
    vec4 position_VS = pc.modelview * vec4(position_TS, 1.0);

#if defined(ROCKY_ATMOSPHERE)
    atmos_vertex_main(position_VS.xyz);
#endif
    
    mat3 normalMatrix = mat3(transpose(inverse(pc.modelview)));

    vary.uv = (tile.colorMatrix * vec4(in_uvw.st, 0, 1)).st;
    vary.vertex_VS = position_VS.xyz / position_VS.w;
    vary.normal_VS = normalize(normalMatrix * compute_tbn_TS(in_upVector_TS) * compute_normal_TS(in_uvw.st));
    
    gl_Position = pc.projection * position_VS;
}
