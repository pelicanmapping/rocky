#version 450

// vsg push constants
layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelview;
} pc;

// see rocky::MeshStyleRecord
struct MeshStyle {
    vec4 color;
    float depthOffset;
    int textureIndex;
};

layout(set = 0, binding = 0) readonly buffer MeshStyles {
    MeshStyle lut[1024];
} styles;

layout(set = 0, binding = 1) uniform MeshUniforms {
    int style;
    int padding[3];
} mesh;

// input vertex attributes
layout(location = 0) in vec3 in_vertex;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec4 in_color;
layout(location = 3) in vec2 in_uv;

// inter-stage interface block
struct Varyings {
    vec4 color;
    int textureIndex;
};
layout(location = 1) out vec2 uv;
layout(location = 2) flat out Varyings vary;

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
    vec4 style_color = styles.lut[mesh.style].color;
    vary.color = style_color.a > 0.0 ? style_color : in_color;
    float depthOffset = styles.lut[mesh.style].depthOffset;

    uv = in_uv;

    // TODO: lighting
    
    // Depth offset (view-space approach):
    vec4 view = pc.modelview * vec4(in_vertex, 1);
    view.xyz = apply_depth_offset(view.xyz, depthOffset);
    vec4 clip = pc.projection * view;

    gl_Position = clip;
}
