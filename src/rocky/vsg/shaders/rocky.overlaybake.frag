#version 450
/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */

layout(location = 0) in vec4 out_color;
layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = out_color;
}
