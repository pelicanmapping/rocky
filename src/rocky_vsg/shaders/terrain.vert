#version 450

layout(push_constant) uniform PushConstants
{
    mat4 projection;
    mat4 modelview;
} pc;

layout(location = 0) in vec3 inVertex;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inUV;
layout(location = 3) in vec3 inNeighborVertex;
layout(location = 4) in vec3 inNeighborNormal;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragUV;
layout(location = 2) out vec3 oe_UpVectorView;
layout(location = 3) out vec2 oe_normalMapCoords;
layout(location = 4) out vec3 oe_normalMapBinormal;

out gl_PerVertex {
    vec4 gl_Position;
};

void main()
{
    mat4 projection = pc.projection;

    //if (reverseDepth != 0)
    //{
        //projection[2][2] *= -pc.projection[2][2] - 1.0;
        //projection[3][2] *= -1.0;
    //}

    float elevation = 0.0; // oe_terrain_getElevation(inTexCoord);
    vec3 position = inVertex + inNormal*elevation;
    gl_Position = (projection * pc.modelview) * vec4(position, 1.0);

    mat3 normalMatrix = mat3(transpose(inverse(pc.modelview)));
    
    fragColor = vec3(1, 1, 1);
    fragUV = inUV;

    // The normal map stuff
    oe_UpVectorView = normalMatrix * inNormal;
    oe_normalMapCoords = inUV.st;
    oe_normalMapBinormal = normalize(normalMatrix * vec3(0.0, 1.0, 0.0));
}
