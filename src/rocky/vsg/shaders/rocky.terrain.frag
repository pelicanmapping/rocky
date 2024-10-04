#version 450

#pragma import_defines(RK_LIGHTING)
#pragma import_defines(RK_WIREFRAME_OVERLAY)


layout(push_constant) uniform PushConstants
{
    mat4 projection;
    mat4 modelview;
} pc;

// inter-stage interface block
struct RkData {
    vec4 color;
    vec2 uv;
    vec3 up_view;
    vec3 vertex_view;
};

// input varyings
layout(location = 0) in RkData rk;

// uniforms
layout(set = 0, binding = 11) uniform sampler2D color_tex;
layout(set = 0, binding = 12) uniform sampler2D normal_tex;

#if defined(RK_LIGHTING)
#include "rocky.lighting.frag.glsl"
#endif

// outputs
layout(location = 0) out vec4 out_color;

vec3 get_normal()
{
    // temporary! until we support normal maps
    vec3 dx = dFdx(rk.vertex_view);
    vec3 dy = dFdy(rk.vertex_view);
    vec3 n = -normalize(cross(dx, dy));
    return n;
}

void main()
{
    vec4 texel = texture(color_tex, rk.uv);
    out_color = mix(rk.color, clamp(texel, 0, 1), texel.a);

    if (gl_FrontFacing == false)
        out_color.r = 1.0;

#if defined(RK_LIGHTING)
    apply_lighting(out_color, rk.vertex_view, get_normal());
#endif

#if defined(RK_WIREFRAME_OVERLAY)
    // tile outlines - debugging
    vec2 outline_uv = abs(rk.uv * 2.0 - 1.0);
    if (outline_uv.x > 0.99 || outline_uv.y > 0.99)
        out_color = vec4(1.0, 0.9, 0.0, 1.0);
#endif
}
