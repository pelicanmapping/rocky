#version 450

layout(location = 1) in vec2 uv;

// inter-stage interface block
struct Varyings {
    vec4 color;
    int textureIndex;
};
layout(location = 2) flat in Varyings vary;

// outputs
layout(location = 0) out vec4 outColor;

// textures
layout(set = 0, binding = 2) uniform sampler2D meshTexture[1024];


void main()
{
    outColor = vary.color;

    if (vary.textureIndex >= 0)
    {
        outColor.rgb *= texture(meshTexture[vary.textureIndex], uv).rgb;
    }
}
