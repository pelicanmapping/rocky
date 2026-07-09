#version 450
/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */

// vsg push constants
layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelview;
} pc;

// input vertex attributes
layout(location = 0) in vec3 in_vertex;
layout(location = 1) in vec3 in_vertexPrev;
layout(location = 2) in vec3 in_vertexNext;
layout(location = 3) in vec4 in_color;

// rocky::detail::LineStyleRecord
struct LineStyle {
    vec4 color;
    float width;
    int stipplePattern;
    int stippleFactor;
    float depthOffset;
    uint perVertexMask; // 0x1 = color
    float devicePixelRatio;
    uint padding[2];
};

// rocky::detail::LineStyleUniform
layout(set = 0, binding = 1) uniform LineStyleUniform {
    LineStyle style;
} u_line;

// vsg viewport data
layout(set = 1, binding = 1) readonly buffer VSG_Viewports {
    vec4 viewport[1]; // x, y, width, height
} vsg_viewports;

// inter-stage interface block
struct Varyings {
    vec4 color;
    vec2 stippleDir;
    int stipplePattern;
    int stippleFactor;
};

layout(location = 1) out float lateral;
layout(location = 2) flat out Varyings vary;

// GL built-ins
out gl_PerVertex {
    vec4 gl_Position;
};

#pragma include "rocky.viewdependentstate.glsl"
#pragma include "rocky.projection.glsl"
#pragma include "rocky.depthoffset.glsl"

void main()
{
    bool perVertexColor = (u_line.style.perVertexMask & 0x1) != 0;

    vary.color = perVertexColor ? in_color : u_line.style.color;
    vary.stipplePattern = u_line.style.stipplePattern;
    vary.stippleFactor = u_line.style.stippleFactor;

    float thickness = max(0.5, floor(u_line.style.width * u_line.style.devicePixelRatio));
    float len = thickness;
    int code = (gl_VertexIndex + 2) & 3;
    bool is_start = code <= 1;
    bool is_right = code == 0 || code == 2;
    lateral = is_right ? -1.0 : 1.0;

#if 0
    // Enforce the start/end limits on the line:
    if (u_line.first >= 0 && u_line.last >= 0)
    {
        if (gl_VertexIndex < u_line.first || (u_line.last > 0 && gl_VertexIndex > u_line.last))
        {
            // no draw
            lateral = 0.0;
            return;
        }
    }
#endif

    vec2 viewport_size = vsg_viewports.viewport[0].zw;

    float bias = u_line.style.depthOffset;

    vec4 curr_view = pc.modelview * vec4(in_vertex, 1);
    curr_view      = applyProjection(curr_view, pc.projection);
    curr_view      = applyDepthOffset(curr_view, bias, pc.projection);
    vec4 curr_clip = pc.projection * curr_view;

    vec4 prev_view = pc.modelview * vec4(in_vertexPrev, 1);
    prev_view      = applyProjection(prev_view, pc.projection);
    prev_view      = applyDepthOffset(prev_view, bias, pc.projection);
    vec4 prev_clip = pc.projection * prev_view;

    vec4 next_view = pc.modelview * vec4(in_vertexNext, 1);
    next_view      = applyProjection(next_view, pc.projection);
    next_view      = applyDepthOffset(next_view, bias, pc.projection);
    vec4 next_clip = pc.projection * next_view;

    vec2 curr_pixel = (curr_clip.xy / curr_clip.w) * viewport_size;
    vec2 prev_pixel = (prev_clip.xy / prev_clip.w) * viewport_size;
    vec2 next_pixel = (next_clip.xy / next_clip.w) * viewport_size;

    vec2 dir;

    // The following vertex comparisons must be done in model 
    // space because the equivalency gets mashed after projection.
    const float EPSILON = 1e-7;

    // starting point uses (next - current)
    if (distance(in_vertex, in_vertexPrev) < EPSILON)
    {
        dir = normalize(next_pixel - curr_pixel);
        vary.stippleDir = dir;
    }

    // ending point uses (current - previous)
    else if (distance(in_vertex, in_vertexNext) < EPSILON)
    {
        dir = normalize(curr_pixel - prev_pixel);
        vary.stippleDir = dir;
    }

    else
    {
        vec2 dir_in = normalize(curr_pixel - prev_pixel);
        vec2 dir_out = normalize(next_pixel - curr_pixel);

        if (dot(dir_in, dir_out) < -0.999999)
        {
            dir = is_start ? dir_out : dir_in;
        }
        else
        {
            vec2 tangent = normalize(dir_in + dir_out);
            vec2 perp = vec2(-dir_in.y, dir_in.x);
            vec2 miter = vec2(-tangent.y, tangent.x);
            dir = tangent;
            len = thickness / dot(miter, perp);

            // limit the length of a mitered corner, to prevent unsightly spikes
            const float LIMIT = 2.0;
            if (len > thickness * LIMIT)
            {
                len = thickness;
                dir = is_start ? dir_out : dir_in;
            }
        }
        vary.stippleDir = dir_out;
    }

    // calculate the extrusion vector in pixels
    // note: seems like it should be len/2, BUT remember we are in [-w..w] space
    vec2 extrude_pixel = vec2(-dir.y, dir.x) * len;

    // and convert to unit space:
    vec2 extrude_unit = extrude_pixel / viewport_size;

    // calculate the offset in clip space and apply it.
    vec2 offset = extrude_unit * lateral;
    curr_clip.xy += (offset * curr_clip.w);

    if (u_line.style.stipplePattern != 0xffff)
    {
        const float QUANTIZE = 8.0;

        // Calculate the (quantized) rotation angle that will project the
        // fragment coord onto the X-axis for stipple pattern sampling.
        // Note: this relies on the GLSL "provoking vertex" being at the 
        // beginning of the line segment!

        const float R2D = 57.29577951;
        const float D2R = 1.0 / R2D;
        int a = int(R2D * (atan(vary.stippleDir.y, vary.stippleDir.x)) + 180.0);
        int q = int(360.0 / QUANTIZE);
        int r = a % q;
        int qa = (r > q / 2) ? a + q - r : a - r;
        float qangle = D2R * (float(qa) - 180.0);
        vary.stippleDir = vec2(cos(qangle), sin(qangle));
    }

    gl_Position = curr_clip;
}
