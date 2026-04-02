#version 450
#pragma import_defines(ROCKY_ATMOSPHERE)

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelview;
} pc;

// see rocky::TerrainTileDescriptors
layout(set = 0, binding = 13) uniform TileData {
    mat4 elevation_matrix;
    mat4 color_matrix;
    mat4 model_matrix;
    float min_height;
    float max_height;
    float span;
    float padding[1];
} tile;

// input vertex attributes
layout(location = 0) in vec3 in_vertex;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec3 in_uvw;

// inter-stage interface block
layout(location = 0) out Varyings {
    vec2 uv;
    vec3 normalView;
    vec3 upView;
    vec3 vertexView;
    vec2 elevation_uv;
} vary;

layout(set = 0, binding = 10) uniform sampler2D elevation_tex;

#if defined(ROCKY_ATMOSPHERE)
#include "rocky.atmo.ground.vert.glsl"
#endif

// GL built-ins
out gl_PerVertex {
    vec4 gl_Position;
};

// sample the elevation data at a UV tile coordinate
vec3 terrain_get_elevation(in vec2 uv)
{
    float size = float(textureSize(elevation_tex, 0).x);
    vec2 coeff = vec2((size - 1.0) / size, 0.5 / size);

    // Texel-level scale and bias allow us to sample the elevation texture
    // on texel center instead of edge.
    vec2 elevc = uv
        * coeff.x * tile.elevation_matrix[0][0] // scale
        + coeff.x * tile.elevation_matrix[3].st // bias
        + coeff.y;

    elevc = clamp(elevc, vec2(0.0), vec2(1.0)); // avoid sampling outside the texture

    float h = texture(elevation_tex, elevc).r;

    if (tile.max_height >= tile.min_height)
    {
        if (h == 1.0)
            h = 0.0; // replace no-data with zero elevation
        else
            h = h * (tile.max_height - tile.min_height) + tile.min_height; // R16_UNORM
    }

    return vec3(uv.s * tile.span, uv.t * tile.span, h);
}

vec3 compute_normal()
{
    vec2 texelSize = 1.0 / (textureSize(elevation_tex, 0) - vec2(1.0));

    vec3 p_east  = terrain_get_elevation(in_uvw.st + vec2( texelSize.x, 0.0));
    vec3 p_west  = terrain_get_elevation(in_uvw.st + vec2(-texelSize.x, 0.0));
    vec3 p_north = terrain_get_elevation(in_uvw.st + vec2(0.0,  texelSize.y));
    vec3 p_south = terrain_get_elevation(in_uvw.st + vec2(0.0, -texelSize.y));

    vec3 we = p_east - p_west;
    vec3 ns = p_north - p_south;
    return normalize(cross(we, ns));
}

void main()
{
    vec3 p_elevation = terrain_get_elevation(in_uvw.st);
    vec3 position = in_vertex + in_normal * p_elevation.z;
    vec4 position_view = pc.modelview * vec4(position, 1.0);

#if defined(ROCKY_ATMOSPHERE)
    atmos_vertex_main(position_view.xyz);
#endif
    
    vary.uv = (tile.color_matrix * vec4(in_uvw.st, 0, 1)).st;
    vary.vertexView = position_view.xyz / position_view.w;

    vary.elevation_uv = (tile.elevation_matrix * vec4(in_uvw.st, 0, 1)).st;

    mat3 normalMatrix = mat3(transpose(inverse(pc.modelview)));
    vary.upView = normalize(normalMatrix * compute_normal());
    vary.normalView = normalize(normalMatrix * compute_normal());
    
    gl_Position = pc.projection * position_view;
}
