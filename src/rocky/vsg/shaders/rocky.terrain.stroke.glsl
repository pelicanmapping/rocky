/** rocky c++; Copyright 2026 Pelican Mapping; MIT License */
#ifndef ROCKY_TERRAIN_STROKE_GLSL
#define ROCKY_TERRAIN_STROKE_GLSL
#pragma include "rocky.slug.path.glsl"

// Screen footprint of an edge distance, using the carrier's explicit derivatives.
// Unlike an axis average, this retains edge orientation and anisotropic projection.
float terrainStrokeRate(vec2 gradient, vec2 dx, vec2 dy)
{
    return max(abs(dot(gradient, dx)) + abs(dot(gradient, dy)), 1e-30);
}

// The same round-ended segment coverage used by the Slug path pass. End/body gradients
// are analytic here because atlas traversal is divergent and cannot safely call fwidth.
// Points are already relative to the fragment; round segment unions also form round joins.
float terrainStrokeSegment(vec2 a, vec2 b, float radius, vec2 dx, vec2 dy)
{
    vec2 segment = b - a;
    float length2 = dot(segment, segment);
    float da = length(a), db = length(b);
    if (length2 <= 1e-20)
    {
        vec2 normal = da > 1e-15 ? -a / da : vec2(1.0, 0.0);
        float rate = terrainStrokeRate(normal, dx, dy);
        return clamp((radius - da) / rate + 0.5, 0.0, 1.0);
    }
    vec2 dir = segment * inversesqrt(length2);
    vec2 perp = slug_PathPerp(dir);
    float alongStart = dot(-a, dir), alongEnd = dot(b, dir);
    float roundStart = radius - da, roundEnd = radius - db;
    slug_PathSample pathSample = slug_PathSample(
        max(alongStart, roundStart), max(alongEnd, roundEnd), dot(-a, perp));
    vec2 startGradient = alongStart >= roundStart || da <= 1e-15 ? dir : a / da;
    vec2 endGradient = alongEnd >= roundEnd || db <= 1e-15 ? -dir : b / db;
    vec3 rates = vec3(terrainStrokeRate(startGradient, dx, dy),
        terrainStrokeRate(endGradient, dx, dy), terrainStrokeRate(perp, dx, dy));
    return slug_PathCoverage(pathSample, rates, radius);
}

// Evaluate the tile's polyline atlas with Slug's path coverage instead of the experimental
// scalar-distance ramp. Band lookups include the entire pixel footprint before early exits.
float terrainRenderStroke(vec2 coord, vec2 dx, vec2 dy, vec4 bandTransform,
    ivec2 glyphLoc, ivec2 bandMax, sampler2D curves, usampler2D bands, int widthLog2, float radius)
{
    vec2 pixel = abs(dx) + abs(dy);
    vec2 bandCoord = coord * bandTransform.xy + bandTransform.zw;
    vec2 margin = 0.5 * abs(pixel * bandTransform.xy);
    if (any(lessThan(bandCoord, -margin)) ||
        any(greaterThan(bandCoord, vec2(float(SLUG_INDIRECTION_SIZE)) + margin))) return 0.0;

    ivec2 q = clamp(ivec2(bandCoord), ivec2(0), ivec2(SLUG_INDIRECTION_SIZE - 1));
    int bandY = int(texelFetch(bands, ivec2(glyphLoc.x + q.y, glyphLoc.y), 0).r);
    int bandX = int(texelFetch(bands, ivec2(glyphLoc.x + SLUG_INDIRECTION_SIZE + q.x, glyphLoc.y), 0).r);
    int hHeaders = glyphLoc.x + 2 * SLUG_INDIRECTION_SIZE;
    int vHeaders = hHeaders + bandMax.y + 1;
    bool horizontal = texelFetch(bands, ivec2(hHeaders + bandY, glyphLoc.y), 0).x <=
        texelFetch(bands, ivec2(vHeaders + bandX, glyphLoc.y), 0).x;
    float position = horizontal ? coord.y : coord.x;
    float scale = horizontal ? bandTransform.y : bandTransform.x;
    float offset = horizontal ? bandTransform.w : bandTransform.z;
    float fringe = 0.5 * (horizontal ? pixel.y : pixel.x);
    int indirection = glyphLoc.x + (horizontal ? 0 : SLUG_INDIRECTION_SIZE);
    int headers = horizontal ? hHeaders : vHeaders;
    int qFirst = clamp(int((position - fringe) * scale + offset), 0, SLUG_INDIRECTION_SIZE - 1);
    int qLast = clamp(int((position + fringe) * scale + offset), 0, SLUG_INDIRECTION_SIZE - 1);
    int first = int(texelFetch(bands, ivec2(indirection + qFirst, glyphLoc.y), 0).r);
    int last = int(texelFetch(bands, ivec2(indirection + qLast, glyphLoc.y), 0).r);
    float cutoff = -radius - 0.5 * (horizontal ? pixel.x : pixel.y);
    float coverage = 0.0;
    for (int band = first; band <= last; ++band)
    {
        uvec2 data = texelFetch(bands, ivec2(headers + band, glyphLoc.y), 0).xy;
        ivec2 list = slug_CalcBandLoc(glyphLoc, data.y, widthLog2);
        for (uint c = 0u; c < data.x; ++c)
        {
            ivec2 location = ivec2(texelFetch(bands, slug_CalcBandLoc(list, c, widthLog2), 0).xy);
            vec4 p12 = texelFetch(curves, location, 0) - vec4(coord, coord);
            vec2 p3 = texelFetch(curves, slug_CalcBandLoc(location, 1u, widthLog2), 0).xy - coord;
            float maximum = horizontal ? max(max(p12.x, p12.z), p3.x) : max(max(p12.y, p12.w), p3.y);
            if (maximum < cutoff) break;
            // VectorTile line contours are polylines; the adapter stores lineTo as {a,b,b}.
            coverage = max(coverage, terrainStrokeSegment(p12.xy, p3, radius, dx, dy));
        }
    }
    return coverage;
}
#endif
