#version 450

layout(location = 1) in Varyings {
    vec4 color;
    vec2 uv;
    vec3 normal;
    vec3 vertexView;
    float applyTexture;
    float applyLighting;
} vary;

// outputs
layout(location = 0) out vec4 outColor;

// textures
layout(set = 0, binding = 2) uniform sampler2D meshTexture;

// lighting
#include "rocky.lighting.frag.glsl"


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
    outColor = vary.color;

    outColor = mix(outColor, outColor * texture(meshTexture, vary.uv), vary.applyTexture);

    vec4 litColor = apply_lighting(outColor, vary.vertexView, vary.normal);
    outColor = mix(outColor, litColor, vary.applyLighting);

    // Tone mapping (apply_lighting returns linear HDR)
    outColor.rgb = mix(outColor.rgb, ACES_tonemap(outColor.rgb * 3.3), vary.applyLighting);
}
