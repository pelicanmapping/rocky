/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma include "rocky.defines.h.glsl"

layout(set = VDS_DESCRIPTOR_SET_INDEX, binding = BINDING_VDS_RENDER_PARAMS) uniform RenderParams {
    mat4 inverseViewMatrix;
    vec2 ellipsoidAxes;
    uint stereographic;
    float _padding[1];
} u_vds;
