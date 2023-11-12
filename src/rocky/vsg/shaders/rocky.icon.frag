#version 450
#extension GL_NV_fragment_shader_barycentric : enable

// uniforms
layout(set = 0, binding = 2) uniform sampler2D icon_texture;

// inputs
layout(location = 0) in vec2 uv;

// outputs
layout(location = 0) out vec4 out_color;

void main()
{
    out_color = texture(icon_texture, uv);
}
