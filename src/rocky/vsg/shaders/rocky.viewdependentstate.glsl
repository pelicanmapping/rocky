/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma include "rocky.defines.h.glsl"

layout(set = DESCRIPTOR_SET_VDS, binding = BINDING_VDS_RENDER_PARAMS) uniform RenderParams
{
    mat4 viewMatrix;
    mat4 inverseViewMatrix;
    vec2 ellipsoidAxes;
    uint stereographic;
    float _padding[1];
}
u_renderParams;
