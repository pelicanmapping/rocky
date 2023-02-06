#version 450

#if 0
#include "terrain.sdk"
#else
layout(set = 0, binding = 10) uniform sampler2D elevation_tex;

layout(set = 0, binding = 13) uniform TileData {
    mat4 elevation_matrix;
    mat4 color_matrix;
    mat4 normal_matrix;
    vec2 elevTexelCoeff;
} tile;
#endif

layout(push_constant) uniform PushConstants
{
    mat4 projection;
    mat4 modelview;
} pc;

layout(location = 0) in vec3 in_vertex;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec3 in_uvw;
layout(location = 3) in vec3 in_neighbor_vertex;
layout(location = 4) in vec3 in_neighbor_normal;

layout(location = 0) out vec3 out_color;
layout(location = 1) out vec2 out_uv;
layout(location = 2) out vec3 up_vector_view;
layout(location = 3) out vec2 normalmap_coords;
layout(location = 4) out vec3 normalmap_binormal;

out gl_PerVertex {
    vec4 gl_Position;
};

const float elev_tile_size = 257;
const float elev_tile_bias = 0.5;
const vec2 elev_tile_coeff = vec2(
    (elev_tile_size - (2.0*elev_tile_bias)) / elev_tile_size,
    elev_tile_bias / elev_tile_size);

// Sample the elevation data at a UV tile coordinate
float terrain_get_elevation(in vec2 uv)
{
    // Texel-level scale and bias allow us to sample the elevation texture
    // on texel center instead of edge.
    vec2 elevc = uv
        * elev_tile_coeff.x * tile.elevation_matrix[0][0] // scale
        + elev_tile_coeff.x * tile.elevation_matrix[3].st // bias
        + elev_tile_coeff.y;

    return texture(elevation_tex, elevc).r;
}

void main()
{
    float elevation = terrain_get_elevation(in_uvw.st);

    vec3 position = in_vertex + in_normal*elevation;
    gl_Position = (pc.projection * pc.modelview) * vec4(position, 1.0);

    mat3 normal_matrix = mat3(transpose(inverse(pc.modelview)));
    
    out_color = vec3(1);
    out_uv = (tile.color_matrix * vec4(in_uvw.st, 0, 1)).st;

    // The normal map stuff
    up_vector_view = normal_matrix * in_normal;
    normalmap_coords = in_uvw.st;
    normalmap_binormal = normalize(normal_matrix * vec3(0.0, 1.0, 0.0));
}
