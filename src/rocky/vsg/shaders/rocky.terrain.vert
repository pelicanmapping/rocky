#version 450
#pragma import_defines(ROCKY_LIGHTING)
#pragma import_defines(ROCKY_ATMOSPHERE)

layout(set = 0, binding = 10) uniform sampler2D elevation_tex;

layout(push_constant) uniform PushConstants
{
    mat4 projection;
    mat4 modelview;
} pc;

// see rocky::TerrainTileDescriptors
layout(set = 0, binding = 13) uniform TileData
{
    mat4 elevation_matrix;
    mat4 color_matrix;
    mat4 normal_matrix;
    mat4 model_matrix;
} tile;

// input vertex attributes
layout(location = 0) in vec3 in_vertex;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec3 in_uvw;

// inter-stage interface block
struct RockyVaryings {
    vec4 color;
    vec2 uv;
    vec3 up_view;
    vec3 vertex_view;
};

// output varyings
layout(location = 0) out RockyVaryings varyings;

#if defined(ROCKY_ATMOSPHERE)
#include "rocky.atmo.ground.vert.glsl"
#endif

// GL built-ins
out gl_PerVertex {
    vec4 gl_Position;
};

// sample the elevation data at a UV tile coordinate
float terrain_get_elevation(in vec2 uv)
{
    float size = float(textureSize(elevation_tex, 0).x);
    vec2 coeff = vec2((size - 1.0) / size, 0.5 / size);

    // Texel-level scale and bias allow us to sample the elevation texture
    // on texel center instead of edge.
    vec2 elevc = uv
        * coeff.x * tile.elevation_matrix[0][0] // scale
        + coeff.x * tile.elevation_matrix[3].st // bias
        + coeff.y;

    return texture(elevation_tex, elevc).r;
}

void main()
{
    float elevation = terrain_get_elevation(in_uvw.st);
    vec3 position = in_vertex + in_normal*elevation;
    vec4 position_view = pc.modelview * vec4(position, 1.0);

#if defined(ROCKY_ATMOSPHERE)
    atmos_vertex_main(position_view.xyz);
#endif

    mat3 normal_matrix = mat3(transpose(inverse(pc.modelview)));
    varyings.up_view = normal_matrix * in_normal;
    
    varyings.color = vec4(0.5); // placeholder
    varyings.uv = (tile.color_matrix * vec4(in_uvw.st, 0, 1)).st;
    varyings.vertex_view = position_view.xyz / position_view.w;
    
    gl_Position = pc.projection * position_view;
}
