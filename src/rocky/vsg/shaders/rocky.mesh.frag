#version 450


//layout(location = 1) in vec2 uv;
//layout(location = 2) in vec3 normal;
//layout(location = 3) in vec3 vertexView;
//layout(location = 4) flat in vec4 color;
//layout(location = 5) flat in int hasTexture;
//layout(location = 6) flat in int hasLighting;

layout(location = 1) in Varyings {
    vec2 uv;
    vec3 normal;
    vec3 vertexView;
    flat vec4 color;
    flat int hasTexture;
    flat int hasLighting;
} vary;

// outputs
layout(location = 0) out vec4 outColor;

// textures
layout(set = 0, binding = 2) uniform sampler2D meshTexture;

// lighting
#include "rocky.lighting.frag.glsl"


vec3 get_normal()
{
    // temporary! until we support normal maps
    vec3 dx = dFdx(vary.vertexView);
    vec3 dy = dFdy(vary.vertexView);
    vec3 n = -normalize(cross(dx, dy));
    return n;
}

void main()
{
    outColor = vary.color;

    if (vary.hasTexture > 0)
    {
        outColor.rgb *= texture(meshTexture, vary.uv).rgb;
    }

    if (vary.hasLighting > 0)
    {
        apply_lighting(outColor, vary.vertexView, vary.normal); //get_normal());
    }
}
