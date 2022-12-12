#version 450

// inputs

layout(location = 0) in vec3 frag_color;
layout(location = 1) in vec2 frag_uv;
layout(location = 2) in vec3 oe_UpVectorView;
layout(location = 3) in vec2 oe_normalMapCoords;
layout(location = 4) in vec3 oe_normalMapBinormal;

// uniforms

layout(set = 0, binding = 11) uniform sampler2D color_tex;
layout(set = 0, binding = 12) uniform sampler2D normal_tex;

//layout(set = 0, binding = 3) uniform TerrainTile
//{
//    mat4 color_matrix;
//    mat4 elevation_matrix;
//    mat4 normal_matrix;
//    vec4 elevation_coefficients;
//} tile;

// outputs

layout(location = 0) out vec4 out_color;

//struct TileImageLayer
//{
//    mat4 imageTexMatrix;
//    uint layerIndex;            // Layer's index in voeLayers.imageLayerParams
//    // 12 bytes padding
//};

void main()
{
    out_color = texture(color_tex, frag_uv);

    if (gl_FrontFacing == false)
        out_color *= 0.5;
}
