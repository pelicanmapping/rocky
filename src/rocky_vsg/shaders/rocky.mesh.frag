#version 450
#extension GL_NV_fragment_shader_barycentric : enable

// inter-stage interface block
struct Varyings {
    vec4 color;
    float wireframe;
};
layout(location = 1) flat in Varyings rk;

// outputs
layout(location = 0) out vec4 out_color;


void main()
{
    out_color = rk.color;

    if (rk.wireframe > 0.0)
    {
        float b = min(gl_BaryCoordNV.x, min(gl_BaryCoordNV.y, gl_BaryCoordNV.z)) * rk.wireframe;
        out_color.rgb = mix(vec3(1, 1, 1), out_color.rgb, clamp(b, 0.85, 1.0));
    }
}