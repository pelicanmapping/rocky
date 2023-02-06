#version 450
#extension GL_NV_fragment_shader_barycentric : enable

// inputs

layout(location = 0) in vec3 in_color;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec3 up_vector_view;
layout(location = 3) in vec2 normalmap_coords;
layout(location = 4) in vec3 normalmap_binormal;

// uniforms

layout(set = 0, binding = 11) uniform sampler2D color_tex;
layout(set = 0, binding = 12) uniform sampler2D normal_tex;

// outputs

layout(location = 0) out vec4 out_color;

void main()
{
    vec4 texel = texture(color_tex, in_uv);
    out_color = mix(vec4(in_color,1), clamp(texel, 0, 1), texel.a);

    if (gl_FrontFacing == false)
        out_color.r = 1.0;
        
    // outlines
    float b = min(gl_BaryCoordNV.x, min(gl_BaryCoordNV.y, gl_BaryCoordNV.z))*32.0;
    out_color.rgb = mix(vec3(1,1,1), out_color.rgb, clamp(b,0,1));
}
