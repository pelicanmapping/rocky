#version 460

#extension GL_EXT_fragment_shader_barycentric : enable

#pragma import_defines(ROCKY_HAS_VK_BARYCENTRIC_EXTENSION)
#pragma import_defines(ROCKY_ATMOSPHERE)

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelview;
} pc;

// inter-stage interface block
layout(location = 0) in Varyings {
    vec2 uv;
    vec3 normal_vs;
    vec3 vertex_vs;
    vec3 vertex_ws;
    vec3 camera_ws;
    vec3 sun_ws;
} vary;

// uniforms (TerrainState.h)
layout(set = 0, binding = 9) uniform TerrainData {
    vec4 backgroundColor;
    vec2 ellipsoidAxes;
    float atmosphere;
    float wireOverlay;
    float lighting;
    float debugNormals;
} settings;

layout(set = 0, binding = 11) uniform sampler2D color_tex;

#include "rocky.lighting.frag.glsl"

#if defined(ROCKY_ATMOSPHERE)
#include "rocky.atmo.ground.frag.glsl"
#endif

// outputs
layout(location = 0) out vec4 out_color;

void main()
{
    // sample the imagery color
    vec4 texel = texture(color_tex, vary.uv);

    // mix in the background color
    out_color = mix(settings.backgroundColor, clamp(texel, 0, 1), texel.a);

    vec3 normal_vs = normalize(vary.normal_vs);

    // debug normals
    out_color.rgb = mix(out_color.rgb, (normal_vs + 1.0) * 0.5, settings.debugNormals);

#if defined(ROCKY_ATMOSPHERE)
    vec3 ground_color = apply_atmo_color_to_ground(out_color.rgb, vary.vertex_vs, vary.vertex_ws,
        vary.camera_ws, normalize(vary.sun_ws), settings.ellipsoidAxes);

    out_color.rgb = mix(out_color.rgb, ground_color, settings.lighting * settings.atmosphere);
#endif

    // PBR lighting
    vec4 lit_color = apply_lighting(out_color, vary.vertex_vs, normal_vs);
    out_color = mix(out_color, lit_color, settings.lighting);

#if defined(ROCKY_HAS_VK_BARYCENTRIC_EXTENSION) && defined(GL_EXT_fragment_shader_barycentric)
    // wireframe overlay
    vec3 b = fwidth(gl_BaryCoordEXT.xyz);
    vec3 edge = smoothstep(vec3(0.0), b, gl_BaryCoordEXT.xyz);
    float wire = 1.0 - min(min(edge.x, edge.y), edge.z);
    vec3 wire_color = clamp(out_color.rgb*3.0, 0.05, 1.0);
    out_color.rgb = mix(out_color.rgb, wire_color, clamp(wire, 0.0, 1.0) * settings.wireOverlay);
#endif
}
