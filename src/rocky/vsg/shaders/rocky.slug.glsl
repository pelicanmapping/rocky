/*
 * Slug analytic coverage evaluator.
 *
 * Adapted from example/slughorn-example-glfw.cpp in Slughorn commit
 * 312ef217aaf6b1c47b05ba7575342b513daa830d.
 *
 * Copyright (c) 2026 AlphaPixel LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * The upstream example obtains emsPerPixel with fwidth(renderCoord). Rocky
 * passes the equivalent derivative magnitude in so this include remains
 * legal when rocky.decal.h.glsl is also compiled into the decal cull shader.
 */
#ifndef ROCKY_SLUG_GLSL
#define ROCKY_SLUG_GLSL

uint slug_CalcRootCode(float y1, float y2, float y3)
{
    uint i1 = floatBitsToUint(y1) >> 31u;
    uint i2 = floatBitsToUint(y2) >> 30u;
    uint i3 = floatBitsToUint(y3) >> 29u;

    uint shift = (i2 & 2u) | (i1 & ~2u);
    shift = (i3 & 4u) | (shift & ~4u);

    return ((0x2E74u >> shift) & 0x0101u);
}

vec2 slug_SolveHorizPoly(vec4 p12, vec2 p3)
{
    vec2 a = p12.xy - p12.zw * 2.0 + p3;
    vec2 b = p12.xy - p12.zw;
    float ra = 1.0 / a.y;
    float rb = 0.5 / b.y;
    float d = sqrt(max(b.y * b.y - a.y * p12.y, 0.0));
    float t1 = (b.y - d) * ra;
    float t2 = (b.y + d) * ra;
    if (abs(a.y) < 1.0 / 65536.0) { t1 = p12.y * rb; t2 = t1; }
    return vec2(
        (a.x * t1 - b.x * 2.0) * t1 + p12.x,
        (a.x * t2 - b.x * 2.0) * t2 + p12.x);
}

vec2 slug_SolveVertPoly(vec4 p12, vec2 p3)
{
    vec2 a = p12.xy - p12.zw * 2.0 + p3;
    vec2 b = p12.xy - p12.zw;
    float ra = 1.0 / a.x;
    float rb = 0.5 / b.x;
    float d = sqrt(max(b.x * b.x - a.x * p12.x, 0.0));
    float t1 = (b.x - d) * ra;
    float t2 = (b.x + d) * ra;
    if (abs(a.x) < 1.0 / 65536.0) { t1 = p12.x * rb; t2 = t1; }
    return vec2(
        (a.y * t1 - b.y * 2.0) * t1 + p12.y,
        (a.y * t2 - b.y * 2.0) * t2 + p12.y);
}

ivec2 slug_CalcBandLoc(ivec2 glyphLoc, uint offset, int textureWidthLog2)
{
    ivec2 bandLoc = ivec2(glyphLoc.x + int(offset), glyphLoc.y);
    bandLoc.y += bandLoc.x >> textureWidthLog2;
    bandLoc.x &= (1 << textureWidthLog2) - 1;
    return bandLoc;
}

float slug_CalcCoverage(float xcov, float ycov, float xwgt, float ywgt)
{
    float coverage = max(
        abs(xcov * xwgt + ycov * ywgt) / max(xwgt + ywgt, 1.0 / 65536.0),
        min(abs(xcov), abs(ycov)));
    return clamp(coverage, 0.0, 1.0);
}

