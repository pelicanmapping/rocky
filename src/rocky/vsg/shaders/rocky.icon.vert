#version 450

// vsg push constants
layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelview;
} pc;

// rocky::IconStyle
layout(set = 0, binding = 1) uniform IconStyle {
    float size;
    float rotation;
    float padding[2];
} icon;

// vsg viewport data
layout(set = 1, binding = 1) buffer VSG_Viewports {
    vec4 viewport[1]; // x, y, width, height
} vsg_viewports;

// input vertex attributes
layout(location = 0) in vec3 in_vertex;

// output varyings
layout(location = 0) out vec2 uv;

// GL built-ins
out gl_PerVertex {
    vec4 gl_Position;
};

void main()
{
    vec4 clip = pc.projection * pc.modelview * vec4(0, 0, 0, 1);

    // extrude the vertex based on its index to form a clip-space billboard
    vec2 signs = vec2(
        gl_VertexIndex == 0 || gl_VertexIndex == 3 || gl_VertexIndex == 5 ? -1 : 1,
        gl_VertexIndex == 0 || gl_VertexIndex == 1 || gl_VertexIndex == 3 ? -1 : 1);

    vec2 viewport_size = vsg_viewports.viewport[0].zw;
    vec2 pixel_size = 2.0 / viewport_size;

    // scale and rotate:
    float sr = sin(icon.rotation), cr = cos(icon.rotation);
    vec2 offset = mat2(cr, sr, -sr, cr) * (icon.size * signs * 0.5);

    clip.xy += (offset * pixel_size * clip.w);

    uv = vec2(signs.x + 1.0, -signs.y + 1.0) * 0.5;

    gl_Position = clip;
}
