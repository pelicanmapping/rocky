#version 460

#extension GL_EXT_fragment_shader_barycentric : enable

#pragma import_defines(ROCKY_HAS_VK_BARYCENTRIC_EXTENSION)

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelview;
} pc;

// inter-stage interface block
struct RockyVaryings {
    vec2 uv;
    vec3 up_view;
    vec3 vertex_view;
};

// input varyings
layout(location = 0) in RockyVaryings varyings;

// uniforms (TerrainNode.h)
layout(set = 0, binding = 9) uniform TerrainData
{
    vec4 backgroundColor;
    bool wireOverlay;
} settings;

layout(set = 0, binding = 11) uniform sampler2D color_tex;

#include "rocky.lighting.frag.glsl"

// outputs
layout(location = 0) out vec4 out_color;

vec3 get_normal()
{
    // temporary! until we support normal maps
    vec3 dx = dFdx(varyings.vertex_view);
    vec3 dy = dFdy(varyings.vertex_view);
    vec3 n = -normalize(cross(dx, dy));
    return n;
}

void main()
{
    vec4 texel = texture(color_tex, varyings.uv);
    out_color = mix(settings.backgroundColor, clamp(texel, 0, 1), texel.a);

    if (gl_FrontFacing == false)
        out_color.r = 1.0;

    apply_lighting(out_color, varyings.vertex_view, get_normal());

#if defined(ROCKY_HAS_VK_BARYCENTRIC_EXTENSION) && defined(GL_EXT_fragment_shader_barycentric)
    if (settings.wireOverlay)
    {
        const float pixelWidth = 1.0;
        vec3 b = fwidth(gl_BaryCoordEXT.xyz);
        vec3 edge = smoothstep(vec3(0.0), b * pixelWidth, gl_BaryCoordEXT.xyz);
        float wire = 1.0 - min(min(edge.x, edge.y), edge.z);
        vec3 wire_color = clamp(out_color.rgb*3.0, 0.05, 1.0);
        out_color.rgb = mix(out_color.rgb, wire_color, clamp(wire, 0.0, 1.0));
    }
#endif
}