float slug_Render(
    vec2 renderCoord,
    vec2 emsPerPixel,
    vec4 bandTransform,
    ivec2 glyphLoc,
    ivec2 bandMax,
    int atlasIndex,
    int textureWidthLog2)
{
    vec2 pixelsPerEm = 1.0 / emsPerPixel;

    // Slughorn normally rasterizes only the shape's computed quad. Rocky uses
    // the decal projector itself as the carrier, so reject fragments outside
    // that quad before the indirection lookup clamps them to an edge band.
    // Keep half a pixel of room for analytic antialiasing at the boundary.
    vec2 bandCoord = renderCoord * bandTransform.xy + bandTransform.zw;
    vec2 bandMargin = 0.5 * abs(emsPerPixel * bandTransform.xy);
    if (any(lessThan(bandCoord, -bandMargin)) ||
        any(greaterThan(
            bandCoord,
            vec2(float(SLUG_INDIRECTION_SIZE)) + bandMargin)))
    {
        return 0.0;
    }

    // O(1) band index via indirection tables (2 fetches per axis).
    int qY = clamp(int(bandCoord.y), 0, SLUG_INDIRECTION_SIZE - 1);
    int qX = clamp(int(bandCoord.x), 0, SLUG_INDIRECTION_SIZE - 1);
    int bandY = int(texelFetch(u_slugBandTexture[nonuniformEXT(atlasIndex)], ivec2(glyphLoc.x + qY, glyphLoc.y), 0).r);
    int bandX = int(texelFetch(u_slugBandTexture[nonuniformEXT(atlasIndex)], ivec2(glyphLoc.x + SLUG_INDIRECTION_SIZE + qX, glyphLoc.y), 0).r);

    // Horizontal bands -- headers at glyphLoc + 2*IS + bandY.
    float xcov = 0.0, xwgt = 0.0;
    uvec2 hbandData = texelFetch(
        u_slugBandTexture[nonuniformEXT(atlasIndex)],
        ivec2(glyphLoc.x + 2 * SLUG_INDIRECTION_SIZE + bandY, glyphLoc.y), 0).xy;
    ivec2 hbandLoc = slug_CalcBandLoc(glyphLoc, hbandData.y, textureWidthLog2);

    for (int ci = 0; ci < int(hbandData.x); ++ci)
    {
        ivec2 curveLoc = ivec2(texelFetch(
            u_slugBandTexture[nonuniformEXT(atlasIndex)], ivec2(hbandLoc.x + ci, hbandLoc.y), 0).xy);
        vec4 p12 = texelFetch(u_slugCurveTexture[nonuniformEXT(atlasIndex)], curveLoc, 0) - vec4(renderCoord, renderCoord);
        vec2 p3 = texelFetch(u_slugCurveTexture[nonuniformEXT(atlasIndex)], ivec2(curveLoc.x + 1, curveLoc.y), 0).xy - renderCoord;

        if (max(max(p12.x, p12.z), p3.x) * pixelsPerEm.x < -0.5) break;

        uint code = slug_CalcRootCode(p12.y, p12.w, p3.y);
        if (code != 0u)
        {
            vec2 r = slug_SolveHorizPoly(p12, p3) * pixelsPerEm.x;
            if ((code & 1u) != 0u)
            {
                xcov += clamp(r.x + 0.5, 0.0, 1.0);
                xwgt = max(xwgt, clamp(1.0 - abs(r.x) * 2.0, 0.0, 1.0));
            }
            if (code > 1u)
            {
                xcov -= clamp(r.y + 0.5, 0.0, 1.0);
                xwgt = max(xwgt, clamp(1.0 - abs(r.y) * 2.0, 0.0, 1.0));
            }
        }
    }

    // Vertical bands -- headers at glyphLoc + 2*IS + numHBands + bandX.
    float ycov = 0.0, ywgt = 0.0;
    uvec2 vbandData = texelFetch(
        u_slugBandTexture[nonuniformEXT(atlasIndex)],
        ivec2(glyphLoc.x + 2 * SLUG_INDIRECTION_SIZE + bandMax.y + 1 + bandX, glyphLoc.y), 0).xy;
    ivec2 vbandLoc = slug_CalcBandLoc(glyphLoc, vbandData.y, textureWidthLog2);

    for (int ci = 0; ci < int(vbandData.x); ++ci)
    {
        ivec2 curveLoc = ivec2(texelFetch(
            u_slugBandTexture[nonuniformEXT(atlasIndex)], ivec2(vbandLoc.x + ci, vbandLoc.y), 0).xy);
        vec4 p12 = texelFetch(u_slugCurveTexture[nonuniformEXT(atlasIndex)], curveLoc, 0) - vec4(renderCoord, renderCoord);
        vec2 p3 = texelFetch(u_slugCurveTexture[nonuniformEXT(atlasIndex)], ivec2(curveLoc.x + 1, curveLoc.y), 0).xy - renderCoord;

        if (max(max(p12.y, p12.w), p3.y) * pixelsPerEm.y < -0.5) break;

        uint code = slug_CalcRootCode(p12.x, p12.z, p3.x);
        if (code != 0u)
        {
            vec2 r = slug_SolveVertPoly(p12, p3) * pixelsPerEm.y;
            if ((code & 1u) != 0u)
            {
                ycov -= clamp(r.x + 0.5, 0.0, 1.0);
                ywgt = max(ywgt, clamp(1.0 - abs(r.x) * 2.0, 0.0, 1.0));
            }
            if (code > 1u)
            {
                ycov += clamp(r.y + 0.5, 0.0, 1.0);
                ywgt = max(ywgt, clamp(1.0 - abs(r.y) * 2.0, 0.0, 1.0));
            }
        }
    }

    return slug_CalcCoverage(xcov, ycov, xwgt, ywgt);
}

#endif // ROCKY_SLUG_GLSL
