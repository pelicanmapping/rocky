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

    outColor = mix(outColor, apply_lighting(outColor, vary.vertexView, vary.normal), vary.applyLighting);
}
