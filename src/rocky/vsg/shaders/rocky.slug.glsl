/*
 * Slug analytic coverage evaluator.
 *
 * Adapted from example/slughorn-example-glfw.cpp in Slughorn commit
 * 312ef217aaf6b1c47b05ba7575342b513daa830d.
 * Atlas addressing updated for the packed, row-spanning layout in commit
 * 1b203c191bd152c20685f8a896d22dcff974cec4.
 * Root solver and centerline strokes match commit
 * 1e09656e5fe2da7a24c164fe8697187ebf28f673.
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

// Roots t1 = (b - d) / a and t2 = (b + d) / a of a*t^2 - 2*b*t + c. Computed directly, the root
// whose numerator subtracts nearly equal values loses every significant digit once |a| is small
// next to |b| - a nearly straight curve, or a straight edge whose float32 midpoint control point
// leaves a at one ulp of the coordinate magnitude (2^-16 at 128). That root is taken in its equal
// form c / (b + d) or c / (b - d) instead: q = b + sign(b) * d never cancels, so the roots are
// c / q and q / a, continuous across the callers' linear fallback. A clamped (negative)
// discriminant keeps both roots at b / a so a grazing curve's paired contributions still cancel.
// Mirrors slughorn::render::Sampler::_solveRoots().
vec2 slug_SolveRoots(float a, float b, float c)
{
    float disc = b * b - a * c;
    float d = sqrt(max(disc, 0.0));
    float q = b >= 0.0 ? b + d : b - d;
    float tFar = q / a;
    float tNear = disc > 0.0 ? c / q : tFar;
    return b >= 0.0 ? vec2(tNear, tFar) : vec2(tFar, tNear);
}

vec2 slug_SolveHorizPoly(vec4 p12, vec2 p3)
{
    vec2 a = p12.xy - p12.zw * 2.0 + p3;
    vec2 b = p12.xy - p12.zw;
    float t1, t2;
    if (abs(a.y) < 1.0 / 65536.0)
    {
        // Straight and axis-degenerate curves dominate stroked line work. Take
        // the linear root before issuing the quadratic reciprocal and sqrt.
        t1 = p12.y * (0.5 / b.y);
        t2 = t1;
    }
    else
    {
        vec2 t = slug_SolveRoots(a.y, b.y, p12.y);
        t1 = t.x;
        t2 = t.y;
    }
    return vec2(
        (a.x * t1 - b.x * 2.0) * t1 + p12.x,
        (a.x * t2 - b.x * 2.0) * t2 + p12.x);
}

vec2 slug_SolveVertPoly(vec4 p12, vec2 p3)
{
    vec2 a = p12.xy - p12.zw * 2.0 + p3;
    vec2 b = p12.xy - p12.zw;
    float t1, t2;
    if (abs(a.x) < 1.0 / 65536.0)
    {
        t1 = p12.x * (0.5 / b.x);
        t2 = t1;
    }
    else
    {
        vec2 t = slug_SolveRoots(a.x, b.x, p12.x);
        t1 = t.x;
        t2 = t.y;
    }
    return vec2(
        (a.y * t1 - b.y * 2.0) * t1 + p12.y,
        (a.y * t2 - b.y * 2.0) * t2 + p12.y);
}

// Both textures use linear addressing: band lists and curve pairs can cross
// rows. Curve starts need not be even-aligned because adjacent curves share
// their endpoint texel. Only the indirection/header block stays on one row.
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
    sampler2D curveTexture,
    usampler2D bandTexture,
    int textureWidthLog2
    )
{
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

    // Keep the reciprocal behind the inexpensive carrier rejection above.
    vec2 pixelsPerEm = 1.0 / emsPerPixel;

    // O(1) band index via indirection tables (2 fetches per axis).
    int qY = clamp(int(bandCoord.y), 0, SLUG_INDIRECTION_SIZE - 1);
    int qX = clamp(int(bandCoord.x), 0, SLUG_INDIRECTION_SIZE - 1);
    int bandY = int(texelFetch(bandTexture, ivec2(glyphLoc.x + qY, glyphLoc.y), 0).r);
    int bandX = int(texelFetch(bandTexture, ivec2(glyphLoc.x + SLUG_INDIRECTION_SIZE + qX, glyphLoc.y), 0).r);

    // Horizontal bands -- headers at glyphLoc + 2*IS + bandY.
    float xcov = 0.0, xwgt = 0.0;
    uvec2 hbandData = texelFetch(
        bandTexture,
        ivec2(glyphLoc.x + 2 * SLUG_INDIRECTION_SIZE + bandY, glyphLoc.y), 0).xy;
    ivec2 hbandLoc = slug_CalcBandLoc(glyphLoc, hbandData.y, textureWidthLog2);

    for (int ci = 0; ci < int(hbandData.x); ++ci)
    {
        ivec2 curveLoc = ivec2(texelFetch(
            bandTexture, slug_CalcBandLoc(hbandLoc, uint(ci), textureWidthLog2), 0).xy);
        vec4 p12 = texelFetch(curveTexture, curveLoc, 0) - vec4(renderCoord, renderCoord);
        vec2 p3 = texelFetch(curveTexture, slug_CalcBandLoc(curveLoc, 1u, textureWidthLog2), 0).xy - renderCoord;

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
        bandTexture,
        ivec2(glyphLoc.x + 2 * SLUG_INDIRECTION_SIZE + bandMax.y + 1 + bandX, glyphLoc.y), 0).xy;
    ivec2 vbandLoc = slug_CalcBandLoc(glyphLoc, vbandData.y, textureWidthLog2);

    for (int ci = 0; ci < int(vbandData.x); ++ci)
    {
        ivec2 curveLoc = ivec2(texelFetch(
            bandTexture, slug_CalcBandLoc(vbandLoc, uint(ci), textureWidthLog2), 0).xy);
        vec4 p12 = texelFetch(curveTexture, curveLoc, 0) - vec4(renderCoord, renderCoord);
        vec2 p3 = texelFetch(curveTexture, slug_CalcBandLoc(curveLoc, 1u, textureWidthLog2), 0).xy - renderCoord;

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

// Distance from the origin to B(t) = p1 + 2t * a + t^2 * c.
float slug_DistanceAt(vec2 p1, vec2 a, vec2 c, float t)
{
    return length(p1 + t * (2.0 * a + t * c));
}

// Distance from the origin to the quadratic p1, p2, p3 (points already relative to the pixel).
// The nearest point solves B(t) . B'(t) = 0:
//
//     (c . c) t^3 + 3 (a . c) t^2 + (2 (a . a) + p1 . c) t + p1 . a = 0
//
// with a = p2 - p1 and c = p1 - 2 * p2 + p3. Its real roots clamped to [0, 1], plus both
// endpoints (round caps), are the candidates. Mirrors slughorn::render::distanceToCurve().
float slug_DistanceToCurve(vec2 p1, vec2 p2, vec2 p3)
{
    vec2 a = p2 - p1;
    vec2 c = p1 - 2.0 * p2 + p3;
    float cc = dot(c, c);
    float aa = dot(a, a);

    // Degenerate: a segment p1 -> p1 + 2a (or a single point when a is also zero).
    if (cc <= 1e-12 * aa || cc == 0.0)
    {
        vec2 d = 2.0 * a;
        float len2 = dot(d, d);
        float t = len2 > 0.0 ? clamp(-dot(p1, d) / len2, 0.0, 1.0) : 0.0;
        return length(p1 + t * d);
    }

    // Monic cubic t^3 + ma t^2 + mb t + mk, then depressed with t = u - ma / 3: u^3 + p u + q = 0.
    float ma = 3.0 * dot(a, c) / cc;
    float mb = (2.0 * aa + dot(p1, c)) / cc;
    float mk = dot(p1, a) / cc;
    float p = mb - ma * ma / 3.0;
    float q = 2.0 * ma * ma * ma / 27.0 - ma * mb / 3.0 + mk;
    float shift = -ma / 3.0;
    float disc = q * q / 4.0 + p * p * p / 27.0;
    float best = min(slug_DistanceAt(p1, a, c, 0.0), slug_DistanceAt(p1, a, c, 1.0));

    if (disc > 0.0)
    {
        // One real root. The cube root takes the sign that adds like signs (no cancellation); the
        // other term is -p / (3 * w).
        float s = sqrt(disc);
        float h = q >= 0.0 ? -q / 2.0 - s : -q / 2.0 + s;
        float w = sign(h) * pow(abs(h), 1.0 / 3.0);
        float u = w != 0.0 ? w - p / (3.0 * w) : 0.0;
        best = min(best, slug_DistanceAt(p1, a, c, clamp(u + shift, 0.0, 1.0)));
    }
    else
    {
        // Three real roots (p <= 0): the trigonometric form.
        float m = 2.0 * sqrt(max(-p / 3.0, 0.0));
        float arg = m > 0.0 ? clamp(3.0 * q / (p * m), -1.0, 1.0) : 0.0;
        float theta = acos(arg) / 3.0;
        const float third = 2.0943951; // 2 * pi / 3
        for (int i = 0; i < 3; ++i)
        {
            float u = m * cos(theta - float(i) * third);
            best = min(best, slug_DistanceAt(p1, a, c, clamp(u + shift, 0.0, 1.0)));
        }
    }

    return best;
}

// A stroke shape's curves are a centerline; a point is covered within halfWidth em of the nearest
// one. Coverage ramps linearly over one pixel centered on that boundary, the pixel's em size taken
// as the mean of its two axes.
//
// The builder files each curve in every band within halfWidth of it, so the pixel's h-band and its
// v-band each hold every curve that can reach it, and the shorter list is walked. Lists are sorted
// by descending max x (h-bands) / max y (v-bands), so once a curve's max is farther behind the
// pixel than halfWidth plus a pixel, no later curve can reach it either.
//
// The ramp reaches half a pixel past halfWidth but the band padding is only halfWidth, so every band
// of the walked axis within half a pixel of the point is walked; a curve seen twice cannot change
// the minimum. Mirrors slughorn::render::Sampler::_renderStroke().
float slug_RenderStroke(
    vec2 renderCoord,
    vec2 emsPerPixel,
    vec4 bandTransform,
    ivec2 glyphLoc,
    ivec2 bandMax,
    sampler2D curveTexture,
    usampler2D bandTexture,
    int textureWidthLog2,
    float halfWidth
    )
{
    vec2 bandCoord = renderCoord * bandTransform.xy + bandTransform.zw;
    vec2 bandMargin = 0.5 * abs(emsPerPixel * bandTransform.xy);
    if (any(lessThan(bandCoord, -bandMargin)) ||
        any(greaterThan(
            bandCoord,
            vec2(float(SLUG_INDIRECTION_SIZE)) + bandMargin)))
    {
        return 0.0;
    }

    // emsPerPixel is per axis (|dx| + |dy|); exact for an axis-aligned mapping.
    float emPerPixel = max(0.5 * (emsPerPixel.x + emsPerPixel.y), 1e-12);

    int qY = clamp(int(bandCoord.y), 0, SLUG_INDIRECTION_SIZE - 1);
    int qX = clamp(int(bandCoord.x), 0, SLUG_INDIRECTION_SIZE - 1);
    int bandY = int(texelFetch(bandTexture, ivec2(glyphLoc.x + qY, glyphLoc.y), 0).r);
    int bandX = int(texelFetch(bandTexture, ivec2(glyphLoc.x + SLUG_INDIRECTION_SIZE + qX, glyphLoc.y), 0).r);
    int hHeaders = glyphLoc.x + 2 * SLUG_INDIRECTION_SIZE;
    int vHeaders = hHeaders + bandMax.y + 1;
    uint hcount = texelFetch(bandTexture, ivec2(hHeaders + bandY, glyphLoc.y), 0).x;
    uint vcount = texelFetch(bandTexture, ivec2(vHeaders + bandX, glyphLoc.y), 0).x;
    bool useH = hcount <= vcount;

    // The band axis is perpendicular to the sort axis: h-bands slice y, v-bands slice x.
    float coord = useH ? renderCoord.y : renderCoord.x;
    float bandScale = useH ? bandTransform.y : bandTransform.x;
    float bandOffset = useH ? bandTransform.w : bandTransform.z;
    int indir = glyphLoc.x + (useH ? 0 : SLUG_INDIRECTION_SIZE);
    int headers = useH ? hHeaders : vHeaders;
    float fringe = 0.5 * emPerPixel;
    int qFirst = clamp(int((coord - fringe) * bandScale + bandOffset), 0, SLUG_INDIRECTION_SIZE - 1);
    int qLast = clamp(int((coord + fringe) * bandScale + bandOffset), 0, SLUG_INDIRECTION_SIZE - 1);
    int first = int(texelFetch(bandTexture, ivec2(indir + qFirst, glyphLoc.y), 0).r);
    int last = int(texelFetch(bandTexture, ivec2(indir + qLast, glyphLoc.y), 0).r);

    // Curve points are taken relative to the pixel, so the cutoff is relative too.
    float cutoff = -halfWidth - emPerPixel;
    float d = 1e30;

    for (int band = first; band <= last; ++band)
    {
        uvec2 bandData = texelFetch(bandTexture, ivec2(headers + band, glyphLoc.y), 0).xy;
        ivec2 bandLoc = slug_CalcBandLoc(glyphLoc, bandData.y, textureWidthLog2);

        for (int ci = 0; ci < int(bandData.x); ++ci)
        {
            ivec2 curveLoc = ivec2(texelFetch(
                bandTexture, slug_CalcBandLoc(bandLoc, uint(ci), textureWidthLog2), 0).xy);
            vec4 p12 = texelFetch(curveTexture, curveLoc, 0) - vec4(renderCoord, renderCoord);
            vec2 p3 = texelFetch(
                curveTexture, slug_CalcBandLoc(curveLoc, 1u, textureWidthLog2), 0).xy - renderCoord;

            float maxCoord = useH ? max(max(p12.x, p12.z), p3.x) : max(max(p12.y, p12.w), p3.y);
            if (maxCoord < cutoff) break;

            d = min(d, slug_DistanceToCurve(p12.xy, p12.zw, p3));
        }
    }

    return clamp((halfWidth - d) / emPerPixel + 0.5, 0.0, 1.0);
}

#endif // ROCKY_SLUG_GLSL
