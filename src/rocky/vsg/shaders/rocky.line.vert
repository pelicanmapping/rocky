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
layout(set = 1, binding = 1) uniform VSG_Viewports {
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

#define DEPTH_OFFSET_TEST_OE 1

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


#ifdef DEPTH_OFFSET_TEST_OE // testing depth offset from OE
    vec4 curr_view = pc.modelview * vec4(in_vertex, 1);
    //vec4 prev_view = pc.modelview * vec3(in_vertex_prev, 1);
    //vec4 next_view = pc.modelview * vec3(in_vertex_next, 1);

    float range = length(curr_view.xyz);

    // extract params for clarity.
    float minBias = 100.0; // oe_DepthOffset_params[0];
    float maxBias = 10000.0; // oe_DepthOffset_params[1];
    float minRange = sqrt(line.depth_offset) * 19.0; // 1000.0; // oe_DepthOffset_params[2];
    float maxRange = 10000000.0; //  oe_DepthOffset_params[3];

    // calculate the depth offset bias for this range:
    float ratio = (clamp(range, minRange, maxRange) - minRange) / (maxRange - minRange);
    float bias = minBias + ratio * (maxBias - minBias);
    bias = min(bias, range * 0.5);
    bias = min(bias, maxBias);
    vec3 pullVec = normalize(curr_view.xyz);
    curr_view.xyz = curr_view.xyz - pullVec * bias;

    vec4 curr_clip = pc.projection * curr_view;

#else

    vec4 curr_clip = pc.projection * pc.modelview * vec4(in_vertex, 1);

#endif
    vec4 prev_clip = pc.projection * pc.modelview * vec4(in_vertex_prev, 1);
    vec4 next_clip = pc.projection * pc.modelview * vec4(in_vertex_next, 1);

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

    // apply the depth offset
    curr_clip.z += line.depth_offset * curr_clip.w;

    gl_Position = curr_clip;
}
