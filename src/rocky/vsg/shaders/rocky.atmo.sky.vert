#version 450

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelview;
} pc;

// input attributes
layout(location = 0) in vec3 in_vertex;

// output varyings
layout(location = 0) out vec3 v_worldPos;
layout(location = 1) out vec3 v_camera_ecef;
layout(location = 2) out vec3 v_sundir_ecef;

// GL built-ins
out gl_PerVertex {
    vec4 gl_Position;
};

#include "rocky.lighting.glsl"

void main()
{
    // Dome vertex position is already in ECEF
    v_worldPos = in_vertex;

    // Extract camera ECEF position from the modelview matrix.
    // For an orthonormal rotation, inverse(mat3(MV)) == transpose(mat3(MV))
    mat3 rot = transpose(mat3(pc.modelview));
    v_camera_ecef = rot * (-pc.modelview[3].xyz);
    v_sundir_ecef = rot * (-get_sunlight_direction());

    gl_Position = pc.projection * pc.modelview * vec4(in_vertex, 1.0);
}
