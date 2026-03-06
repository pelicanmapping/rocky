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
    vec2 elevation_uv;
} vary;

// see rocky::TerrainTileDescriptors
layout(set = 0, binding = 13) uniform TileData {
    mat4 elevation_matrix;
    mat4 color_matrix;
    mat4 model_matrix;
    float min_height;
    float max_height;
    float padding[2];
} tile;

// uniforms (TerrainState.h)
layout(set = 0, binding = 9) uniform TerrainData {
    vec4 backgroundColor;
    float wireOverlay;
    float lighting;
    float debugNormals;
} settings;

layout(set = 0, binding = 10) uniform sampler2D elevation_tex;
layout(set = 0, binding = 11) uniform sampler2D color_tex;

#include "rocky.lighting.frag.glsl"

// outputs
layout(location = 0) out vec4 out_color;

// sample the elevation data at a UV tile coordinate
float terrain_get_elevation(in vec2 uv)
{
    float h = texture(elevation_tex, uv).r;
    if (tile.max_height >= tile.min_height)
        h = (h == 1.0) ? 0.0 : (h * (tile.max_height - tile.min_height) + tile.min_height);
    return h;
}

vec3 get_normal()
{
    vec2 texelSize = 1.0 / textureSize(elevation_tex, 0);

    float h_east  = terrain_get_elevation(vary.elevation_uv + vec2( texelSize.x, 0.0));
    float h_west  = terrain_get_elevation(vary.elevation_uv + vec2(-texelSize.x, 0.0));
    float h_north = terrain_get_elevation(vary.elevation_uv + vec2(0.0,  texelSize.y));
    float h_south = terrain_get_elevation(vary.elevation_uv + vec2(0.0, -texelSize.y));

    // Elevation gradients in meters per elevation-UV unit
    float dh_du = (h_east  - h_west)  * 0.5 / texelSize.x;
    float dh_dv = (h_north - h_south) * 0.5 / texelSize.y;

    // Invert the Jacobian of the screen-to-UV mapping to get view-space
    // tangent vectors (in meters) per UV unit.
    vec3 dp_dx  = dFdx(vary.vertexView);
    vec3 dp_dy  = dFdy(vary.vertexView);
    vec2 duv_dx = dFdx(vary.elevation_uv);
    vec2 duv_dy = dFdy(vary.elevation_uv);

    float det   = duv_dx.x * duv_dy.y - duv_dx.y * duv_dy.x;
    vec3 dpos_du = (dp_dx * duv_dy.y - dp_dy * duv_dx.y) / det;
    vec3 dpos_dv = (dp_dy * duv_dx.x - dp_dx * duv_dy.x) / det;

    // Add the elevation-gradient contribution along the surface-up direction.
    vec3 tangent_u = dpos_du + dh_du * vary.upView;
    vec3 tangent_v = dpos_dv + dh_dv * vary.upView;

    // Result is already in view space.
    return normalize(cross(tangent_u, tangent_v));
}

void main()
{
    vec4 texel = texture(color_tex, vary.uv);

    out_color = mix(settings.backgroundColor, clamp(texel, 0, 1), texel.a);

    vec3 normal = get_normal();

    // lighting:
    vec4 lit_color = apply_lighting(out_color, vary.vertexView, normal);
    out_color = mix(out_color, lit_color, settings.lighting);

    // debug normals:
    const float exag = 1.0;
    //normal = normalize(vec3(normal.x*exag, normal.y*exag, normal.z));
    normal = (normal + 1.0) * 0.5;
    out_color.rgb = mix(out_color.rgb, normal, settings.debugNormals);

#if defined(ROCKY_HAS_VK_BARYCENTRIC_EXTENSION) && defined(GL_EXT_fragment_shader_barycentric)
    // wireframe overlay:
    vec3 b = fwidth(gl_BaryCoordEXT.xyz);
    vec3 edge = smoothstep(vec3(0.0), b, gl_BaryCoordEXT.xyz);
    float wire = 1.0 - min(min(edge.x, edge.y), edge.z);
    vec3 wire_color = clamp(out_color.rgb*3.0, 0.05, 1.0);
    out_color.rgb = mix(out_color.rgb, wire_color, clamp(wire, 0.0, 1.0) * settings.wireOverlay);
#endif
}
