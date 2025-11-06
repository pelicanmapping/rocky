#version 450
#pragma import_defines(ROCKY_ATMOSPHERE)

// vsg push constants
layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelview;
} pc;

// input vertex attributes
layout(location = 0) in vec3 in_vertex;
layout(location = 1) in vec4 in_color;
layout(location = 2) in float in_width;

// rocky::detail::PointStyleRecord
struct PointStyle {
    vec4 color;
    float width;
    float antialias;
    float depthOffset;
    uint perVertexMask; // 0x1 = color, 0x2 = width
};

#define PER_VERTEX_COLOR 0x1
#define PER_VERTEX_WIDTH 0x2

layout(set = 0, binding = 1) uniform PointUniform {
    PointStyle style;
} point;

layout(location = 1) out Varyings {
    flat vec4 color;
    flat float antialias;
} vary;

// GL built-ins
out gl_PerVertex {
    vec4 gl_Position;
    float gl_PointSize;
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
    bool perVertexColor = (point.style.perVertexMask & PER_VERTEX_COLOR) != 0;
    bool perVertexWidth = (point.style.perVertexMask & PER_VERTEX_WIDTH) != 0;

    vary.color = perVertexColor ? in_color : point.style.color;
    vary.antialias = point.style.antialias;

    float depthOffset = point.style.depthOffset;
    
    // Depth offset (view-space approach):
    vec4 view = pc.modelview * vec4(in_vertex, 1);
    view.xyz = apply_depth_offset(view.xyz, depthOffset);
    vec4 clip = pc.projection * view;

    gl_PointSize = perVertexWidth ? in_width : point.style.width;
    gl_Position = clip;
}
