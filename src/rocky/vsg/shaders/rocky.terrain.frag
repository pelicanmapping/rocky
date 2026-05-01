#version 460
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
    vec3 viewdir_ecef;
    vec3 camera_ecef;
    vec3 sundir_ecef;
    float discardVert;
} vary;

// uniforms (TerrainState.h)
layout(set = 0, binding = 9) uniform TerrainData {
    vec4 backgroundColor;
    vec2 ellipsoidAxes;
    float atmosphere;
    float lighting;
    float debugTriangles;
    float debugNormals;
} settings;

layout(set = 0, binding = 11) uniform sampler2D color_tex;

#pragma include "rocky.debug.frag.glsl"
#pragma include "rocky.lighting.glsl"
#pragma include "rocky.atmo.glsl"

// outputs
layout(location = 0) out vec4 out_color;

void main()
{
    if (vary.discardVert > 0.0)
        discard;

    // sample the imagery color
    vec4 texel = texture(color_tex, vary.uv);

    // mix in the background color
    out_color = mix(settings.backgroundColor, clamp(texel, 0, 1), texel.a);

    vec3 normal_vs = normalize(vary.normal_vs);

    // debug normals
    out_color.rgb = mix(out_color.rgb, (normal_vs + 1.0) * 0.5, settings.debugNormals);

#if defined(ROCKY_ATMOSPHERE)
    vec3 ground_color = apply_atmo_color_to_ground(out_color.rgb, vary.vertex_vs, vary.viewdir_ecef,
        vary.camera_ecef, normalize(vary.sundir_ecef), settings.ellipsoidAxes);

    out_color.rgb = mix(out_color.rgb, ground_color, settings.lighting * settings.atmosphere);
#endif

    // PBR lighting
    vec4 lit_color = apply_lighting(out_color, vary.vertex_vs, normal_vs);
    out_color = mix(out_color, lit_color, settings.lighting);

    // show triangle outlines
    apply_debug_triangles(out_color, settings.debugTriangles);
}
