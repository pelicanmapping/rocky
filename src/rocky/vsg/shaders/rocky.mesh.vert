#version 450
#pragma import_defines(USE_MESH_STYLE)

// vsg push constants
layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelview;
} pc;

#ifdef USE_MESH_STYLE
// see rocky::MeshStyle
layout(set = 0, binding = 1) uniform MeshData {
    vec4 color;
    float depthoffset;
} mesh;
#endif

// input vertex attributes
layout(location = 0) in vec3 in_vertex;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec4 in_color;
layout(location = 3) in vec2 in_uv;
layout(location = 4) in float in_depthoffset;

// inter-stage interface block
struct Varyings {
    vec4 color;
};
layout(location = 1) out vec2 uv;
layout(location = 2) flat out Varyings vary;

// GL built-ins
out gl_PerVertex {
    vec4 gl_Position;
};

void main()
{
    float depthoffset = in_depthoffset;

#ifdef USE_MESH_STYLE
    vary.color = mesh.color.a > 0.0 ? mesh.color : in_color;
    if (mesh.depthoffset != 0.0)
        depthoffset = mesh.depthoffset;
#else
    vary.color = in_color;
#endif

    uv = in_uv;

    // TODO: lighting
    
    // Depth offset (view-space approach):
    vec4 view = pc.modelview * vec4(in_vertex, 1);
    view.xyz -= normalize(view.xyz) * depthoffset;
    vec4 clip = pc.projection * view;

    gl_Position = clip;
}
