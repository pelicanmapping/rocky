#version 450

// vsg push constants
layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelview;
} pc;

// see rocky::LineStyle
layout(set = 0, binding = 1) uniform LineData {
    vec4 color;
    float width;
    int stipple_pattern;
    int stipple_factor;
    float resolution;
    float depth_offset;
} line;

// vsg viewport data
layout(set = 1, binding = 1) readonly buffer VSG_Viewports {
    vec4 viewport[1]; // x, y, width, height
} vsg_viewports;

// input vertex attributes
layout(location = 0) in vec3 in_vertex;
layout(location = 1) in vec3 in_vertex_prev;
layout(location = 2) in vec3 in_vertex_next;
layout(location = 3) in vec4 in_color;

// inter-stage interface block
struct Varyings {
    vec4 color;
    vec2 stipple_dir;
    int stipple_pattern;
    int stipple_factor;
};
layout(location = 0) out float lateral;
layout(location = 1) flat out Varyings rk;

// GL built-ins
out gl_PerVertex {
    vec4 gl_Position;
};

vec3 bias_and_clamp_to_near_z(in vec3 vertex, float bias, float nearz)
{
    float len = length(vertex);
    if (len - bias <= nearz)
        bias = len - nearz - 1.0;

    return vertex - (vertex/len) * bias;
}

void main()
{
    rk.color = line.color.a > 0.0 ? line.color : in_color;
    rk.stipple_pattern = line.stipple_pattern;
    rk.stipple_factor = line.stipple_factor;

    float thickness = max(0.5, floor(line.width));
    float len = thickness;
    int code = (gl_VertexIndex + 2) & 3;
    bool is_start = code <= 1;
    bool is_right = code == 0 || code == 2;
    lateral = is_right ? -1.0 : 1.0;

#if 0
    // Enforce the start/end limits on the line:
    if (line.first >= 0 && line.last >= 0)
    {
        if (gl_VertexIndex < line.first || (line.last > 0 && gl_VertexIndex > line.last))
        {
            // no draw
            lateral = 0.0;
            return;
        }
    }
#endif

    vec2 viewport_size = vsg_viewports.viewport[0].zw;

    float bias = line.depth_offset;
    float nearz = -pc.projection[3][2] / (pc.projection[2][2] + 1.0);


    //bias = (length(curr_view.xyz) - nearz - 1.0) * 0.5;

    vec4 curr_view = pc.modelview * vec4(in_vertex, 1);
    curr_view.xyz  = bias_and_clamp_to_near_z(curr_view.xyz, bias, nearz);
    //curr_view.xyz -= normalize(curr_view.xyz) * bias;
    vec4 curr_clip = pc.projection * curr_view;

    vec4 prev_view = pc.modelview * vec4(in_vertex_prev, 1);
    prev_view.xyz  = bias_and_clamp_to_near_z(prev_view.xyz, bias, nearz);
    //prev_view.xyz -= normalize(prev_view.xyz) * bias;
    vec4 prev_clip = pc.projection * prev_view;

    vec4 next_view = pc.modelview * vec4(in_vertex_next, 1);
    next_view.xyz  = bias_and_clamp_to_near_z(next_view.xyz, bias, nearz);
    //next_view.xyz -= normalize(next_view.xyz) * bias;
    vec4 next_clip = pc.projection * next_view;

    vec2 curr_pixel = (curr_clip.xy / curr_clip.w) * viewport_size;
    vec2 prev_pixel = (prev_clip.xy / prev_clip.w) * viewport_size;
    vec2 next_pixel = (next_clip.xy / next_clip.w) * viewport_size;

    vec2 dir;

    // The following vertex comparisons must be done in model 
    // space because the equivalency gets mashed after projection.

    // starting point uses (next - current)
    if (in_vertex == in_vertex_prev)
    {
        dir = normalize(next_pixel - curr_pixel);
        rk.stipple_dir = dir;
    }

    // ending point uses (current - previous)
    if (in_vertex == in_vertex_next)
    {
        dir = normalize(curr_pixel - prev_pixel);
        rk.stipple_dir = dir;
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
            const float limit = 2.0;
            if (len > thickness * limit)
            {
                len = thickness;
                dir = is_start ? dir_out : dir_in;
            }
        }
        rk.stipple_dir = dir_out;
    }

    // calculate the extrusion vector in pixels
    // note: seems like it should be len/2, BUT remember we are in [-w..w] space
    vec2 extrude_pixel = vec2(-dir.y, dir.x) * len;

    // and convert to unit space:
    vec2 extrude_unit = extrude_pixel / viewport_size;

    // calculate the offset in clip space and apply it.
    vec2 offset = extrude_unit * lateral;
    curr_clip.xy += (offset * curr_clip.w);

    if (line.stipple_pattern != 0xffff)
    {
        const float quantize = 8.0;

        // Calculate the (quantized) rotation angle that will project the
        // fragment coord onto the X-axis for stipple pattern sampling.
        // Note: this relies on the GLSL "provoking vertex" being at the 
        // beginning of the line segment!

        const float r2d = 57.29577951;
        const float d2r = 1.0 / r2d;
        int a = int(r2d * (atan(rk.stipple_dir.y, rk.stipple_dir.x)) + 180.0);
        int q = int(360.0 / quantize);
        int r = a % q;
        int qa = (r > q / 2) ? a + q - r : a - r;
        float qangle = d2r * (float(qa) - 180.0);
        rk.stipple_dir = vec2(cos(qangle), sin(qangle));
    }

    // apply a static clip-space offset for z-flight mitigation.
    const float clip_offset = 1e-7;
    curr_clip.z += clip_offset * curr_clip.w;

    gl_Position = curr_clip;
}
