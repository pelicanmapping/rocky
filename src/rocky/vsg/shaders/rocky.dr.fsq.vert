#version 450

layout(location = 0) out vec2 uv;

void main()
{
    // hard coded full screen quad (tristrip topology)
    const vec2 positions[4] = {
        vec2(-1.0, -1.0),
        vec2( 1.0, -1.0),
        vec2(-1.0,  1.0),
        vec2( 1.0,  1.0)      
    };

    vec2 pos = positions[gl_VertexIndex];
    gl_Position = vec4(pos, 0.0, 1.0);

    // map from clip to UV
    uv = pos * 0.5 + 0.5;
}