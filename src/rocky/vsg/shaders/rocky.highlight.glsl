/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
layout(set = 2, binding = 0) uniform HighlightUniform {
    vec4 color;
} u_highlight;

// Blend the final RGB toward the entity tint while preserving coverage and opacity.
vec4 applyHighlight(vec4 color)
{
    return vec4(mix(color.rgb, u_highlight.color.rgb, clamp(u_highlight.color.a, 0.0, 1.0)), color.a);
}
