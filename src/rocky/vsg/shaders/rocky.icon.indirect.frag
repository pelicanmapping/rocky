#version 460

// Texture arena (fixed size)
//layout(set = 0, binding = 3) uniform sampler samp;
layout(set = 0, binding = 4) uniform sampler2D textures[1];

// input varyings
layout(location = 0) in vec2 uv;
layout(location = 1) flat in int texture_index;

// outputs
layout(location = 0) out vec4 out_color;

void main()
{
    const vec4 error_color = vec4(1,0,0,1);

    if (texture_index < 0)
    {
        out_color = error_color;
    }
    else
    {
        out_color = texture(textures[texture_index], uv);
    }

    if (out_color.a < 0.15)
        discard;
}
