#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragUV;
layout(location = 2) in vec3 oe_UpVectorView;
layout(location = 3) in vec2 oe_normalMapCoords;
layout(location = 4) in vec3 oe_normalMapBinormal;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor.rgb = fragColor;
}
