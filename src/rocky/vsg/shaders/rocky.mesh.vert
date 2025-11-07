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
    uint featureMask; // see defines below
    int padding[2];
};
#define MASK_HAS_TEXTURE 1
#define MASK_HAS_LIGHTING 2
#define MASK_HAS_PER_VERTEX_COLORS 4

layout(set = 0, binding = 1) uniform MeshUniform {
    MeshStyle style;
} mesh;

layout(location = 1) out Varyings {
    vec4 color;
    vec2 uv;
    vec3 normal;
    vec3 vertexView;
    float applyTexture;
    float applyLighting;
} vary;

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
    bool hasPerVertexColors = (MASK_HAS_PER_VERTEX_COLORS & mesh.style.featureMask) != 0;
    bool hasTexture = (MASK_HAS_TEXTURE & mesh.style.featureMask) != 0;
    bool hasLighting = (MASK_HAS_LIGHTING & mesh.style.featureMask) != 0;

    vary.color = hasPerVertexColors ? in_color : mesh.style.color;
    vary.applyTexture = hasTexture ? 1.0 : 0.0;
    vary.applyLighting = hasLighting ? 1.0 : 0.0;

    vec4 vv = pc.modelview * vec4(in_vertex, 1.0);
    vary.vertexView = vv.xyz / vv.w;

    float depthOffset = mesh.style.depthOffset;

    vary.uv = in_uv;

    mat3 normal_matrix = mat3(transpose(inverse(pc.modelview)));
    vary.normal = normal_matrix * in_normal;

    // TODO: lighting
    
    // Depth offset (view-space approach):
    vec4 view = pc.modelview * vec4(in_vertex, 1);
    view.xyz = apply_depth_offset(view.xyz, depthOffset);
    vec4 clip = pc.projection * view;

    gl_Position = clip;
}
