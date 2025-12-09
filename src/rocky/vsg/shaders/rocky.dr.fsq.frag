#version 450

layout(location = 0) in vec2 uv;

// g-buffer descriptors:
layout(set = 0, binding = 0) uniform sampler2D gAlbedo;
layout(set = 0, binding = 1) uniform sampler2D gNormal;
layout(set = 0, binding = 2) uniform sampler2D gDepth;

// final output fragment:
layout(location = 0) out vec4 outColor;

// renders a full-screen color gradiant. For testing.
vec4 gradientColor(vec2 in_uv)
{
    const vec4 colors[4] = {
        vec4(1,0,0,1),
        vec4(0,1,0,1),
        vec4(0,0,1,1),
        vec4(1,1,0,1)
    };
    vec4 topcolor = mix(colors[0], colors[1], uv.x);
    vec4 botcolor = mix(colors[2], colors[3], uv.x);
    return mix(topcolor, botcolor, uv.y);
}

vec4 sampleAlbedo(vec2 in_uv)
{
    return texture(gAlbedo, in_uv);
}

// samples the gNormal channel to apply a very simple headlight shading (returns 0..1)
float lighting(vec2 in_uv)
{
    vec3 N = normalize((texture(gNormal, in_uv).xyz * 2.0) - 1.0);
    vec3 L = normalize(vec3(0, 0, 1));
    float NdotL = max(dot(N, L), 0.0);
    return NdotL;
}

// applies a Sobel edge-detection filter at the UV coordinates, returning
// edge intensity [0..1]
float sobel(in vec2 in_uv)
{
    float gx[9] = float[9](
        -1, 0, 1,
        -2, 0, 2,
        -1, 0, 1
    );
    float gy[9] = float[9](
        1, 2, 1,
        0, 0, 0,
        -1, -2, -1
    );
    vec2 tex_offset = 1.0 / vec2(textureSize(gAlbedo, 0)); // gets size of single texel
    float sumX = 0.0;
    float sumY = 0.0;
    for(int y = -1; y <= 1; y++)
    {
        for(int x = -1; x <= 1; x++)
        {
            vec2 offset = vec2(float(x), float(y)) * tex_offset;
            float intensity = length(sampleAlbedo(in_uv + offset).rgb);
            sumX += intensity * gx[(y+1)*3 + (x+1)];
            sumY += intensity * gy[(y+1)*3 + (x+1)];
        }
    }
    float edgeIntensity = length(vec2(sumX, sumY));
    return edgeIntensity;
}

// FSAA calls this to sample a single pixel with lighting and edge detection.
vec4 samplePixel(in vec2 in_uv)
{
    vec4 c = texture(gAlbedo, in_uv);
    c.rgb *= lighting(in_uv);
    c.rgb = mix(c.rgb, vec3(1,1,1), sobel(in_uv));
    return c;
}

// applies simple anti-aliasing (AA) using a 3x3 Gaussian kernel
vec4 aa()
{
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
            result += samplePixel(uv + offset).rgb * kernel[(y+1)*3 + (x+1)];
        }
    }
    return vec4(result, 1.0);
}

// entry point
void main()
{
    //outColor = texture(gAlbedo, uv);
    //outColor.rgb *= lighting(uv);
    //outColor.rgb = mix(outColor.rgb, vec3(1,1,1), sobel(uv));

    // combines all of the above
    outColor = aa();
}
