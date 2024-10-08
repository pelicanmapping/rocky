#version 450
#pragma import_defines(USE_MESH_TEXTURE)

layout(location = 1) in vec2 uv;

// inter-stage interface block
struct Varyings {
    vec4 color;
};
layout(location = 2) flat in Varyings vary;

// outputs
layout(location = 0) out vec4 out_color;

// unis
#ifdef USE_MESH_TEXTURE
layout(binding=6) uniform sampler2D mesh_texture;
#endif


void main()
{
    out_color = vary.color;

#ifdef USE_MESH_TEXTURE
    out_color.rgb *= texture(mesh_texture, uv).rgb;
#endif
}
