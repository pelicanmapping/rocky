#version 450

// inputs

layout(location = 0) in vec3 frag_color;
layout(location = 1) in vec2 frag_uv;
layout(location = 2) in vec3 oe_UpVectorView;
layout(location = 3) in vec2 oe_normalMapCoords;
layout(location = 4) in vec3 oe_normalMapBinormal;

// uniforms

layout(set = 0, binding = 11) uniform sampler2D color_tex;
layout(set = 0, binding = 12) uniform sampler2D normal_tex;

layout(set = 0, binding = 13) uniform TileData {
    mat4 elevation_matrix;
    mat4 color_matrix;
    mat4 normal_matrix;
    vec2 elevTexelCoeff;
} tile;

// outputs

layout(location = 0) out vec4 out_color;

void main()
{
    vec4 texel = texture(color_tex, frag_uv);
    out_color = clamp(texel, 0, 1);

    if (gl_FrontFacing == false)
        out_color.r = 1.0;
}
