/*
 * Slug path strokes: engine-neutral core.
 *
 * Used by the separate Slug pass. Mirrors osgSlug's PathDrawable Sluggit mode (mitered
 * per-segment quads, round/bevel/miter joins, butt/round/square caps, closed-form unit-square
 * coverage) as functions of em-space points alone: no engine bindings, buffers or uniforms. Each
 * engine supplies the glue (where points come from, how a quad reaches the screen, where a
 * fragment's em position comes from).
 *
 * Every segment's quad ends on its joints' bisectors, so neighboring quads share that edge and a
 * stroke point belongs to exactly one segment; a round or bevel join piece lies inside the miter
 * wedge and is cut out per fragment. Past the miter limit (a joint "extended"), a segment's end
 * instead stops square one half width past the joint, and the two segments overlap there.
 *
 * The coverage functions need slug_CalcCoverage() and are compiled only after rocky.slug.glsl.
 */
#ifndef ROCKY_SLUG_PATH_GLSL
#define ROCKY_SLUG_PATH_GLSL

// slughorn::canvas::LineJoin / LineCap as int.
const int SLUG_JOIN_MITER = 0;
const int SLUG_JOIN_BEVEL = 1;
const int SLUG_JOIN_ROUND = 2;

const int SLUG_CAP_BUTT = 0;
const int SLUG_CAP_ROUND = 1;
const int SLUG_CAP_SQUARE = 2;

// Miter ratio past which a non-miter join (whose piece lies inside the miter wedge) extends
// instead: the wedge's tip would be too far away to rasterize.
const float SLUG_PATH_MAX_MITER = 4.0;

// An end distance for a segment end that is neither a cap nor a join piece: beyond any half width.
const float SLUG_PATH_NO_END = 1.0e6;

vec2 slug_PathPerp(vec2 dir)
{
    return vec2(-dir.y, dir.x);
}

// A segment shorter than this has no usable direction (typically a repeated point).
bool slug_PathIsShort(vec2 v, float halfWidth)
{
    float eps = 1.0e-6 * halfWidth;
    return dot(v, v) <= eps * eps;
}

// The joint at p, between the segments (prev, p) and (p, next). Both segments evaluate it from the
// same points with the same expressions, so they agree on it bit for bit.
struct slug_PathJoint
{
    vec2 miter;    // unit miter direction toward the +perp side of both segments
    vec2 outer;    // unit direction toward the turn's outer side (the incoming direction at a full reversal)
    float cosHalf; // cos(half the turn angle); 0 at a full reversal
    bool extended; // miter ratio 1 / cosHalf past the join's limit
    int clip;      // join piece cut per fragment: -1 (none, a plain miter), SLUG_JOIN_BEVEL or SLUG_JOIN_ROUND
};

slug_PathJoint slug_PathJointAt(vec2 prev, vec2 p, vec2 next, float halfWidth, int join, float miterLimit)
{
    vec2 vIn = p - prev;
    vec2 vOut = next - p;

    slug_PathJoint j;

    // Next to a zero-length segment the turn is unknown: round joint whatever the turn.
    if (slug_PathIsShort(vIn, halfWidth) || slug_PathIsShort(vOut, halfWidth))
    {
        j.miter = vec2(0.0);
        j.outer = vec2(0.0);
        j.cosHalf = 0.0;
        j.extended = true;
        j.clip = SLUG_JOIN_ROUND;
        return j;
    }

    vec2 dirIn = normalize(vIn);
    vec2 dirOut = normalize(vOut);
    vec2 sum = slug_PathPerp(dirIn) + slug_PathPerp(dirOut);

    j.miter = dot(sum, sum) > 1.0e-12 ? normalize(sum) : vec2(0.0);
    j.cosHalf = max(dot(j.miter, slug_PathPerp(dirIn)), 0.0);

    // A left turn (positive cross product) bends toward +perp, so its outer side is -perp.
    float turn = dirIn.x * dirOut.y - dirIn.y * dirOut.x;
    j.outer = j.cosHalf > 0.0 ? j.miter * (turn > 0.0 ? -1.0 : 1.0) : dirIn;

    float limit = join == SLUG_JOIN_MITER ? miterLimit : SLUG_PATH_MAX_MITER;
    j.extended = j.cosHalf * limit < 1.0;
    j.clip = join == SLUG_JOIN_MITER ? (j.extended ? SLUG_JOIN_BEVEL : -1) : join;
    return j;
}

// The joint state of an end without a neighbor segment (never read for it).
const slug_PathJoint SLUG_PATH_NO_JOINT = slug_PathJoint(vec2(0.0), vec2(0.0), 0.0, true, -1);

// The path's direction through the joint at p: the normal of the bisector line the two segments'
// quads share there. A point x lies on the outgoing segment's side when dot(x - p, axis) >= 0.
// Evaluated from the same points by both segments, so they split the joint bit for bit alike.
vec2 slug_PathJointAxis(vec2 prev, vec2 p, vec2 next)
{
    vec2 sum = normalize(p - prev) + normalize(next - p);
    return dot(sum, sum) > 1.0e-12 ? normalize(sum) : vec2(0.0);
}

