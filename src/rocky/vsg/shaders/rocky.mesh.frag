#version 450
/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
layout(location = 1) in Varyings {
    vec4 color;
    vec2 uv;
    vec3 normal;
    vec3 vertexVs;
    float applyTexture;
    float applyLighting;
    flat uint stipplePattern;
} vary;

// outputs
layout(location = 0) out vec4 outColor;

// u_textures
layout(set = 0, binding = 2) uniform sampler2D u_meshTexture;

// lighting
#include "rocky.lighting.glsl"

// f+ tiles
#include "rocky.frustumgrid.h.glsl"

bool stipple(ivec2 p)
{
    // 4x4 stipple pattern
    int bit = (p.y % 4) * 4 + (p.x % 4);
    return (vary.stipplePattern & (1 << bit)) != 0;
}

void main()
{
    outColor = vary.color;

    outColor = mix(outColor, outColor * texture(u_meshTexture, vary.uv), vary.applyTexture);

    vec4 litColor = applyLighting(outColor, vary.vertexVs, vary.normal);
    outColor = mix(outColor, litColor, vary.applyLighting);

    if (!stipple(ivec2(gl_FragCoord.xy)))
        discard;
}
