#version 450
/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma import_defines(ROCKY_HAS_ATMOSPHERE)

#pragma include "rocky.viewdependentstate.glsl"
#pragma include "rocky.projection.glsl"
#pragma include "rocky.depthoffset.glsl"

// vsg push constants
layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelview;
} pc;

// input vertex attributes
layout(location = 0) in vec3 in_vertex;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec4 in_color;
layout(location = 3) in vec2 in_uv;

// rocky::detail::MeshStyleRecord
struct MeshStyle {
    vec4 color;
    float depthOffset;
    uint stipplePattern;
    uint featureMask; // see defines below
    uint _padding[1];
};

#define MASK_HAS_TEXTURE 1
#define MASK_HAS_LIGHTING 2
#define MASK_HAS_PER_VERTEX_COLORS 4

layout(set = 0, binding = 1) uniform MeshUniform {
    MeshStyle style;
} u_mesh;

layout(location = 1) out Varyings {
    vec4 color;
    vec2 uv;
    vec3 normal;
    vec3 vertexVs;
    float applyTexture;
    float applyLighting;
    flat uint stipplePattern;
} vary;

// GLSL built-ins
out gl_PerVertex {
    vec4 gl_Position;
    float gl_ClipDistance[1];
};

void main()
{    
    bool hasPerVertexColors = (MASK_HAS_PER_VERTEX_COLORS & u_mesh.style.featureMask) != 0;
    bool hasTexture = (MASK_HAS_TEXTURE & u_mesh.style.featureMask) != 0;
    bool hasLighting = (MASK_HAS_LIGHTING & u_mesh.style.featureMask) != 0;

    vary.color = hasPerVertexColors ? in_color : u_mesh.style.color;
    vary.applyTexture = hasTexture ? 1.0 : 0.0;
    vary.applyLighting = hasLighting ? 1.0 : 0.0;
    vary.stipplePattern = u_mesh.style.stipplePattern;

    vec4 vertexVs = pc.modelview * vec4(in_vertex, 1.0);
    
    vertexVs = applyProjection(vertexVs, pc.projection);

    mat3 normalMatrix = mat3(transpose(inverse(pc.modelview)));
    vary.normal = normalMatrix * in_normal;
    
    vertexVs = applyDepthOffset(vertexVs, u_mesh.style.depthOffset, pc.projection);

    vary.vertexVs = vertexVs.xyz / vertexVs.w;
    vary.uv = in_uv;

    gl_Position = pc.projection * vertexVs;
}
