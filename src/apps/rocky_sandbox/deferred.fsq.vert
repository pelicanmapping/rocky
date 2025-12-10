#version 450

layout(location = 0) out vec2 uv;

// call with vkCmdDraw(cmd, 4, 1, 0, 0) and a TRIPSTRIP IA topology
// to render a full-screen quad.
void main()
{
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