// The offset from the centerline point mix(p0, p1, s.x) to the segment quad's corner s (s.x 0 at
// p0, 1 at p1; s.y -1 or +1 across). A joined, non-extended end's corners sit on the joint's
// bisector; an extended end, or a round or square cap, runs one half width past its point.
vec2 slug_PathCornerOffset(
    vec2 p0,
    vec2 p1,
    bool hasPrev,
    slug_PathJoint startJoint,
    bool hasNext,
    slug_PathJoint endJoint,
    vec2 s,
    float halfWidth,
    int cap
    )
{
    vec2 dir = normalize(p1 - p0);
    vec2 perp = slug_PathPerp(dir);
    bool atEnd = s.x > 0.5;
    float along = atEnd ? halfWidth : -halfWidth;

    if (atEnd ? hasNext : hasPrev)
    {
        slug_PathJoint j = atEnd ? endJoint : startJoint;
        if (j.extended)
            return perp * (halfWidth * s.y) + dir * along;
        return j.miter * (halfWidth / j.cosHalf * s.y);
    }

    vec2 offset = perp * (halfWidth * s.y);
    if (cap != SLUG_CAP_BUTT)
        offset += dir * along;
    return offset;
}

// A joint's join piece as (outer.xy, cosHalf, clip); clip -1 for none. A round cap end is the round
// join's disc alone.
vec4 slug_PathEndJoin(bool joined, slug_PathJoint j, int cap)
{
    if (joined)
        return vec4(j.outer, j.cosHalf, float(j.clip));
    return vec4(0.0, 0.0, 0.0, cap == SLUG_CAP_ROUND ? float(SLUG_JOIN_ROUND) : -1.0);
}

// Inside distance (positive inside) of one segment end: dist (along the segment, from the end's
// boundary) unioned with its join piece when join.w is a bevel (1) or round (2) join. rel is the
// point's offset from the end's point.
float slug_PathEndInside(float dist, vec2 rel, vec4 join, float halfWidth)
{
    if (join.w < 0.0)
        return dist;

    float d = length(rel) - halfWidth;
    if (join.w < 1.5)
        d = max(d, dot(rel, join.xy) - halfWidth * join.z);
    return max(dist, -d);
}

// A point's two end distances and its signed distance across the centerline, in the path's em
// units. An end that is neither a cap nor a join piece (a plain miter joint) gets SLUG_PATH_NO_END.
struct slug_PathSample
{
    float startIn;
    float endIn;
    float across;
};

slug_PathSample slug_PathSampleAt(
    vec2 p,
    vec2 p0,
    vec2 p1,
    bool hasPrev,
    vec4 startJoin,
    bool hasNext,
    vec4 endJoin,
    float halfWidth,
    int cap
    )
{
    vec2 dir = normalize(p1 - p0);
    float capReach = cap == SLUG_CAP_SQUARE ? halfWidth : 0.0;
    float startDist = dot(p - p0, dir);
    float endDist = dot(p1 - p, dir);

    // A cap's boundary is its point (a square cap's, one half width past it); a joint with a join
    // piece ends the body square at the joint; a plain miter joint has no boundary.
    startDist = !hasPrev ? startDist + capReach : (startJoin.w >= 0.0 ? startDist : SLUG_PATH_NO_END);
    endDist = !hasNext ? endDist + capReach : (endJoin.w >= 0.0 ? endDist : SLUG_PATH_NO_END);

    slug_PathSample s;
    s.startIn = slug_PathEndInside(startDist, p - p0, startJoin, halfWidth);
    s.endIn = slug_PathEndInside(endDist, p - p1, endJoin, halfWidth);
    s.across = dot(p - p0, slug_PathPerp(dir));
    return s;
}

#ifdef ROCKY_SLUG_GLSL
// Unit-square coverage, as osgSlug's slug_RenderUnitSquare(): slug_Render()'s band loop evaluated in
// closed form for the square (0,0)-(1,1), whose four straight edges need no curve fetches.
float slug_RenderUnitSquare(vec2 renderCoord, vec2 pixelsPerEm)
{
    vec2 r0 = -renderCoord * pixelsPerEm;
    vec2 r1 = (1.0 - renderCoord) * pixelsPerEm;
    vec2 cov = clamp(r1 + 0.5, 0.0, 1.0) - clamp(r0 + 0.5, 0.0, 1.0);
    vec2 wgt = max(clamp(1.0 - abs(r0) * 2.0, 0.0, 1.0), clamp(1.0 - abs(r1) * 2.0, 0.0, 1.0));

    float hasX = step(0.0, renderCoord.y) * step(renderCoord.y, 1.0);
    float hasY = step(0.0, renderCoord.x) * step(renderCoord.x, 1.0);

    return slug_CalcCoverage(cov.x * hasX, cov.y * hasY, wgt.x * hasX, wgt.y * hasY);
}

// Stroke coverage from a sample and the screen rates (per pixel) of its three distances, mapping the
// stroke onto the unit square: across spans the width, and the nearer end's inside distance gives
// the square's X, clamped at its midpoint (a point far from both ends reads the center column).
float slug_PathCoverage(slug_PathSample s, vec3 rates, float halfWidth)
{
    float emPerUnit = 1.0 / (2.0 * halfWidth);
    bool fromStart = s.startIn < s.endIn;
    float inside = fromStart ? s.startIn : s.endIn;
    float x = min(inside * emPerUnit, 0.5);
    x = fromStart ? x : 1.0 - x;
    float y = s.across * emPerUnit + 0.5;

    // A constant (SLUG_PATH_NO_END) end has zero rate; x then sits at the midpoint and its rate
    // only needs to be finite.
    float rateX = fromStart ? rates.x : rates.y;
    rateX = rateX > 0.0 ? rateX : rates.z;

    return slug_RenderUnitSquare(vec2(x, y), 1.0 / max(vec2(rateX, rates.z) * emPerUnit, vec2(1e-30)));
}
#endif // ROCKY_SLUG_GLSL

#endif // ROCKY_SLUG_PATH_GLSL
