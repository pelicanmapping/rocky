#version 460

// vsg push constants
layout(push_constant) uniform PushConstants
{
    mat4 projection;
    mat4 modelview;
} pc;

struct Instance
{
    mat4 proj;
    mat4 modelview;
    vec4 viewport;          // viewport x,y,w,h
    float rotation;         // rotation, radians
    float size;             // size in pixels; 0 = not visible
    int texture_index;      // ID of icon texture, -1 = error
    float padding[1];       // pad to 16 bytes
};

// draw buffer, output from the culling shader
layout(set = 0, binding = 2) buffer DrawBuffer
{
    Instance drawList[];
};

// vsg viewport data
layout(set = 1, binding = 1) readonly buffer VSG_Viewports
{
    vec4 vsg_viewports[1]; // x, y, width, height
};

// input vertex attributes
layout(location = 0) in vec3 in_vertex;

// output varyings
layout(location = 0) out vec2 uv;
layout(location = 1) flat out int texture_index;

// GL built-ins
out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{    
    int i = gl_InstanceIndex;
    texture_index = -1;

    vec4 clip = drawList[i].proj * drawList[i].modelview * vec4(0,0,0,1);

    vec2 viewport_size = vsg_viewports[0].zw;
    vec2 pixel_size = 2.0 / viewport_size;

    vec2 signs = vec2(
        gl_VertexIndex == 0 || gl_VertexIndex == 3 ? -1 : 1,
        gl_VertexIndex == 0 || gl_VertexIndex == 1 ? -1 : 1);
        
    float size = abs(drawList[i].size);
    float sr = sin(drawList[i].rotation);
    float cr = cos(drawList[i].rotation);
    vec2 offset = mat2(cr, sr, -sr, cr) * (size * signs * 0.5);

    clip.xy += offset * pixel_size * clip.w;

    uv = vec2(signs.x + 1.0, -signs.y + 1.0) * 0.5;

    if (drawList[i].size > 0.0)
    {
        texture_index = drawList[i].texture_index;
    }

    gl_Position = clip;
}
