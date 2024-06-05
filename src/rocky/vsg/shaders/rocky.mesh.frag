#version 450
#extension GL_NV_fragment_shader_barycentric : enable
#pragma import_defines(USE_MESH_TEXTURE)

layout(location = 1) in vec2 uv;

// inter-stage interface block
struct Varyings {
    vec4 color;
    float wireframe;
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

#ifdef GL_NV_fragment_shader_barycentric
    if (vary.wireframe > 0.0)
    {
        float b = min(gl_BaryCoordNV.x, min(gl_BaryCoordNV.y, gl_BaryCoordNV.z)) * vary.wireframe;
        out_color.rgb = mix(vec3(1, 1, 1), out_color.rgb, clamp(b, 0.85, 1.0));
    }
#endif
}
