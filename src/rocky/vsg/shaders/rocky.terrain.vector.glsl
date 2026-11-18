/** rocky c++; Copyright 2026 Pelican Mapping; MIT License */
#ifdef ROCKY_HAS_SLUGHORN
#pragma include "rocky.slug.glsl"
#pragma include "rocky.terrain.stroke.glsl"

layout(set=DESCRIPTOR_SET_LOCAL, binding=BINDING_TERRAIN_VECTOR_CURVES) uniform sampler2D u_vectorCurves;
layout(set=DESCRIPTOR_SET_LOCAL, binding=BINDING_TERRAIN_VECTOR_BANDS) uniform usampler2D u_vectorBands;

struct TerrainVectorLayer {
    vec4 color;
    vec4 uvToEmX;
    vec4 uvToEmY;
    vec4 bandTransform;
    uvec4 shapeData;
    vec4 projection;
};
layout(std430, set=DESCRIPTOR_SET_LOCAL, binding=BINDING_TERRAIN_VECTOR_LAYERS) readonly buffer VectorLayers {
    uvec4 header;
    TerrainVectorLayer layers[];
} u_vectorLayers;
layout(std430, set=DESCRIPTOR_SET_LOCAL, binding=BINDING_TERRAIN_VECTOR_GRID) readonly buffer VectorGrid {
    uint words[];
} u_vectorGrid;

// Evaluate tile-local analytic coverage directly on the terrain surface.
// Bins retain painter order; broad pixel footprints use the complete list.
void applyTerrainVectors(inout vec4 color, vec2 uv, vec2 dx, vec2 dy)
{
    uint count = u_vectorLayers.header.x;
    if (count == 0u) return;
    bool full = any(greaterThan(abs(dx) + abs(dy), vec2(1.0 / 16.0)));
    uvec2 cell = uvec2(clamp(floor(uv * 16.0), vec2(0), vec2(15)));
    uint address = (cell.y * 16u + cell.x) * 2u;
    uint start = u_vectorGrid.words[address];
    uint n = full ? count : u_vectorGrid.words[address + 1u];
    uint group = ~0u;
    float accumulated = 0.0;
    vec4 paint = vec4(0);
    for (uint j = 0u; j < n; ++j)
    {
        uint i = full ? j : u_vectorGrid.words[start + j];
        TerrainVectorLayer layer = u_vectorLayers.layers[i];
        uint nextGroup = uint(layer.uvToEmY.w);
        if (nextGroup != group || any(notEqual(paint, layer.color)))
        {
            float alpha = clamp(accumulated, 0.0, 1.0) * paint.a;
            color.rgb = mix(color.rgb, paint.rgb, alpha);
            color.a = alpha + color.a * (1.0 - alpha);
            accumulated = 0.0;
            paint = layer.color;
            group = nextGroup;
        }
        vec2 coord = uv, coordDx = dx, coordDy = dy;
        if (layer.projection.w == 1.0)
        {
            // Mercator differences computed locally avoid cancellation on fine terrain levels.
            const float limit = 1.48442222974533;
            float origin = clamp(layer.projection.x, -limit, limit);
            float rawLatitude = layer.projection.x + (uv.y - 0.5) * layer.projection.y;
            if (abs(rawLatitude) > limit) continue;
            float latitude = clamp(rawLatitude, -limit, limit);
            float delta = (uv.y - 0.5) * layer.projection.y;
            if (abs(rawLatitude) > limit || abs(layer.projection.x) > limit) delta = latitude - origin;
            // Preserve relative precision for tiny tile-local angles; hardware sin can
            // quantize these before the large Mercator normalization magnifies them.
            float delta2 = delta * delta;
            float sinHalfDelta = abs(delta) < 0.01 ?
                0.5 * delta * (1.0 - delta2 / 24.0 + delta2 * delta2 / 1920.0) : sin(delta * 0.5);
            float sineDelta = 2.0 * cos(origin + delta * 0.5) * sinHalfDelta;
            float t = sineDelta / (cos(origin) * cos(origin) - sin(origin) * sineDelta);
            t = clamp(t, -0.9999999, 0.9999999);
            float difference = abs(t) < 0.01 ? t * (1.0 + t*t/3.0 + t*t*t*t/5.0) : atanh(t);
            coord.y = difference * layer.projection.z;
            float jacobian = abs(rawLatitude) < limit ? layer.projection.y * layer.projection.z / cos(latitude) : 0.0;
            coordDx.y *= jacobian;
            coordDy.y *= jacobian;
        }
        else if (layer.projection.w == 2.0)
        {
            coord.y = (uv.y - 0.5) * layer.projection.y;
            coordDx.y *= layer.projection.y;
            coordDy.y *= layer.projection.y;
        }
        vec2 em = vec2(dot(layer.uvToEmX.xyz, vec3(coord, 1)), dot(layer.uvToEmY.xyz, vec3(coord, 1)));
        vec2 emDx = vec2(dot(layer.uvToEmX.xy, coordDx), dot(layer.uvToEmY.xy, coordDx));
        vec2 emDy = vec2(dot(layer.uvToEmX.xy, coordDy), dot(layer.uvToEmY.xy, coordDy));
        bool stroke = layer.uvToEmX.w > 0.0;
        float coverage = stroke ? terrainRenderStroke(em, emDx, emDy, layer.bandTransform,
            ivec2(layer.shapeData.xy), ivec2(layer.shapeData.zw),
            u_vectorCurves, u_vectorBands, int(u_vectorLayers.header.y), layer.uvToEmX.w) :
            slug_Render(em, abs(emDx) + abs(emDy), layer.bandTransform,
                ivec2(layer.shapeData.xy), ivec2(layer.shapeData.zw),
                u_vectorCurves, u_vectorBands, int(u_vectorLayers.header.y));
        coverage = clamp(coverage, 0.0, 1.0);
        accumulated = stroke ? max(accumulated, coverage) : accumulated + coverage;
    }
    float alpha = clamp(accumulated, 0.0, 1.0) * paint.a;
    color.rgb = mix(color.rgb, paint.rgb, alpha);
    color.a = alpha + color.a * (1.0 - alpha);
}
#endif
