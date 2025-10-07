#version 450
#pragma import_defines(ROCKY_ATMOSPHERE)

// vsg push constants
layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelview;
} pc;

// input vertex attributes
layout(location = 0) in vec3 in_vertex;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec4 in_color;
layout(location = 3) in vec2 in_uv;

// rocky::detail::MeshStyleRecord
struct MeshStyle {
    vec4 color;
    float depthOffset;
    int hasTexture;
    int hasLighting;
    int padding[1];
};

layout(set = 0, binding = 1) uniform MeshUniform {
    MeshStyle style;
} mesh;

layout(location = 1) out vec2 uv;
layout(location = 2) out vec3 normal;
layout(location = 3) out vec3 vertexView;
layout(location = 4) flat out vec4 color;
layout(location = 5) flat out int hasTexture;
layout(location = 6) flat out int hasLighting;

// GL built-ins
out gl_PerVertex {
    vec4 gl_Position;
};

// Moves the vertex closer to the camera by the specified bias,
// clamping it beyond the near clip plane if necessary.
vec3 apply_depth_offset(in vec3 vertex, in float offset)
{
    float n = -pc.projection[3][2] / (pc.projection[2][2] + 1.0); // near plane
    float t_n = (-n + 1.0) / -vertex.z; // [0..1] -> [n+1 .. vertex]
    if (t_n <= 0.0) return vertex; // already behind near plane
    float len = length(vertex);
    float t_offset = 1.0 - (offset/len);
    return vertex * max(t_n, t_offset);
}

void main()
{    
    color = mesh.style.color.a > 0.0 ? mesh.style.color : in_color;
    hasTexture = mesh.style.hasTexture;
    hasLighting = mesh.style.hasLighting;

    vec4 vv = pc.modelview * vec4(in_vertex, 1.0);
    vertexView = vv.xyz / vv.w;

    float depthOffset = mesh.style.depthOffset;

    uv = in_uv;

    mat3 normal_matrix = mat3(transpose(inverse(pc.modelview)));
    normal = normal_matrix * in_normal;

    // TODO: lighting
    
    // Depth offset (view-space approach):
    vec4 view = pc.modelview * vec4(in_vertex, 1);
    view.xyz = apply_depth_offset(view.xyz, depthOffset);
    vec4 clip = pc.projection * view;

    gl_Position = clip;
}
