#version 450
/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma import_defines(ROCKY_HAS_ATMOSPHERE)

#pragma include "rocky.viewdependentstate.glsl"
#pragma include "rocky.projection.glsl"
#pragma include "rocky.depthoffset.glsl"

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
    uint featureMask; // 0x1 = color, 0x2 = width, 0x4 = horizon clipping
    float devicePixelRatio;
    uint padding[3];
};

#define PER_VERTEX_COLOR 0x1
#define PER_VERTEX_WIDTH 0x2
#define HORIZON_CLIPPING 0x4

layout(set = 0, binding = 1) uniform PointUniform {
    PointStyle style;
} u_point;

layout(location = 1) out Varyings {
    flat vec4 color;
    flat float antialias;
} vary;

// GL built-ins
out gl_PerVertex {
    vec4 gl_Position;
    float gl_PointSize;
    float gl_ClipDistance[1];
};

void main()
{    
    bool perVertexColor = (u_point.style.featureMask & PER_VERTEX_COLOR) != 0;
    bool perVertexWidth = (u_point.style.featureMask & PER_VERTEX_WIDTH) != 0;

    vary.color = perVertexColor ? in_color : u_point.style.color;
    vary.antialias = u_point.style.antialias;
    
    vec4 view = pc.modelview * vec4(in_vertex, 1);
    // Clip the point center before projection adjustments and visual depth offsets.
    gl_ClipDistance[0] = (u_point.style.featureMask & HORIZON_CLIPPING) != 0 ?
        dot(u_renderParams.horizonPlane, view) : 1.0;
    view = applyProjection(view, pc.projection);
    view = applyDepthOffset(view, u_point.style.depthOffset, pc.projection);

    gl_PointSize = (perVertexWidth ? in_width : u_point.style.width) * u_point.style.devicePixelRatio;
    gl_Position = pc.projection * view;
}
