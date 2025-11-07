#version 460

#extension GL_EXT_fragment_shader_barycentric : enable

#pragma import_defines(ROCKY_HAS_VK_BARYCENTRIC_EXTENSION)

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelview;
} pc;

// inter-stage interface block
layout(location = 0) in Varyings {
    vec2 uv;
    vec3 upView;
    vec3 vertexView;
} vary;

// uniforms (TerrainNode.h)
layout(set = 0, binding = 9) uniform TerrainData {
    vec4 backgroundColor;
    float wireOverlay;
    float lighting;
} settings;

layout(set = 0, binding = 11) uniform sampler2D color_tex;

#include "rocky.lighting.frag.glsl"

// outputs
layout(location = 0) out vec4 out_color;

vec3 get_normal()
{
    // temporary! until we support normal maps
    vec3 dx = dFdx(vary.vertexView);
    vec3 dy = dFdy(vary.vertexView);
    vec3 n = -normalize(cross(dx, dy));
    return n;
}

void main()
{
    vec4 texel = texture(color_tex, vary.uv);

    out_color = mix(settings.backgroundColor, clamp(texel, 0, 1), texel.a);

    // lighting:
    out_color = mix(
        out_color,
        apply_lighting(out_color, vary.vertexView, get_normal()),
        settings.lighting);

#if defined(ROCKY_HAS_VK_BARYCENTRIC_EXTENSION) && defined(GL_EXT_fragment_shader_barycentric)
    // wireframe overlay:
    vec3 b = fwidth(gl_BaryCoordEXT.xyz);
    vec3 edge = smoothstep(vec3(0.0), b, gl_BaryCoordEXT.xyz);
    float wire = 1.0 - min(min(edge.x, edge.y), edge.z);
    vec3 wire_color = clamp(out_color.rgb*3.0, 0.05, 1.0);
    out_color.rgb = mix(out_color.rgb, wire_color, clamp(wire, 0.0, 1.0) * settings.wireOverlay);
#endif
}
