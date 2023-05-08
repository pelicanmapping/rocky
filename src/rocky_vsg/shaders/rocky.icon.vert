#version 450

// vsg push constants
layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelview;
} pc;

// rocky::IconStyle
layout(set = 0, binding = 1) uniform IconStyle {
    float size;
    float padding[3];
} icon;

// vsg viewport data
layout(set = 1, binding = 1) uniform ViewportData {
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

    vec2 signs = vec2(
        gl_VertexIndex == 0 || gl_VertexIndex == 3 || gl_VertexIndex == 5 ? -1 : 1,
        gl_VertexIndex == 0 || gl_VertexIndex == 1 || gl_VertexIndex == 3 ? -1 : 1);

    vec2 viewport_size = vsg_viewports.viewport[0].zw;
    vec2 pixel_size = 2.0 / viewport_size;
    vec2 dims_clip = icon.size * pixel_size * signs;

    clip.xy += (dims_clip * clip.w) * 0.5;

    uv = vec2(signs.x + 1.0, -signs.y + 1.0) * 0.5;

    gl_Position = clip;
}
