#version 450

// vsg push constants
layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelview;
} pc;

// see rocky::MeshStyle
layout(set = 0, binding = 1) uniform MeshData {
    vec4 color;
    float wireframe;
} mesh;

// input vertex attributes
layout(location = 0) in vec3 in_vertex;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec4 in_color;

// inter-stage interface block
struct Varyings {
    vec4 color;
    float wireframe;
};
layout(location = 1) flat out Varyings rk;

// GL built-ins
out gl_PerVertex {
    vec4 gl_Position;
};

void main()
{
    rk.color = mesh.color.a > 0.0 ? mesh.color : in_color;
    rk.wireframe = mesh.wireframe;

    // TODO: lighting

    gl_Position = pc.projection * pc.modelview * vec4(in_vertex, 1);
}
