#version 450

layout(location = 0) in vec2 uv;

layout(set = 0, binding = 0) uniform sampler2D gAlbedo;
layout(set = 0, binding = 1) uniform sampler2D gDepth;

layout(location = 0) out vec4 outColor;

void main()
{
    const vec4 colors[4] = {
        vec4(1,0,0,1),
        vec4(0,1,0,1),
        vec4(0,0,1,1),
        vec4(1,1,0,1)
    };

    vec4 topcolor = mix(colors[0], colors[1], uv.x);
    vec4 botcolor = mix(colors[2], colors[3], uv.x);
    outColor = mix(topcolor, botcolor, uv.y);

    outColor = texture(gAlbedo, uv);

    // simple blur filter:
    const float kernel[9] = {
        1.0/16, 2.0/16, 1.0/16,
        2.0/16, 4.0/16, 2.0/16,
        1.0/16, 2.0/16, 1.0/16
    };


    vec2 tex_offset = 1.0 / vec2(textureSize(gAlbedo, 0)); // gets size of single texel
    vec3 result = vec3(0.0);
    for(int y = -1; y <= 1; y++)
    {
        for(int x = -1; x <= 1; x++)
        {
            vec2 offset = vec2(float(x), float(y)) * tex_offset;
            result += texture(gAlbedo, uv + offset).rgb * kernel[(y+1)*3 + (x+1)];
        }
    }
    outColor = vec4(result, 1.0);

    //if (texture(gDepth, uv).r > 0.0) outColor = vec4(1,1,0,1); else outColor = vec4(0,0,0,1);
}
