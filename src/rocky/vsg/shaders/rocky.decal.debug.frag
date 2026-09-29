#version 450
/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
layout(location = 0) out vec4 out_color;

// High-contrast outline only; both pipeline variants leave scene depth unchanged.
void main()
{
    out_color = vec4(1.0, 0.25, 0.05, 1.0);
}
