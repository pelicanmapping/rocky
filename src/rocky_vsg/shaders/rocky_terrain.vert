#version 450

layout(set = 0, binding = 10) uniform sampler2D elevation_tex;

// see rocky::TerrainTileDescriptors
layout(set = 0, binding = 13) uniform TileData
{
    mat4 elevation_matrix;
    mat4 color_matrix;
    mat4 normal_matrix;
} tile;

layout(push_constant) uniform PushConstants
{
    mat4 projection;
    mat4 modelview;
} pc;

// input attributes
layout(location = 0) in vec3 in_vertex;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec3 in_uvw;
layout(location = 3) in vec3 in_neighbor_vertex;
layout(location = 4) in vec3 in_neighbor_normal;

// output varyings
layout(location = 0) out vec3 out_color;
layout(location = 1) out vec2 out_uv;
layout(location = 2) out vec3 out_upvector_view;
layout(location = 5) out vec3 out_vertex_view;

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

    mat3 normal_matrix = mat3(transpose(inverse(pc.modelview)));
    
    out_color = vec3(1);
    out_uv = (tile.color_matrix * vec4(in_uvw.st, 0, 1)).st;
    out_upvector_view = normal_matrix * in_normal;
    out_vertex_view = position_view.xyz / position_view.w;
    
    gl_Position = pc.projection * position_view;
}
