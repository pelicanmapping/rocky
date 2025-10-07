#version 450


layout(location = 1) in vec2 uv;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec3 vertexView;
layout(location = 4) flat in vec4 color;
layout(location = 5) flat in int hasTexture;
layout(location = 6) flat in int hasLighting;

// outputs
layout(location = 0) out vec4 outColor;

// textures
layout(set = 0, binding = 2) uniform sampler2D meshTexture;

// lighting
#include "rocky.lighting.frag.glsl"


vec3 get_normal()
{
    // temporary! until we support normal maps
    vec3 dx = dFdx(vertexView);
    vec3 dy = dFdy(vertexView);
    vec3 n = -normalize(cross(dx, dy));
    return n;
}

void main()
{
    outColor = color;

    if (hasTexture > 0)
    {
        outColor.rgb *= texture(meshTexture, uv).rgb;
    }

    if (hasLighting > 0)
    {
        apply_lighting(outColor, vertexView, normal); //get_normal());
    }
}
