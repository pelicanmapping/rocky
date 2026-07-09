#version 450
/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
layout(location = 1) in Varyings {
    flat vec4 color;
    flat float antialias;
} vary;

// outputs
layout(location = 0) out vec4 outColor;

void main()
{
    outColor = vary.color;

    vec2 p = gl_PointCoord - vec2(0.5);
    float r = length(p) * 2.0;
    outColor.a = smoothstep(1.0, 1.0 - vary.antialias, r);

    if (outColor.a < 0.01)
        discard;
}
