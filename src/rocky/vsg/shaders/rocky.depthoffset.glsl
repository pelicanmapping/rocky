/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */

// Moves the vertex closer to the camera by the specified bias,
// clamping it beyond the near clip plane if necessary.
vec4 applyDepthOffset(in vec4 vertex, in float offset, in mat4 projection)
{
    // In overlay bake passes, style depth offsets cause geometry to leave the
    // bake camera frustum and produce empty/incorrect projections.
    // ViewDependentState encodes this in u_renderParams.renderDomain.
    if (u_renderParams.renderDomain > 0.5)
        offset = 0.0;

    vertex.xyz /= vertex.w;
    float n = projection[3][3] == 0 ?
        -projection[3][2] / (projection[2][2] + 1.0) : // perspective
        -1.0; //-(projection[3][2] + 1.0) / projection[2][2];  // orthographic
    float t_n = (-n + 1.0) / -vertex.z; // [0..1] -> [n+1 .. vertex]
    if (t_n <= 0.0)
        return vertex; // already behind near plane
    float len = length(vertex.xyz);
    float t_offset = 1.0 - (offset / len);
    return vec4(vertex.xyz * max(t_n, t_offset), vertex.w);
}