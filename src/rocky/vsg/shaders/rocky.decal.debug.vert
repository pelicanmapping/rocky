#version 450
/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma include "rocky.defines.h.glsl"
#pragma include "rocky.decal.record.h.glsl"

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelview;
} pc;

layout(std430, set = DESCRIPTOR_SET_VDS, binding = BINDING_VDS_DECALS) readonly buffer Decals {
    Decal decal[];
} b_decals;

// Twelve independent edges, shared by every instance; no vertex/index buffer is required.
const int corners[24] = int[](
    0, 1, 1, 3, 3, 2, 2, 0,
    4, 5, 5, 7, 7, 6, 6, 4,
    0, 4, 1, 5, 2, 6, 3, 7);

// Reconstruct the receiving box or perspective frustum from the same record used by decal shading.
void main()
{
    int corner = corners[gl_VertexIndex];
    vec3 p = vec3(float(corner & 1), float((corner >> 1) & 1), float((corner >> 2) & 1));
    int index = gl_InstanceIndex; // firstInstance is one: skip the count header.
    if (b_decals.decal[index].distance > 0.0)
    {
        float z = mix(b_decals.decal[index].zMin, b_decals.decal[index].zMax, p.z);
        float halfHeight = -z * b_decals.decal[index].tanHalfFovY;
        p = vec3((p.xy * 2.0 - 1.0) * vec2(halfHeight * b_decals.decal[index].aspect, halfHeight), z);
    }
    else
    {
        p -= 0.5;
    }
    // mvm already maps to this view, including terrain-fitted receiving depth. Do not apply modelview twice.
    gl_Position = pc.projection * b_decals.decal[index].mvm * vec4(p, 1.0);
}
