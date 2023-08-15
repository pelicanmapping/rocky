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
    float depth_offset;
} mesh;

// input vertex attributes
layout(location = 0) in vec3 in_vertex;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec4 in_color;
layout(location = 3) in vec2 in_uv;

// inter-stage interface block
struct Varyings {
    vec4 color;
    float wireframe;
};
layout(location = 1) out vec2 uv;
layout(location = 2) flat out Varyings vary;

// GL built-ins
out gl_PerVertex {
    vec4 gl_Position;
};

void main()
{
    vary.color = mesh.color.a > 0.0 ? mesh.color : in_color;
    vary.wireframe = mesh.wireframe;
    uv = in_uv;

    // TODO: lighting (optional)

    // Meters/view approach:
    //vec4 view = pc.modelview * vec4(in_vertex, 1);
    //vec3 look = -normalize(view.xyz);
    //view.xyz += look * mesh.depth_offset;
    //gl_Position = pc.projection * view;
    
    // Depth/clip approach:
    vec4 clip = pc.projection * pc.modelview * vec4(in_vertex, 1);
    clip.z += mesh.depth_offset * clip.w;
    gl_Position = clip;
}
