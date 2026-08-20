#pragma once

// ================================================================================================
// canvas.hpp - HTML Canvas-style drawing context for slughorn
//
// Two cooperating types:
//
// Path - owns all geometry state and authoring verbs (moveTo / lineTo / arc / strokePath / etc.)
// plus the arc-length sampling API (sample / arcLength). Can be built standalone and passed
// explicitly to Canvas commit verbs, or used implicitly through the Canvas forwarding API.
// Equivalent to HTML Canvas Path2D.
//
// Canvas - stateful CompositeShape builder. Holds one implicit Path internally and forwards the
// full authoring API to it. Commit verbs (fill / stroke / defineShape / fillGradient) accept either
// the implicit path or an explicit Path argument.
//
// IMPLICIT PATH (existing style - unchanged from caller's perspective):
//
// canvas.beginPath();
// canvas.moveTo(...); canvas.lineTo(...);
// canvas.fill(color);
//
// EXPLICIT PATH (new Path2D style):
//
// slughorn::canvas::Path p;
// p.moveTo(...); p.lineTo(...);
// canvas.fill(p, color); // p is unchanged; can be reused
// canvas.stroke(p, w, color);
//
// PATH SAMPLING:
//
// auto s = p.sample(0.5); // Sample { x, y, angle } at arc-length midpoint
// auto snap = canvas.path(); // copy of internal path; sample or continue building
//
// SCALE CONTRACT
// The `scale` parameter to fill() / defineShape() is the backend normalization scale; it
// converts authoring-space coordinates into slughorn em-space [0,1]. It is NEVER stored in
// Layer::scale (reserved for FreeType2). Leave at 1.0 for all geometry authored here.
//
// No implementation guard needed. Pure C++ header, no external dependencies.
// ================================================================================================

#include "slughorn.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>

namespace slughorn {
namespace canvas {

// ================================================================================================
// detail::flattenCurve - shared by Path::strokePath and Path::_rebuildLUT
// ================================================================================================

namespace detail {

inline void flattenCurve(
	slug_t p0x, slug_t p0y,
	slug_t p1x, slug_t p1y,
	slug_t p2x, slug_t p2y,
	slug_t tol, size_t depth,
	std::vector<std::pair<slug_t, slug_t>>& pts
) {
	const slug_t dx = p2x - p0x, dy = p2y - p0y;
	const slug_t lenSq = dx * dx + dy * dy;
	const slug_t cross = (p1x - p0x) * dy - (p1y - p0y) * dx;

	if(lenSq < 1e-12_cv || (cross * cross) <= (tol * tol * lenSq) || depth >= 8) {
		pts.push_back({p2x, p2y});

		return;
	}

	const slug_t m01x = (p0x + p1x) * 0.5_cv, m01y = (p0y + p1y) * 0.5_cv;
	const slug_t m12x = (p1x + p2x) * 0.5_cv, m12y = (p1y + p2y) * 0.5_cv;
	const slug_t mx = (m01x + m12x) * 0.5_cv, my = (m01y + m12y) * 0.5_cv;

	flattenCurve(p0x, p0y, m01x, m01y, mx, my, tol, depth + 1, pts);
	flattenCurve(mx, my, m12x, m12y, p2x, p2y, tol, depth + 1, pts);
}

}

// ================================================================================================
// Stroke join/cap styles - matches SVG/Canvas2D stroke-linejoin/stroke-linecap semantics.
//
// LineJoin::Miter mitres every corner whose miter ratio stays within miterLimit (strokePath()'s
// own parameter, SVG default 4.0); any corner that exceeds it falls back to Bevel for that corner
// only. Bevel/Round ignore miterLimit and apply unconditionally to every corner.
// ================================================================================================

enum class LineJoin { Miter, Bevel, Round };
enum class LineCap { Butt, Round, Square };

// ================================================================================================
// Path
// ================================================================================================

class Canvas;

class Path {
public:
	struct Sample { slug_t x, y, angle; };

	friend std::ostream& operator<<(std::ostream& os, const Sample& s);
	friend std::ostream& operator<<(std::ostream& os, const Path& p);

	Path(): _decomposer(_activeCurves) {}

	// `curves` carries no subpath-boundary information (e.g. a raw numpy-array buffer from the
	// Python Path(curves) binding) -- _pendingSubpathsUntracked stays true for the life of this
	// Path (and contaminates any Path that later addPath()'s from it, see addPath()'s contagion
	// rule), so strokePath()/pendingContourStarts() fall back to the coordinate-gap heuristic.
	explicit Path(Atlas::Curves curves):
	_pendingCurves(std::move(curves)),
	_pendingSubpathsUntracked(true),
	_decomposer(_activeCurves) {}

	// Custom copy/move needed: CurveDecomposer.curves is a reference that must stay
	// bound to THIS object's _activeCurves, not the source's.

	Path(const Path& o):
	_activeCurves(o._activeCurves),
	_pendingCurves(o._pendingCurves),
	_activeSubpathStarts(o._activeSubpathStarts),
	_pendingSubpathStarts(o._pendingSubpathStarts),
	_pendingSubpathsUntracked(o._pendingSubpathsUntracked),
	_penX(o._penX), _penY(o._penY),
	_ctm(o._ctm), _ctmStack(o._ctmStack),
	_decomposer(_activeCurves) {
		_decomposer.tolerance = o._decomposer.tolerance;
		_decomposer._x = o._decomposer._x;
		_decomposer._y = o._decomposer._y;
		_decomposer._sx = o._decomposer._sx;
		_decomposer._sy = o._decomposer._sy;
	}

	Path& operator=(const Path& o) {
		if(this == &o) return *this;

		_activeCurves = o._activeCurves;
		_pendingCurves = o._pendingCurves;
		_activeSubpathStarts = o._activeSubpathStarts;
		_pendingSubpathStarts = o._pendingSubpathStarts;
		_pendingSubpathsUntracked = o._pendingSubpathsUntracked;
		_penX = o._penX; _penY = o._penY;
		_ctm = o._ctm; _ctmStack = o._ctmStack;
		_decomposer.tolerance = o._decomposer.tolerance;
		_decomposer._x = o._decomposer._x;
		_decomposer._y = o._decomposer._y;
		_decomposer._sx = o._decomposer._sx;
		_decomposer._sy = o._decomposer._sy;
		_lutDirty = true;

		return *this;
	}

	Path(Path&& o) noexcept:
	_activeCurves(std::move(o._activeCurves)),
	_pendingCurves(std::move(o._pendingCurves)),
	_activeSubpathStarts(std::move(o._activeSubpathStarts)),
	_pendingSubpathStarts(std::move(o._pendingSubpathStarts)),
	_pendingSubpathsUntracked(o._pendingSubpathsUntracked),
	_penX(o._penX), _penY(o._penY),
	_ctm(o._ctm), _ctmStack(std::move(o._ctmStack)),
	_decomposer(_activeCurves) {
		_decomposer.tolerance = o._decomposer.tolerance;
		_decomposer._x = o._decomposer._x;
		_decomposer._y = o._decomposer._y;
		_decomposer._sx = o._decomposer._sx;
		_decomposer._sy = o._decomposer._sy;
	}

	Path& operator=(Path&& o) noexcept {
		if(this == &o) return *this;

		_activeCurves = std::move(o._activeCurves);
		_pendingCurves = std::move(o._pendingCurves);
		_activeSubpathStarts = std::move(o._activeSubpathStarts);
		_pendingSubpathStarts = std::move(o._pendingSubpathStarts);
		_pendingSubpathsUntracked = o._pendingSubpathsUntracked;
		_penX = o._penX; _penY = o._penY;
		_ctm = o._ctm; _ctmStack = std::move(o._ctmStack);
		_decomposer.tolerance = o._decomposer.tolerance;
		_decomposer._x = o._decomposer._x;
		_decomposer._y = o._decomposer._y;
		_decomposer._sx = o._decomposer._sx;
		_decomposer._sy = o._decomposer._sy;
		_lutDirty = true;

		return *this;
	}

	// -------------------------------------------------------------------------
	// CurveDecomposer access
	// -------------------------------------------------------------------------

	CurveDecomposer& decomposer() { return _decomposer; }
	const CurveDecomposer& decomposer() const { return _decomposer; }

	// -------------------------------------------------------------------------
	// Path management
	// -------------------------------------------------------------------------

	// Reset all geometry state. CTM is intentionally NOT cleared - matches HTML Canvas
	// behavior where beginPath() does not affect the current transform.
	Path& clear() {
		_pendingCurves.clear();
		_activeCurves.clear();
		_pendingSubpathStarts.clear();
		_activeSubpathStarts.clear();
		_pendingSubpathsUntracked = false;

		_decomposer._x = _decomposer._y = 0_cv;
		_decomposer._sx = _decomposer._sy = 0_cv;
		_penX = _penY = 0_cv;
		_lutDirty = true;

		return *this;
	}

	// Append all curves from @p other into this path's pending accumulator, preserving subpath
	// boundaries (both other's already-closed subpaths AND its current in-progress one, if any) --
	// see _appendTrackedCurves()'s doc comment. Does not affect @p other.
	Path& addPath(const Path& other) {
		_appendTrackedCurves(other._pendingCurves, other._pendingSubpathStarts, [](const Atlas::Curve& c) { return c; });
		_appendTrackedCurves(other._activeCurves, other._activeSubpathStarts, [](const Atlas::Curve& c) { return c; });

		if(other._pendingSubpathsUntracked) _pendingSubpathsUntracked = true;

		_lutDirty = true;

		return *this;
	}

	// Append curves from @p other with each control point transformed by @p transform, preserving
	// subpath boundaries the same way as the untransformed overload above.
	// Matches HTML Canvas Path2D.addPath(path, DOMMatrix) semantics.
	Path& addPath(const Path& other, const slughorn::Matrix& transform) {
		auto xform = [&](const slughorn::Atlas::Curve& c) {
			slughorn::Atlas::Curve out;

			transform.apply(c.x1, c.y1, out.x1, out.y1);
			transform.apply(c.x2, c.y2, out.x2, out.y2);
			transform.apply(c.x3, c.y3, out.x3, out.y3);

			return out;
		};

		_appendTrackedCurves(other._pendingCurves, other._pendingSubpathStarts, xform);
		_appendTrackedCurves(other._activeCurves, other._activeSubpathStarts, xform);

		if(other._pendingSubpathsUntracked) _pendingSubpathsUntracked = true;

		_lutDirty = true;

		return *this;
	}

	// -------------------------------------------------------------------------
	// Transform stack
	//
	// CTM is applied to every path command (moveTo / lineTo / etc.) before
	// coordinates reach the decomposer. Geometry is baked in its transformed
	// state; the CTM does not affect already-accumulated curves.
	// -------------------------------------------------------------------------

	Path& save() { _ctmStack.push_back(_ctm); return *this; }

	Path& restore() {
		if(!_ctmStack.empty()) {
			_ctm = _ctmStack.back();
			_ctmStack.pop_back();
		}

		return *this;
	}

	Path& resetTransform() { _ctm = Matrix::identity(); return *this; }
	Path& setTransform(const Matrix& m) { _ctm = m; return *this; }
	Path& transform(const Matrix& m) { _ctm = _ctm * m; return *this; }

	Path& translate(slug_t tx, slug_t ty) {
		_ctm = _ctm * Matrix::translate(tx, ty);

		return *this;
	}

	Path& rotate(slug_t angle) {
		_ctm = _ctm * Matrix::rotate(angle);

		return *this;
	}

	Path& scale(slug_t sx, slug_t sy) {
		_ctm = _ctm * Matrix::scale(sx, sy);

		return *this;
	}

	// -------------------------------------------------------------------------
	// Path commands
	// -------------------------------------------------------------------------

	Path& moveTo(slug_t x, slug_t y) {
		// A moveTo() while _activeCurves already holds data starts a NEW subpath -- record where
		// it will begin (its first curve is about to be pushed at this exact index). An empty
		// _activeCurves (fresh path, or right after clear()/closePath()) needs no marker: that
		// subpath's start is the implicit index 0.
		if(!_activeCurves.empty()) _activeSubpathStarts.push_back(_activeCurves.size());

		_penX = x;
		_penY = y;

		slug_t tx, ty;

		_ctm.apply(x, y, tx, ty);

		_decomposer.moveTo(tx, ty);

		_lutDirty = true;

		return *this;
	}

	Path& lineTo(slug_t x, slug_t y) {
		_penX = x;
		_penY = y;

		slug_t tx, ty;

		_ctm.apply(x, y, tx, ty);
		_decomposer.lineTo(tx, ty);

		_lutDirty = true;

		return *this;
	}

	Path& quadTo(slug_t cx, slug_t cy, slug_t x, slug_t y) {
		_penX = x;
		_penY = y;

		slug_t tcx, tcy, tx, ty;

		_ctm.apply(cx, cy, tcx, tcy);
		_ctm.apply(x, y, tx, ty);

		_decomposer.quadTo(tcx, tcy, tx, ty);

		_lutDirty = true;

		return *this;
	}

	Path& bezierTo(slug_t c1x, slug_t c1y, slug_t c2x, slug_t c2y, slug_t x, slug_t y) {
		_penX = x;
		_penY = y;

		slug_t tc1x, tc1y, tc2x, tc2y, tx, ty;

		_ctm.apply(c1x, c1y, tc1x, tc1y);
		_ctm.apply(c2x, c2y, tc2x, tc2y);
		_ctm.apply(x, y, tx, ty);

		_decomposer.cubicTo(tc1x, tc1y, tc2x, tc2y, tx, ty);

		_lutDirty = true;

		return *this;
	}

	Path& closePath() {
		_decomposer.close();
		_flushActiveIntoPending();

		_lutDirty = true;

		return *this;
	}

	// -------------------------------------------------------------------------
	// Convenience shape helpers
	//
	// These do NOT reset path state. Call clear() / beginPath() first if you
	// want a fresh path. Multiple helpers can be chained after a single clear()
	// to build compound shapes (e.g. a rect with a circular hole).
	// -------------------------------------------------------------------------

	Path& rect(slug_t x, slug_t y, slug_t w, slug_t h) {
		moveTo(x, y);
		lineTo(x + w, y);
		lineTo(x + w, y + h);
		lineTo(x, y + h);

		return closePath();
	}

	Path& roundedRect(slug_t x, slug_t y, slug_t w, slug_t h, slug_t r) {
		r = std::min(r, std::min(w, h) * 0.5_cv);

		if(r <= 0_cv) return rect(x, y, w, h);

		const slug_t k = 0.5522847498_cv;
		const slug_t kr = k * r;

		moveTo(x + r, y);
		lineTo(x + w - r, y);
		bezierTo(x + w - r + kr, y, x + w, y + r - kr, x + w, y + r);
		lineTo(x + w, y + h - r);
		bezierTo(x + w, y + h - r + kr, x + w - r + kr, y + h, x + w - r, y + h);
		lineTo(x + r, y + h);
		bezierTo(x + r - kr, y + h, x, y + h - r + kr, x, y + h - r);
		lineTo(x, y + r);
		bezierTo(x, y + r - kr, x + r - kr, y, x + r, y);

		return closePath();
	}

	Path& circle(slug_t cx, slug_t cy, slug_t r) { return ellipse(cx, cy, r, r); }

	Path& ellipse(slug_t cx, slug_t cy, slug_t rx, slug_t ry) {
		const slug_t k = 0.5522847498_cv;
		const slug_t kx = k * rx;
		const slug_t ky = k * ry;

		moveTo(cx + rx, cy);
		bezierTo(cx + rx, cy + ky, cx + kx, cy + ry, cx, cy + ry);
		bezierTo(cx - kx, cy + ry, cx - rx, cy + ky, cx - rx, cy);
		bezierTo(cx - rx, cy - ky, cx - kx, cy - ry, cx, cy - ry);
		bezierTo(cx + kx, cy - ry, cx + rx, cy - ky, cx + rx, cy);

		return closePath();
	}

	// arc() does NOT call clear(). It appends to the current path, enabling composition
	// with lineTo() for pie slices, stadium shapes, etc.
	Path& arc(slug_t cx, slug_t cy, slug_t r, slug_t startAngle, slug_t endAngle, bool ccw=false) {
		slug_t sweep;

		if(ccw) {
			sweep = startAngle - endAngle;

			if(sweep < 0_cv) sweep += 2_cv * PI_CV;

			sweep = -sweep;
		}

		else {
			sweep = endAngle - startAngle;

			if(sweep < 0_cv) sweep += 2_cv * PI_CV;
		}

		_arcSegments(cx, cy, r, startAngle, sweep);

		return *this;
	}

	Path& arcTo(slug_t x1, slug_t y1, slug_t x2, slug_t y2, slug_t r) {
		if(r <= 0_cv) return lineTo(x1, y1);

		const slug_t p0x = _penX, p0y = _penY;
		const slug_t d0x = p0x - x1, d0y = p0y - y1;
		const slug_t d1x = x2 - x1, d1y = y2 - y1;
		const slug_t len0 = std::sqrt(d0x*d0x + d0y*d0y);
		const slug_t len1 = std::sqrt(d1x*d1x + d1y*d1y);

		if(len0 < 1e-6_cv || len1 < 1e-6_cv) return lineTo(x1, y1);

		const slug_t u0x = d0x / len0, u0y = d0y / len0;
		const slug_t u1x = d1x / len1, u1y = d1y / len1;
		const slug_t cross = u0x*u1y - u0y*u1x;

		if(std::abs(cross) < 1e-6_cv) return lineTo(x1, y1);

		const slug_t dot = u0x*u1x + u0y*u1y;
		const slug_t tanHalf = std::abs(cross) / (1_cv + dot);

		if(tanHalf < 1e-6_cv) return lineTo(x1, y1);

		const slug_t tangentDist = r / tanHalf;
		const slug_t t0x = x1 + u0x * tangentDist, t0y = y1 + u0y * tangentDist;
		const slug_t t1x = x1 + u1x * tangentDist, t1y = y1 + u1y * tangentDist;

		const slug_t sign = (cross > 0_cv) ? 1_cv : -1_cv;
		const slug_t cenX = t0x - sign * u0y * r;
		const slug_t cenY = t0y + sign * u0x * r;

		const slug_t a0 = std::atan2(t0y - cenY, t0x - cenX);
		const slug_t a1 = std::atan2(t1y - cenY, t1x - cenX);

		lineTo(t0x, t0y);
		arc(cenX, cenY, r, a0, a1, cross > 0_cv);

		return *this;
	}

	// -------------------------------------------------------------------------
	// Stroke expansion
	// -------------------------------------------------------------------------

	// @p cw - if false (default) the outline is appended CCW (filled area). If true the outline
	// is appended CW (subtracts coverage), enabling punch-outs when combined with a CCW outline
	// in the same beginPath() session.
	//
	// HTML Canvas semantics: a single stroke() operates on ALL subpaths accumulated since
	// beginPath(). Each moveTo() starts a new subpath; subpaths are stroked independently
	// and their outlines combined into one shape.
	// @p cw - if false (default) the outline is appended CCW (filled area). If true the outline
	// is appended CW (subtracts coverage), enabling punch-outs when combined with a CCW outline
	// in the same beginPath() session.
	//
	// HTML Canvas semantics: a single stroke() operates on ALL subpaths accumulated since
	// beginPath(). Each moveTo() starts a new subpath; subpaths are stroked independently
	// and their outlines combined into one shape. Two-segment interior corners get a real
	// Miter/Bevel/Round join per @p join (Miter falls back to Bevel past @p miterLimit, the SVG
	// default 4.0 -- Bevel/Round ignore miterLimit entirely); open subpath endpoints get a
	// Butt/Round/Square cap per @p cap.
	//
	// Independent butt-capped subpaths that share an endpoint leave an uncovered "star" vertex.
	// A separate additive pass (search "star vertex" below) patches those with a round disc.
	// Round and square caps already contain the complete endpoint disc, so adding another contour
	// for them would be geometrically redundant and can amplify analytic coverage where the
	// coincident boundaries meet.
	bool strokePath(
		slug_t width, bool cw=false,
		LineJoin join=LineJoin::Miter, LineCap cap=LineCap::Butt, slug_t miterLimit=4_cv
	) {
		Atlas::Curves centerline;
		std::vector<size_t> starts; // resolved: index 0 always explicit, unlike the stored *SubpathStarts

		if(!_activeCurves.empty()) {
			starts = _resolveStarts(_activeCurves.size(), _activeSubpathStarts);
			centerline = std::move(_activeCurves);
			_activeSubpathStarts.clear();
		}

		else if(!_pendingCurves.empty()) {
			starts = _pendingSubpathsUntracked
				? std::vector<size_t>{}
				: _resolveStarts(_pendingCurves.size(), _pendingSubpathStarts)
			;

			// Untracked (a raw Path(Atlas::Curves) buffer -- e.g. the Python Path(curves)
			// binding -- or addPath() contagion from one): boundaries are genuinely unknown.
			// Fall back to the coordinate-gap heuristic this tracking mechanism otherwise
			// replaces (see Path::_pendingSubpathsUntracked's doc comment).
			if(starts.empty()) {
				starts.push_back(0);

				for(size_t i = 1; i < _pendingCurves.size(); i++) {
					const auto& prev = _pendingCurves[i - 1];
					const auto& cur = _pendingCurves[i];

					if(
						std::abs(cur.x1 - prev.x3) > 1e-6_cv ||
						std::abs(cur.y1 - prev.y3) > 1e-6_cv
					) starts.push_back(i);
				}
			}

			centerline = std::move(_pendingCurves);
			_pendingSubpathStarts.clear();
			_pendingSubpathsUntracked = false;
		}

		else return false;

		// Scale width by the CTM's pen scale so canvas.scale() affects stroke width.
		const slug_t xScale = std::sqrt(_ctm.xx * _ctm.xx + _ctm.yx * _ctm.yx);
		const slug_t yScale = std::sqrt(_ctm.xy * _ctm.xy + _ctm.yy * _ctm.yy);
		const slug_t penScale = (xScale > 0_cv && yScale > 0_cv) ? std::sqrt(xScale * yScale) : 1_cv;
		const slug_t h = width * penScale * 0.5_cv;
		const slug_t tol = _decomposer.tolerance < TOLERANCE_EXACT
			? _decomposer.tolerance
			: std::max(TOLERANCE_BALANCED, h * 0.1_cv)
		;

		enum class CornerKind { Miter, Bevel, Round };

		// Per-corner join data. innerNx/innerNy is the offset factor applied symmetrically
		// (+h on the left wall, -h on the right wall) exactly like the original single `pn`
		// value did -- correct on its own for Miter (both walls share one point), but for
		// Bevel/Round it's only correct for the INNER wall (unit bisector, never miter-scaled;
		// concave corners never need more, they undershoot by construction). The OUTER wall at
		// a Bevel/Round corner ignores innerNx/innerNy entirely and instead gets two raw
		// segN-offset points joined by an explicit bevel edge or arc -- see the per-segment loop.
		struct Corner {
			CornerKind kind = CornerKind::Miter;
			bool outerIsLeft = true;
			slug_t innerNx = 0_cv, innerNy = 0_cv;
		};

		bool any = false;

		// "Star" vertex tracking: a point where 2+ INDEPENDENT open subpaths in this call share
		// an endpoint (e.g. several disconnected 2-point edges meeting at one point) -- not a
		// 2-segment corner, undefined by any LineJoin style. Collected during the main loop below
		// (one arm per open subpath's true start/end), clustered and round-disc-patched after it.
		struct Arm { slug_t x, y; size_t subpathIndex; };

		std::vector<Arm> arms;

		for(size_t si = 0; si < starts.size(); si++) {
			const size_t begin = starts[si];
			const size_t end = (si + 1 < starts.size()) ? starts[si + 1] : centerline.size();

			if(begin >= end) continue;

			// Pass 1: flatten this subpath to a polyline.
			std::vector<std::pair<slug_t, slug_t>> pts;

			pts.push_back({centerline[begin].x1, centerline[begin].y1});

			for(size_t i = begin; i < end; i++) {
				const auto& c = centerline[i];

				detail::flattenCurve(c.x1, c.y1, c.x2, c.y2, c.x3, c.y3, tol, 0, pts);
			}

			if(pts.size() < 2) continue;

			const size_t numSegs = pts.size() - 1;

			// Pass 2a: per-segment perpendicular normals (left-hand: +90 deg from travel direction).
			std::vector<std::pair<slug_t, slug_t>> segN(numSegs);

			for(size_t i = 0; i < numSegs; i++) {
				const slug_t dx = pts[i + 1].first - pts[i].first;
				const slug_t dy = pts[i + 1].second - pts[i].second;
				const slug_t len = std::sqrt(dx * dx + dy * dy);

				segN[i] = len > 1e-9_cv
					? std::pair{-dy / len, dx / len}
					: std::pair{0_cv, 1_cv}
				;
			}

			const bool isClosed =
				std::abs(pts.back().first - pts.front().first) < 1e-6_cv &&
				std::abs(pts.back().second - pts.front().second) < 1e-6_cv
			;

			if(!isClosed) {
				arms.push_back({pts.front().first, pts.front().second, si});
				arms.push_back({pts.back().first, pts.back().second, si});
			}

			// Pass 2b: tag each interior corner. Corner i sits at pts[i], between incoming
			// segment `prev` and outgoing segment `cur`.
			auto calcCorner = [&](size_t prev, size_t cur) -> Corner {
				Corner c;

				slug_t bx = segN[prev].first + segN[cur].first;
				slug_t by = segN[prev].second + segN[cur].second;

				const slug_t blen = std::sqrt(bx * bx + by * by);

				if(blen > 1e-6_cv) {
					bx /= blen; by /= blen;

					// Sign of cross(segN[prev], segN[cur]) equals the sign of cross(dir[prev],
					// dir[cur]) (a shared 90 deg rotation preserves it) -- positive means a CCW/
					// left turn, whose convex/outer side is the RIGHT wall (verified against a
					// concrete CCW square-corner case: East-then-North turn gives turnSign=+1,
					// and a CCW-traced convex polygon's interior -- the concave side -- is by
					// definition on the left, so outer must be right).
					const slug_t turnSign = segN[prev].first * segN[cur].second - segN[prev].second * segN[cur].first;

					c.outerIsLeft = turnSign < 0_cv;

					const slug_t d = bx * segN[cur].first + by * segN[cur].second;
					slug_t m = 1_cv;
					bool exceeded = false;

					if(d > 1e-3_cv) {
						m = 1_cv / d;
						exceeded = m > miterLimit;
					}

					c.kind = (join == LineJoin::Miter && !exceeded) ? CornerKind::Miter
						: (join == LineJoin::Round ? CornerKind::Round : CornerKind::Bevel)
					;

					if(c.kind == CornerKind::Miter) { c.innerNx = bx * m; c.innerNy = by * m; }
					else { c.innerNx = bx; c.innerNy = by; } // unit bisector, no miter scaling
				}

				// Near-180-degree reversal: bisector degenerates, no real corner to bevel/round --
				// same fallback the original single-normal code used.
				else {
					c.kind = CornerKind::Miter;
					c.innerNx = segN[cur].first; c.innerNy = segN[cur].second;
				}

				return c;
			};

			std::vector<Corner> corners(pts.size());

			for(size_t i = 0; i < pts.size(); i++) {
				if(!i) corners[i] = isClosed
					? calcCorner(numSegs - 1, 0)
					: Corner{CornerKind::Miter, true, segN[0].first, segN[0].second}
				;

				else if(i == numSegs) corners[i] = isClosed
					? corners[0]
					: Corner{CornerKind::Miter, true, segN[numSegs - 1].first, segN[numSegs - 1].second}
				;

				else corners[i] = calcCorner(i - 1, i);
			}

			// Pass 3: build lwall / rwall as append-only curve lists -- a Bevel/Round corner
			// emits two outer-wall points + a connecting piece where a Miter corner emits one
			// shared point, so a fixed 1:1-with-pts index no longer holds.
			Atlas::Curves lwall, rwall;

			auto pushWallSegment = [](Atlas::Curves& wall, slug_t x0, slug_t y0, slug_t x2, slug_t y2) {
				wall.push_back({x0, y0, (x0 + x2) * 0.5_cv, (y0 + y2) * 0.5_cv, x2, y2});
			};

			for(size_t i = 0; i < numSegs; i++) {
				const slug_t p0x = pts[i].first, p0y = pts[i].second;
				const slug_t p2x = pts[i + 1].first, p2y = pts[i + 1].second;
				const Corner& c0 = corners[i];
				const Corner& c1 = corners[i + 1];

				slug_t l0x = p0x + h * c0.innerNx, l0y = p0y + h * c0.innerNy;
				slug_t r0x = p0x - h * c0.innerNx, r0y = p0y - h * c0.innerNy;

				// A Bevel/Round corner's OUTER side ignores the shared inner-wall point and
				// starts this segment at the raw, un-mitered pt + h*segN[i] instead -- matching
				// exactly where the join geometry (emitted below, while closing out the
				// PREVIOUS segment) left off.
				if(c0.kind != CornerKind::Miter) {
					if(c0.outerIsLeft) { l0x = p0x + h * segN[i].first; l0y = p0y + h * segN[i].second; }
					else { r0x = p0x - h * segN[i].first; r0y = p0y - h * segN[i].second; }
				}

				slug_t l2x = p2x + h * c1.innerNx, l2y = p2y + h * c1.innerNy;
				slug_t r2x = p2x - h * c1.innerNx, r2y = p2y - h * c1.innerNy;

				if(c1.kind != CornerKind::Miter) {
					if(c1.outerIsLeft) { l2x = p2x + h * segN[i].first; l2y = p2y + h * segN[i].second; }
					else { r2x = p2x - h * segN[i].first; r2y = p2y - h * segN[i].second; }
				}

				pushWallSegment(lwall, l0x, l0y, l2x, l2y);
				pushWallSegment(rwall, r0x, r0y, r2x, r2y);

				// Emit corner (i+1)'s join geometry on its outer wall, connecting this segment's
				// outer-wall end to the next segment's outer-wall start -- only for a real
				// interior corner (the subpath's own true end gets a cap instead, not a join;
				// corners[0]/corners[numSegs] on an open subpath are always Miter-kind by
				// construction above, so this never fires for them regardless of this guard).
				const bool hasNextSegment = (i + 1 < numSegs) || isClosed;

				if(hasNextSegment && c1.kind != CornerKind::Miter) {
					Atlas::Curves& outerWall = c1.outerIsLeft ? lwall : rwall;
					const slug_t sign = c1.outerIsLeft ? 1_cv : -1_cv;
					const size_t nextSeg = (i + 1 < numSegs) ? (i + 1) : 0;

					if(c1.kind == CornerKind::Bevel) {
						pushWallSegment(
							outerWall,
							p2x + h * sign * segN[i].first, p2y + h * sign * segN[i].second,
							p2x + h * sign * segN[nextSeg].first, p2y + h * sign * segN[nextSeg].second
						);
					}

					else { // Round
						const slug_t a0 = std::atan2(sign * segN[i].second, sign * segN[i].first);
						const slug_t a1 = std::atan2(sign * segN[nextSeg].second, sign * segN[nextSeg].first);
						const slug_t sweep = std::atan2(std::sin(a1 - a0), std::cos(a1 - a0));

						_appendArcChunks(outerWall, p2x, p2y, h, a0, sweep);
					}
				}
			}

			if(lwall.empty()) continue;

			// Caps: only for open subpaths -- closed subpaths keep the existing degenerate
			// zero-length "cap" curve at the wraparound point (harmless, unchanged).
			if(!isClosed && cap == LineCap::Square) {
				// Extend the terminal wall points outward along the path's own tangent
				// (derived from segN, avoids recomputing it) by h; the closure code below then
				// joins the (now-shifted) tips with a straight edge, same as Butt.
				const slug_t d0x = segN[0].second, d0y = -segN[0].first;
				const slug_t dNx = segN[numSegs - 1].second, dNy = -segN[numSegs - 1].first;

				rwall.back().x3 += h * dNx; rwall.back().y3 += h * dNy;
				lwall.back().x3 += h * dNx; lwall.back().y3 += h * dNy;
				lwall.front().x1 -= h * d0x; lwall.front().y1 -= h * d0y;
				rwall.front().x1 -= h * d0x; rwall.front().y1 -= h * d0y;
			}

			Atlas::Curves outline;

			for(const auto& r : rwall) outline.push_back(r);

			// End cap: rwall's terminal point -> lwall's terminal point.
			if(!isClosed && cap == LineCap::Round) {
				const slug_t a0 = std::atan2(-segN[numSegs - 1].second, -segN[numSegs - 1].first);

				_appendArcChunks(outline, pts[numSegs].first, pts[numSegs].second, h, a0, PI_CV);
			}

			else {
				const slug_t ax = rwall.back().x3, ay = rwall.back().y3;
				const slug_t bx = lwall.back().x3, by = lwall.back().y3;

				outline.push_back({ax, ay, (ax + bx) * 0.5_cv, (ay + by) * 0.5_cv, bx, by});
			}

			for(size_t i = lwall.size(); i-- > 0;) {
				const auto& l = lwall[i];

				outline.push_back({l.x3, l.y3, l.x2, l.y2, l.x1, l.y1});
			}

			// Start cap: lwall's start point -> rwall's start point.
			if(!isClosed && cap == LineCap::Round) {
				const slug_t a0 = std::atan2(segN[0].second, segN[0].first);

				_appendArcChunks(outline, pts[0].first, pts[0].second, h, a0, PI_CV);
			}

			else {
				const slug_t ax = lwall.front().x1, ay = lwall.front().y1;
				const slug_t bx = rwall.front().x1, by = rwall.front().y1;

				outline.push_back({ax, ay, (ax + bx) * 0.5_cv, (ay + by) * 0.5_cv, bx, by});
			}

			const size_t outlineStart = _pendingCurves.size();

			if(outlineStart > 0) _pendingSubpathStarts.push_back(outlineStart);

			for(const auto& c : outline) _pendingCurves.push_back(c);

			if(cw) CurveDecomposer::reverseCurves(_pendingCurves, outlineStart, _pendingCurves.size());

			any = true;
		}

		// Star-vertex patch: cluster collected arms by coincident location via spatial hash
		// bucketing (not naive O(n^2) -- a large MVT linestring batch could have hundreds of
		// subpaths per call), and round-disc any butt-capped cluster spanning 2+ DISTINCT
		// subpath indices (same-subpath self-touching is just that subpath's own cap, not a
		// star). Round and square caps already contain this disc; emitting it again adds a
		// redundant winding contour and can overstate analytic antialiasing at tangent edges.
		if(cap == LineCap::Butt && arms.size() >= 2) {
			const slug_t eps = std::max(1e-6_cv, h * 1e-4_cv);

			auto cellOf = [&](slug_t x, slug_t y) -> std::pair<int64_t, int64_t> {
				return {
					static_cast<int64_t>(std::floor(x / eps)),
					static_cast<int64_t>(std::floor(y / eps))
				};
			};

			struct CellHash {
				size_t operator()(const std::pair<int64_t, int64_t>& p) const {
					return std::hash<int64_t>()(p.first) ^ (std::hash<int64_t>()(p.second) * 0x9e3779b97f4a7c15ULL);
				}
			};

			std::unordered_map<std::pair<int64_t, int64_t>, std::vector<size_t>, CellHash> grid;

			for(size_t i = 0; i < arms.size(); i++) grid[cellOf(arms[i].x, arms[i].y)].push_back(i);

			std::vector<bool> visited(arms.size(), false);

			for(size_t i = 0; i < arms.size(); i++) {
				if(visited[i]) continue;

				std::vector<size_t> cluster;
				std::vector<size_t> stack{i};

				visited[i] = true;

				while(!stack.empty()) {
					const size_t cur = stack.back();

					stack.pop_back();
					cluster.push_back(cur);

					const auto [cx, cy] = cellOf(arms[cur].x, arms[cur].y);

					for(int64_t dx = -1; dx <= 1; dx++) {
						for(int64_t dy = -1; dy <= 1; dy++) {
							const auto it = grid.find({cx + dx, cy + dy});

							if(it == grid.end()) continue;

							for(size_t cand : it->second) {
								if(visited[cand]) continue;

								const slug_t ddx = arms[cand].x - arms[cur].x;
								const slug_t ddy = arms[cand].y - arms[cur].y;

								if(std::sqrt(ddx * ddx + ddy * ddy) <= eps) {
									visited[cand] = true;

									stack.push_back(cand);
								}
							}
						}
					}
				}

				bool multi = false;

				for(size_t j = 1; j < cluster.size() && !multi; j++) {
					if(arms[cluster[j]].subpathIndex != arms[cluster[0]].subpathIndex) multi = true;
				}

				if(!multi) continue;

				Atlas::Curves disc;

				_appendArcChunks(disc, arms[cluster[0]].x, arms[cluster[0]].y, h, 0_cv, 2_cv * PI_CV);

				const size_t discStart = _pendingCurves.size();

				if(discStart > 0) _pendingSubpathStarts.push_back(discStart);

				for(const auto& c : disc) _pendingCurves.push_back(c);

				if(cw) CurveDecomposer::reverseCurves(_pendingCurves, discStart, _pendingCurves.size());

				any = true;
			}
		}

		_lutDirty = true;

		return any;
	}

	// -------------------------------------------------------------------------
	// Accessors
	// -------------------------------------------------------------------------

	bool hasPendingPath() const { return !_pendingCurves.empty() || !_activeCurves.empty(); }

	// Resolved subpath-start list for _pendingCurves (index 0 explicit, unlike the internal
	// _pendingSubpathStarts storage convention) -- what strokePath()/getShapeContours() actually
	// want to consume. Empty return means "no curves, or boundaries are unknown" (see
	// _pendingSubpathsUntracked); callers must fall back to the coordinate-gap heuristic in that
	// case, exactly as before this tracking existed.
	std::vector<size_t> pendingContourStarts() const {
		if(_pendingCurves.empty() || _pendingSubpathsUntracked) return {};

		std::vector<size_t> starts;

		starts.reserve(_pendingSubpathStarts.size() + 1);
		starts.push_back(0);
		starts.insert(starts.end(), _pendingSubpathStarts.begin(), _pendingSubpathStarts.end());

		return starts;
	}

	// Same as pendingContourStarts(), but for the curve buffer Canvas::_merged() builds (pending
	// curves followed by any still-open active subpath) without consuming/mutating this Path --
	// used by the explicit-Path commit verbs (fill(const Path&, ...) etc.).
	std::vector<size_t> mergedContourStarts() const {
		if((_pendingCurves.empty() && _activeCurves.empty()) || _pendingSubpathsUntracked) return {};

		std::vector<size_t> starts;

		starts.reserve(_pendingSubpathStarts.size() + _activeSubpathStarts.size() + 2);
		starts.push_back(0);
		starts.insert(starts.end(), _pendingSubpathStarts.begin(), _pendingSubpathStarts.end());

		if(!_activeCurves.empty()) {
			const size_t base = _pendingCurves.size();

			if(base > 0) starts.push_back(base);

			for(size_t s : _activeSubpathStarts) starts.push_back(base + s);
		}

		return starts;
	}

	// -------------------------------------------------------------------------
	// Arc-length sampling
	//
	// sample(t) returns the position and tangent direction at normalized arc-length
	// t in the range [0, 1]. The LUT is rebuilt lazily whenever the path geometry changes.
	// -------------------------------------------------------------------------

	slug_t arcLength() const {
		if(_lutDirty) _rebuildLUT();

		return _totalLength;
	}

	Sample sample(slug_t t) const {
		if(_lutDirty) _rebuildLUT();

		if(_pts.size() < 2) return {};

		const slug_t s = std::max(0_cv, std::min(1_cv, t)) * _totalLength;

		auto it = std::lower_bound(_lut.begin(), _lut.end(), s);
		size_t i = static_cast<size_t>(std::distance(_lut.begin(), it));

		i = std::min(i, _pts.size() - 1);

		if(!i) i = 1;

		const slug_t segLen = _lut[i] - _lut[i-1];
		const slug_t frac = segLen > 1e-12_cv ? (s - _lut[i-1]) / segLen : 0_cv;

		const slug_t x = _pts[i - 1].first + frac * (_pts[i].first - _pts[i - 1].first);
		const slug_t y = _pts[i - 1].second + frac * (_pts[i].second - _pts[i - 1].second);

		const slug_t angle = std::atan2(
			_pts[i].second - _pts[i - 1].second,
			_pts[i].first - _pts[i - 1].first
		);

		return { x, y, angle };
	}

private:
	friend class Canvas;

	// Moves _activeCurves into _pendingCurves (translating any of its subpath-start markers along
	// with it), and clears _activeCurves/_activeSubpathStarts. Shared by closePath() and
	// Canvas::_consolidate() (both "finish the in-progress subpath" call sites) so the marker
	// bookkeeping lives in exactly one place.
	void _flushActiveIntoPending() {
		if(_activeCurves.empty()) return;

		const size_t base = _pendingCurves.size();

		if(base > 0) _pendingSubpathStarts.push_back(base);

		for(size_t s : _activeSubpathStarts) _pendingSubpathStarts.push_back(base + s);

		for(const auto& c : _activeCurves) _pendingCurves.push_back(c);

		_activeCurves.clear();
		_activeSubpathStarts.clear();
	}

	// Appends `srcCurves` (each passed through `xform`) onto _pendingCurves, and folds `srcStarts`
	// (subpath-start offsets relative to srcCurves, same "index 0 implicit" convention as
	// _pendingSubpathStarts) into _pendingSubpathStarts, translated by the pre-append size of
	// _pendingCurves. Shared by both addPath() overloads.
	template<typename Xform>
	void _appendTrackedCurves(const Atlas::Curves& srcCurves, const std::vector<size_t>& srcStarts, Xform&& xform) {
		if(srcCurves.empty()) return;

		const size_t base = _pendingCurves.size();

		if(base > 0) _pendingSubpathStarts.push_back(base);

		for(size_t s : srcStarts) _pendingSubpathStarts.push_back(base + s);

		for(const auto& c : srcCurves) _pendingCurves.push_back(xform(c));
	}

	// Resolves a stored "index 0 implicit" subpath-start vector (_activeSubpathStarts or
	// _pendingSubpathStarts) into the explicit form strokePath()/getShapeContours() consume:
	// index 0 always present, followed by every subsequent boundary in increasing order.
	// `count == 0` (no curves at all) resolves to empty, same as "unknown, use a fallback."
	static std::vector<size_t> _resolveStarts(size_t count, const std::vector<size_t>& stored) {
		if(count == 0) return {};

		std::vector<size_t> starts;

		starts.reserve(stored.size() + 1);
		starts.push_back(0);
		starts.insert(starts.end(), stored.begin(), stored.end());

		return starts;
	}

	// Appends quadratic curves approximating an arc from angle a0 to a0+sweep (|sweep| <= PI),
	// centered at (cx,cy), radius r, into `out`. The first point emitted is exactly
	// (cx + r*cos(a0), cy + r*sin(a0)) -- unlike _arcSegments()/arc() below, this never moveTo()s
	// or bridges from the current pen position; callers (round joins, round caps, the star-vertex
	// discs) already know their own start point and must splice this in directly at an
	// already-computed wall-offset point. Reuses _arcSegments()'s own per-chunk cubic formula,
	// then folds each chunk down to quadratics via a scratch CurveDecomposer -- the same adaptive
	// cubic->quadratic subdivision every other curve-emission path in this file already goes
	// through (CurveDecomposer::cubicTo/_cubicAdaptive), not a second copy of that algorithm.
	static void _appendArcChunks(Atlas::Curves& out, slug_t cx, slug_t cy, slug_t r, slug_t a0, slug_t sweep) {
		if(r <= 0_cv || sweep == 0_cv) return;

		const slug_t absSweep = std::abs(sweep);
		const size_t nSegs = std::max(size_t{1}, static_cast<size_t>(std::ceil(absSweep / PI_2_CV)));
		const slug_t segSweep = sweep / cv(nSegs);

		Atlas::Curves scratch;
		CurveDecomposer dec(scratch);

		dec.moveTo(cx + r * std::cos(a0), cy + r * std::sin(a0));

		slug_t angle = a0;

		for(size_t i = 0; i < nSegs; i++) {
			const slug_t seg0 = angle, seg1 = angle + segSweep;
			const slug_t k = (4_cv / 3_cv) * std::tan(segSweep * 0.25_cv);
			const slug_t cos0 = std::cos(seg0), sin0 = std::sin(seg0);
			const slug_t cos1 = std::cos(seg1), sin1 = std::sin(seg1);

			const slug_t p3x = cx + r * cos1, p3y = cy + r * sin1;
			const slug_t p1x = cx + r * cos0 - k * r * sin0, p1y = cy + r * sin0 + k * r * cos0;
			const slug_t p2x = p3x + k * r * sin1, p2y = p3y - k * r * cos1;

			dec.cubicTo(p1x, p1y, p2x, p2y, p3x, p3y);

			angle = seg1;
		}

		for(const auto& c : scratch) out.push_back(c);
	}

	void _arcSegments(slug_t cx, slug_t cy, slug_t r, slug_t startAngle, slug_t sweep) {
		if(r <= 0_cv || sweep == 0_cv) return;

		const slug_t absSweep = std::abs(sweep);
		const size_t nSegs = std::max(size_t{1}, static_cast<size_t>(std::ceil(absSweep / PI_2_CV)));
		const slug_t segSweep = sweep / cv(nSegs);

		slug_t angle = startAngle;

		for(size_t i = 0; i < nSegs; i++) {
			const slug_t a0 = angle, a1 = angle + segSweep;
			const slug_t k = (4_cv / 3_cv) * std::tan(segSweep * 0.25_cv);
			const slug_t cos0 = std::cos(a0), sin0 = std::sin(a0);
			const slug_t cos1 = std::cos(a1), sin1 = std::sin(a1);

			const slug_t p0x = cx + r * cos0, p0y = cy + r * sin0;
			const slug_t p3x = cx + r * cos1, p3y = cy + r * sin1;
			const slug_t p1x = p0x - k * r * sin0, p1y = p0y + k * r * cos0;
			const slug_t p2x = p3x + k * r * sin1, p2y = p3y - k * r * cos1;

			if(!i && _activeCurves.empty() && _pendingCurves.empty()) moveTo(p0x, p0y);

			else if(!i) {
				const slug_t dx = p0x - _penX, dy = p0y - _penY;

				if(dx*dx + dy*dy > 1e-10_cv) lineTo(p0x, p0y);
			}

			bezierTo(p1x, p1y, p2x, p2y, p3x, p3y);

			angle = a1;
		}
	}

	void _rebuildLUT() const {
		_pts.clear();
		_lut.clear();

		_totalLength = 0_cv;

		Atlas::Curves all = _pendingCurves;

		for(const auto& c : _activeCurves) all.push_back(c);

		if(!all.empty()) {
			_pts.push_back({all.front().x1, all.front().y1});

			for(const auto& c : all) detail::flattenCurve(
				c.x1, c.y1,
				c.x2, c.y2,
				c.x3, c.y3,
				TOLERANCE_BALANCED,
				0,
				_pts
			);

			_lut.resize(_pts.size(), 0_cv);

			for(size_t i = 1; i < _pts.size(); i++) {
				const slug_t dx = _pts[i].first - _pts[i - 1].first;
				const slug_t dy = _pts[i].second - _pts[i - 1].second;

				_lut[i] = _lut[i - 1] + std::sqrt(dx * dx + dy * dy);
			}

			_totalLength = _lut.back();
		}

		_lutDirty = false;
	}

	// _activeCurves MUST be declared before _decomposer (reference binding order).
	Atlas::Curves _activeCurves;
	Atlas::Curves _pendingCurves;

	// Subpath-start curve-index offsets, parallel to _activeCurves/_pendingCurves respectively.
	// Index 0 of a subpath is always implicit (never stored); these hold every SUBSEQUENT
	// boundary in increasing order. Maintained incrementally by moveTo()/closePath()/
	// _flushActiveIntoPending()/addPath() -- see the doc comment at each site. Replaces inferring
	// subpath boundaries from an exact-coordinate gap, which silently fails whenever two
	// independent subpaths happen to share a literal vertex (see strokePath()'s doc comment).
	std::vector<size_t> _activeSubpathStarts;
	std::vector<size_t> _pendingSubpathStarts;

	// Sticky: true only once this Path has ingested curves with no boundary information at all
	// (the raw Path(Atlas::Curves) ctor, or addPath() from a Path that is itself untracked).
	// strokePath() and pendingContourStarts() fall back to the coordinate-gap heuristic whenever
	// this is set, since _pendingSubpathStarts cannot be trusted to be complete.
	bool _pendingSubpathsUntracked = false;

	slug_t _penX = 0_cv, _penY = 0_cv;
	Matrix _ctm = Matrix::identity();
	std::vector<Matrix> _ctmStack;
	CurveDecomposer _decomposer;

	mutable std::vector<std::pair<slug_t, slug_t>> _pts;
	mutable std::vector<slug_t> _lut;
	mutable slug_t _totalLength = 0_cv;
	mutable bool _lutDirty = true;
};

// ================================================================================================
// Text layout enums
// ================================================================================================

enum class TextAnchorY {
	Baseline, // y is the text baseline (default)
	CapCenter, // y is the vertical center of the cap-height band
	CapTop, // y is the top of capital letters
	XCenter, // y is the vertical center of the x-height band
};

enum class TextAlignX {
	Left, // x is the left edge of the first glyph (default, single-pass)
	Center, // x is the horizontal center of the run (two-pass)
	Right, // x is the right edge of the last glyph (two-pass)
};

// ================================================================================================
// Canvas
// ================================================================================================

class Canvas {
public:
	friend std::ostream& operator<<(std::ostream& os, const Canvas& c);

	Canvas(Atlas& atlas, KeyIterator key=KeyIterator()):
	_atlas(atlas),
	_key(key) {
	}

	// -------------------------------------------------------------------------
	// CurveDecomposer access (forwarded to internal Path)
	// -------------------------------------------------------------------------

	CurveDecomposer& decomposer() { return _path.decomposer(); }
	const CurveDecomposer& decomposer() const { return _path.decomposer(); }

	void setTolerance(slug_t tol) { _path.decomposer().tolerance = tol; }
	slug_t getTolerance() const { return _path.decomposer().tolerance; }

	// -------------------------------------------------------------------------
	// Path snapshot
	//
	// Returns a copy of the internal path. The copy can be sampled, modified
	// independently, or passed back to explicit-path commit verbs. The internal
	// path is left completely intact.
	// -------------------------------------------------------------------------

	Path path() const { return _path; }

	// -------------------------------------------------------------------------
	// Transform stack
	//
	// Two parallel CTMs are maintained and kept in sync:
	//
	// _path._ctm - bakes transforms into internal path geometry at moveTo/lineTo time
	// _ctm - applied as a placement transform for external-Path commit verbs
	//
	// Internal path commits (fill(Color), stroke(width, Color), etc.) rely on geometry
	// already baked by _path._ctm, so they do not additionally compose _ctm.
	// External path commits (fill(Path, Color), etc.) compose _ctm into the layer
	// transform so that translate/rotate/scale correctly position pre-built paths.
	// Text always composes _ctm since glyph positions are computed in canvas space.
	// save() / restore() checkpoint both CTMs together.
	// -------------------------------------------------------------------------

	Canvas& save() {
		_path.save();
		_ctmStack.push_back({_ctm, _autoMetrics, _splitsX, _splitsY});

		return *this;
	}

	Canvas& restore() {
		_path.restore();

		if(!_ctmStack.empty()) {
			auto& s = _ctmStack.back();

			_ctm = s.ctm;
			_autoMetrics = s.autoMetrics;
			_splitsX = std::move(s.splitsX);
			_splitsY = std::move(s.splitsY);

			_ctmStack.pop_back();
		}

		return *this;
	}

	const Matrix& getTransform() const { return _ctm; }
	Canvas& resetTransform() { _path.resetTransform(); _ctm = Matrix::identity(); return *this; }
	Canvas& setTransform(const Matrix& m) { _path.setTransform(m); _ctm = m; return *this; }
	Canvas& transform(const Matrix& m) { _path.transform(m); _ctm = _ctm * m; return *this; }

	Canvas& translate(slug_t tx, slug_t ty) {
		_path.translate(tx, ty);

		_ctm = _ctm * Matrix::translate(tx, ty);

		return *this;
	}

	Canvas& rotate(slug_t angle) {
		_path.rotate(angle);

		_ctm = _ctm * Matrix::rotate(angle);

		return *this;
	}

	Canvas& scale(slug_t sx, slug_t sy) {
		_path.scale(sx, sy);

		_ctm = _ctm * Matrix::scale(sx, sy);

		return *this;
	}

	// -------------------------------------------------------------------------
	// Path commands (forwarded to internal Path)
	// -------------------------------------------------------------------------

	Canvas& beginPath() { _path.clear(); return *this; }
	Canvas& addPath(const Path& other) { _path.addPath(other); return *this; }
	Canvas& addPath(const Path& other, const Matrix& transform) { _path.addPath(other, transform); return *this; }
	Canvas& moveTo(slug_t x, slug_t y) { _path.moveTo(x, y); return *this; }
	Canvas& lineTo(slug_t x, slug_t y) { _path.lineTo(x, y); return *this; }
	Canvas& quadTo(slug_t cx, slug_t cy, slug_t x, slug_t y) { _path.quadTo(cx, cy, x, y); return *this; }

	Canvas& bezierTo(slug_t c1x, slug_t c1y, slug_t c2x, slug_t c2y, slug_t x, slug_t y) {
		_path.bezierTo(c1x, c1y, c2x, c2y, x, y);

		return *this;
	}

	Canvas& closePath() { _path.closePath(); return *this; }
	bool strokePath(
		slug_t width, bool cw=false,
		LineJoin join=LineJoin::Miter, LineCap cap=LineCap::Butt, slug_t miterLimit=4_cv
	) { return _path.strokePath(width, cw, join, cap, miterLimit); }
	bool hasPendingPath() const { return _path.hasPendingPath(); }

	// -------------------------------------------------------------------------
	// Shape helpers (forwarded to internal Path)
	// -------------------------------------------------------------------------

	Canvas& rect(slug_t x, slug_t y, slug_t w, slug_t h) { _path.rect(x, y, w, h); return *this; }
	Canvas& roundedRect(slug_t x, slug_t y, slug_t w, slug_t h, slug_t r) { _path.roundedRect(x, y, w, h, r); return *this; }
	Canvas& circle(slug_t cx, slug_t cy, slug_t r) { _path.circle(cx, cy, r); return *this; }
	Canvas& ellipse(slug_t cx, slug_t cy, slug_t rx, slug_t ry) { _path.ellipse(cx, cy, rx, ry); return *this; }

	Canvas& arc(slug_t cx, slug_t cy, slug_t r, slug_t startAngle, slug_t endAngle, bool ccw=false) {
		_path.arc(cx, cy, r, startAngle, endAngle, ccw);

		return *this;
	}

	Canvas& arcTo(slug_t x1, slug_t y1, slug_t x2, slug_t y2, slug_t r) {
		_path.arcTo(x1, y1, x2, y2, r);

		return *this;
	}

	// -------------------------------------------------------------------------
	// Gradient factory
	// -------------------------------------------------------------------------

	struct GradientHandle {
		GradientInfo::Type type = GradientInfo::Type::Linear;

		slug_t x0 = 0_cv, y0 = 0_cv, x1 = 1_cv, y1 = 0_cv;

		std::vector<GradientStop> stops = {};
	};

	GradientHandle createLinearGradient(
		slug_t x0, slug_t y0,
		slug_t x1, slug_t y1,
		std::vector<GradientStop> stops
	) {
		return { GradientInfo::Type::Linear, x0, y0, x1, y1, std::move(stops) };
	}

	GradientHandle createRadialGradient(
		slug_t cx, slug_t cy,
		slug_t r0, slug_t r1,
		std::vector<GradientStop> stops
	) {
		return { GradientInfo::Type::Radial, cx, cy, r0, r1, std::move(stops) };
	}

	GradientHandle createSweepGradient(
		slug_t cx, slug_t cy,
		slug_t startAngle, slug_t endAngle,
		std::vector<GradientStop> stops
	) {
		return { GradientInfo::Type::Sweep, cx, cy, startAngle, endAngle, std::move(stops) };
	}

	// -------------------------------------------------------------------------
	// Band split state (persists like fillStyle - applies to subsequent commits)
	//
	// setSplits() - explicit normalized [0,1] fractions; clears any strategy
	// setSplitStrategy() - callable computed from curves at commit time; clears explicit splits
	// clearSplits() - revert to default auto-band behavior
	// -------------------------------------------------------------------------

	Canvas& setSplits(std::vector<slug_t> splitsX, std::vector<slug_t> splitsY) {
		_splitsX = std::move(splitsX);
		_splitsY = std::move(splitsY);
		_splitStrategy = {};

		return *this;
	}

	Canvas& setSplitStrategy(Atlas::SplitStrategy strategy) {
		_splitStrategy = std::move(strategy);

		_splitsX.clear();
		_splitsY.clear();

		return *this;
	}

	Canvas& clearSplits() {
		_splitsX.clear();
		_splitsY.clear();

		_splitStrategy = {};

		return *this;
	}

	// -------------------------------------------------------------------------
	// Cell extent state
	//
	// setAutoMetrics(false) keep curves in canvas [0,1] space and declare the
	// full unit-square as the shape's extent. Both the GPU quad and the band
	// transforms are calibrated to [0,1]x[0,1], so fract()-based tiling works
	// correctly: padding the author leaves around the shape IS the tile gap.
	// setAutoMetrics(true) restores the default tight-bbox behaviour.
	// -------------------------------------------------------------------------

	Canvas& setAutoMetrics(bool v) { _autoMetrics = v; return *this; }
	bool getAutoMetrics() const { return _autoMetrics; }

	// -------------------------------------------------------------------------
	// Commit verbs - implicit path
	//
	// Operate on the Canvas's internal path. After fill() / stroke(), the
	// accumulated curves remain in the path until the next beginPath() call.
	// -------------------------------------------------------------------------

	Layer fill(Color color, slug_t scale=1_cv, Atlas::ShapeInfo::Origin origin={}) {
		_consolidate();

		if(_path._pendingCurves.empty()) return Layer{};

		return _commitFill(_path._pendingCurves, color, scale, _key.next(), origin, Matrix::identity(), _path.pendingContourStarts());
	}

	Layer fill(Color color, slug_t scale, Key key, Atlas::ShapeInfo::Origin origin={}) {
		_consolidate();

		if(_path._pendingCurves.empty()) return Layer{};

		return _commitFill(_path._pendingCurves, color, scale, key, origin, Matrix::identity(), _path.pendingContourStarts());
	}

	bool defineShape(Key key, slug_t scale=1_cv, Atlas::ShapeInfo::Origin origin={}) {
		_consolidate();

		return _commitShape(_path._pendingCurves, key, scale, origin, _path.pendingContourStarts());
	}

	Layer stroke(
		slug_t width, Color color, slug_t scale=1_cv, Atlas::ShapeInfo::Origin origin={},
		LineJoin join=LineJoin::Miter, LineCap cap=LineCap::Butt, slug_t miterLimit=4_cv
	) {
		if(!_path.strokePath(width, false, join, cap, miterLimit)) return Layer{};

		return _commitFill(_path._pendingCurves, color, scale, _key.next(), origin, Matrix::identity(), _path.pendingContourStarts());
	}

	Layer stroke(
		slug_t width, Color color, slug_t scale, Key key, Atlas::ShapeInfo::Origin origin={},
		LineJoin join=LineJoin::Miter, LineCap cap=LineCap::Butt, slug_t miterLimit=4_cv
	) {
		if(!_path.strokePath(width, false, join, cap, miterLimit)) return Layer{};

		return _commitFill(_path._pendingCurves, color, scale, key, origin, Matrix::identity(), _path.pendingContourStarts());
	}

	Layer fillGradient(const GradientHandle& handle, slug_t scale=1_cv, Atlas::ShapeInfo::Origin origin={}) {
		_consolidate();

		if(_path._pendingCurves.empty()) return Layer{};

		Layer layer = _commitGradient(_path._pendingCurves, handle, scale, _key.next(), origin, Matrix::identity(), _path.pendingContourStarts());

		_path._pendingCurves.clear();
		_path._pendingSubpathStarts.clear();
		_path._pendingSubpathsUntracked = false;

		return layer;
	}

	Layer fillGradient(const GradientHandle& handle, slug_t scale, Key key, Atlas::ShapeInfo::Origin origin={}) {
		_consolidate();

		if(_path._pendingCurves.empty()) return Layer{};

		Layer layer = _commitGradient(_path._pendingCurves, handle, scale, key, origin, Matrix::identity(), _path.pendingContourStarts());

		_path._pendingCurves.clear();
		_path._pendingSubpathStarts.clear();
		_path._pendingSubpathsUntracked = false;

		return layer;
	}

	Layer strokeGradient(
		slug_t width, const GradientHandle& handle, slug_t scale=1_cv, Atlas::ShapeInfo::Origin origin={},
		LineJoin join=LineJoin::Miter, LineCap cap=LineCap::Butt, slug_t miterLimit=4_cv
	) {
		if(!_path.strokePath(width, false, join, cap, miterLimit)) return Layer{};

		Layer layer = _commitGradient(_path._pendingCurves, handle, scale, _key.next(), origin, Matrix::identity(), _path.pendingContourStarts());

		_path._pendingCurves.clear();
		_path._pendingSubpathStarts.clear();
		_path._pendingSubpathsUntracked = false;

		return layer;
	}

	Layer strokeGradient(
		slug_t width, const GradientHandle& handle, slug_t scale, Key key, Atlas::ShapeInfo::Origin origin={},
		LineJoin join=LineJoin::Miter, LineCap cap=LineCap::Butt, slug_t miterLimit=4_cv
	) {
		if(!_path.strokePath(width, false, join, cap, miterLimit)) return Layer{};

		Layer layer = _commitGradient(_path._pendingCurves, handle, scale, key, origin, Matrix::identity(), _path.pendingContourStarts());

		_path._pendingCurves.clear();
		_path._pendingSubpathStarts.clear();
		_path._pendingSubpathsUntracked = false;

		return layer;
	}

	// -------------------------------------------------------------------------
	// Commit verbs - explicit Path
	//
	// Accept a standalone Path. The path is not modified by any of these calls.
	// stroke() makes an internal copy to expand the stroke without altering @p p.
	// -------------------------------------------------------------------------

	Layer fill(const Path& p, Color color, slug_t scale=1_cv, Atlas::ShapeInfo::Origin origin={}) {
		if(!p.hasPendingPath()) return Layer{};

		return _commitFill(_merged(p), color, scale, _key.next(), origin, _ctm, p.mergedContourStarts());
	}

	Layer fill(const Path& p, Color color, slug_t scale, Key key, Atlas::ShapeInfo::Origin origin={}) {
		if(!p.hasPendingPath()) return Layer{};

		return _commitFill(_merged(p), color, scale, key, origin, _ctm, p.mergedContourStarts());
	}

	bool defineShape(const Path& p, Key key, slug_t scale=1_cv, Atlas::ShapeInfo::Origin origin={}) {
		if(!p.hasPendingPath()) return false;

		return _commitShape(_merged(p), key, scale, origin, p.mergedContourStarts());
	}

	Layer stroke(
		const Path& p, slug_t width, Color color, slug_t scale=1_cv, Atlas::ShapeInfo::Origin origin={},
		LineJoin join=LineJoin::Miter, LineCap cap=LineCap::Butt, slug_t miterLimit=4_cv
	) {
		Path copy = p;

		if(!copy.strokePath(width, false, join, cap, miterLimit)) return Layer{};

		return _commitFill(_merged(copy), color, scale, _key.next(), origin, _ctm, copy.mergedContourStarts());
	}

	Layer stroke(
		const Path& p, slug_t width, Color color, slug_t scale, Key key, Atlas::ShapeInfo::Origin origin={},
		LineJoin join=LineJoin::Miter, LineCap cap=LineCap::Butt, slug_t miterLimit=4_cv
	) {
		Path copy = p;

		if(!copy.strokePath(width, false, join, cap, miterLimit)) return Layer{};

		return _commitFill(_merged(copy), color, scale, key, origin, _ctm, copy.mergedContourStarts());
	}

	Layer fillGradient(const Path& p, const GradientHandle& handle, slug_t scale=1_cv, Atlas::ShapeInfo::Origin origin={}) {
		if(!p.hasPendingPath()) return Layer{};

		return _commitGradient(_merged(p), handle, scale, _key.next(), origin, _ctm, p.mergedContourStarts());
	}

	Layer fillGradient(const Path& p, const GradientHandle& handle, slug_t scale, Key key, Atlas::ShapeInfo::Origin origin={}) {
		if(!p.hasPendingPath()) return Layer{};

		return _commitGradient(_merged(p), handle, scale, key, origin, _ctm, p.mergedContourStarts());
	}

	// -------------------------------------------------------------------------
	// Text layout
	//
	// Places glyphs from the atlas into the current composite. The atlas must
	// already contain the requested codepoints (loaded via a font backend before
	// atlas->build()). Handles em-space conversion, vertical anchoring, and
	// optional horizontal alignment internally.
	// -------------------------------------------------------------------------

	Canvas& text(
		std::string_view str,
		slug_t fontSize,
		slug_t x,
		slug_t y,
		Color color,
		const FontMetrics& metrics,
		TextAnchorY anchorY=TextAnchorY::Baseline,
		TextAlignX alignX=TextAlignX::Left,
		// Opt-in namespace matching the mask a font's glyphs were loaded under
		// (freetype.hpp's LoadConfig::mask) -- see slughorn::Key's bit-layout comment.
		// Lets multiple fonts share one Atlas/Canvas instead of needing one per font.
		uint8_t mask=0
	) {
		if(str.empty() || fontSize == 0_cv) return *this;

		// Apply CTM so text() uses the same coordinate space as path operations.
		slug_t tx, ty;

		_ctm.apply(x, y, tx, ty);

		// Baseline dy in em-space, adjusted for vertical anchor
		slug_t dy = ty / fontSize;

		switch(anchorY) {
			case TextAnchorY::Baseline: break;
			case TextAnchorY::CapCenter: dy -= metrics.capHeightRatio * 0.5_cv; break;
			case TextAnchorY::CapTop: dy -= metrics.capHeightRatio; break;
			case TextAnchorY::XCenter: dy -= metrics.xHeightRatio * 0.5_cv; break;
		}

		// Starting dx in em-space; two-pass only for Center/Right
		slug_t dx = tx / fontSize;

		if(alignX != TextAlignX::Left) {
			slug_t totalAdvance = 0_cv;

			for(char c : str) {
				const auto shape = _atlas.getShape(Key(static_cast<uint32_t>(static_cast<unsigned char>(c)), mask));

				totalAdvance += shape ? shape->advance : 0.6_cv;
			}

			if(alignX == TextAlignX::Center) dx -= totalAdvance * 0.5_cv;

			else dx -= totalAdvance;
		}

		for(char c : str) {
			const uint32_t cp = static_cast<uint32_t>(static_cast<unsigned char>(c));
			const auto info = _atlas.getShape(Key(cp, mask));

			Layer layer{
				.key = Key(cp, mask),
				.color = color,
				.transform = {.x = dx, .y = dy},
				.scale = fontSize
			};

			_composite.layers.push_back(layer);

#ifdef SLUGHORN_HAS_MSDF
			// text() is the one commit verb that does NOT go through _commitFill() -- glyph
			// shapes are pre-registered by the Font loader at codepoint keys, so there's no
			// addShape() call here for _applyMSDF() to piggyback on the way
			// _commitFill()/_commitGradient() do. Call it directly per glyph instead.
			_applyMSDF(Key(cp, mask));
#endif

			dx += info ? info->advance : 0.6_cv;
		}

		return *this;
	}

	Canvas& strokeText(
		std::string_view str,
		slug_t fontSize,
		slug_t strokeWidth,
		slug_t x,
		slug_t y,
		Color color,
		const FontMetrics& metrics,
		TextAnchorY anchorY=TextAnchorY::Baseline,
		TextAlignX alignX=TextAlignX::Left,
		uint8_t mask=0 // see text()'s mask doc comment
	) {
		if(str.empty() || fontSize == 0_cv) return *this;

		slug_t tx, ty;

		_ctm.apply(x, y, tx, ty);

		slug_t dy = ty / fontSize;

		switch(anchorY) {
			case TextAnchorY::Baseline: break;
			case TextAnchorY::CapCenter: dy -= metrics.capHeightRatio * 0.5_cv; break;
			case TextAnchorY::CapTop: dy -= metrics.capHeightRatio; break;
			case TextAnchorY::XCenter: dy -= metrics.xHeightRatio * 0.5_cv; break;
		}

		slug_t dx = tx / fontSize;

		if(alignX != TextAlignX::Left) {
			slug_t totalAdvance = 0_cv;

			for(char c : str) {
				const auto shape = _atlas.getShape(Key(static_cast<uint32_t>(static_cast<unsigned char>(c)), mask));

				totalAdvance += shape ? shape->advance : 0.6_cv;
			}

			if(alignX == TextAlignX::Center) dx -= totalAdvance * 0.5_cv;

			else dx -= totalAdvance;
		}

		for(char c : str) {
			const uint32_t cp = static_cast<uint32_t>(static_cast<unsigned char>(c));
			const auto info = _atlas.getShape(Key(cp, mask));

			for(const auto& contour : _atlas.getShapeContours(Key(cp, mask))) {
				Path gp;

				for(const auto& curve : contour) {
					gp._pendingCurves.push_back({
						(curve.x1 + dx) * fontSize, (curve.y1 + dy) * fontSize,
						(curve.x2 + dx) * fontSize, (curve.y2 + dy) * fontSize,
						(curve.x3 + dx) * fontSize, (curve.y3 + dy) * fontSize,
					});
				}

				if(gp.strokePath(strokeWidth))
					_commitFill(gp._pendingCurves, color, 1_cv, _key.next(), {}, Matrix::identity(), gp.pendingContourStarts());
			}

			dx += info ? info->advance : 0.6_cv;
		}

		return *this;
	}

	// textGlyph - bakes a single glyph's em-space curves into world-space at scale=1.
	// x, y are canvas-space (CTM applied internally). angle = tangent in radians.
	// Each call produces one unique atlas shape, acceptable for static/infrequent text.
	// Do NOT use for per-frame animation; use direct addLayer() instead.
	Layer textGlyph(
		uint32_t cp,
		slug_t fontSize,
		slug_t x, slug_t y,
		slug_t angle,
		Color color,
		const FontMetrics& metrics,
		TextAnchorY anchorY=TextAnchorY::Baseline,
		uint8_t mask=0 // see text()'s mask doc comment
	) {
		if(fontSize == 0_cv) return Layer{};

		slug_t tx, ty;

		_ctm.apply(x, y, tx, ty);

		return _textGlyphWorld(cp, fontSize, tx, ty, angle, color, metrics, anchorY, mask);
	}

	// textOnPath - places filled glyphs from str along path, each rotated to follow
	// the tangent. startFrac in [0,1] is the normalized arc-length start position.
	// Glyphs that would extend past the path end are dropped.
	Canvas& textOnPath(
		const Path& path,
		std::string_view str,
		slug_t fontSize,
		slug_t startFrac,
		Color color,
		const FontMetrics& metrics,
		TextAnchorY anchorY=TextAnchorY::Baseline,
		uint8_t mask=0 // see text()'s mask doc comment
	) {
		if(str.empty() || fontSize == 0_cv) return *this;

		const slug_t totalLen = path.arcLength();

		if(totalLen <= 0_cv) return *this;

		slug_t cursor = std::max(0_cv, std::min(1_cv, startFrac)) * totalLen;

		for(char c : str) {
			if(cursor > totalLen) break;

			const uint32_t cp = static_cast<uint32_t>(static_cast<unsigned char>(c));
			const auto info = _atlas.getShape(Key(cp, mask));
			const slug_t advance = (info && info->advance > 0_cv ? info->advance : 0.6_cv) * fontSize;

			const slug_t t = cursor / totalLen;
			const auto s = path.sample(t);

			_textGlyphWorld(cp, fontSize, s.x, s.y, s.angle, color, metrics, anchorY, mask);

			cursor += advance;
		}

		return *this;
	}

	// -------------------------------------------------------------------------
	// CompositeShape management
	// -------------------------------------------------------------------------

	Canvas& beginComposite() {
		_composite = CompositeShape{};

		beginPath();

		return *this;
	}

	Canvas& setAdvance(slug_t advance) { _composite.advance = advance; return *this; }

	// -------------------------------------------------------------------------
	// MSDF opt-in state (persists like fillStyle - applies to subsequent commits, same
	// convention as setSplits()/setAutoMetrics()).
	//
	// setMSDF(true, range) - every subsequent fill()/stroke()/text()/textGlyph() commit also
	// calls Atlas::requestMSDF(key, range) for the shape it just registered. Replaces the
	// "for(layer : compositeShape.layers) atlas->registerMSDF(layer.key, range)" boilerplate
	// loop every MSDF-effect example used to need after finalize()+build(). setMSDF(false)
	// (the default) reverts to no MSDF request at all -- unaffected callers pay nothing.
	//
	// requestMSDF() itself (unlike the old registerMSDF()) is safe to call before build() -- see
	// its doc comment in slughorn.hpp -- which is what makes this a Canvas-side toggle instead
	// of a post-build loop the caller has to remember to write: every commit made under
	// setMSDF(true) just queues its shape's tile request immediately, and build() renders all
	// of them (batched, in parallel) once it's safe to.
	// -------------------------------------------------------------------------

#ifdef SLUGHORN_HAS_MSDF
	Canvas& setMSDF(
		bool enabled,
		slug_t range=0.1_cv,
		Atlas::MSDFEdgeColoring coloring=Atlas::MSDFEdgeColoring::ByDistance
	) {
		_msdfEnabled = enabled;
		_msdfRange = range;
		_msdfColoring = coloring;

		return *this;
	}

	bool getMSDF() const { return _msdfEnabled; }
#endif

	// -------------------------------------------------------------------------
	// Mask authoring
	//
	// Two forms, both sugar over mechanisms that already exist -- neither is new capability,
	// and direct field access (compositeShape.mask = ...) remains valid unchanged either way.
	//
	// mask(range, invert) - commits the Canvas's accumulated internal path as an MSDF-baked
	// mask: defineShape() semantics (registers geometry in the Atlas, does NOT push a Layer),
	// auto-generated key (same convention as fill()/stroke() with no explicit key), then
	// assigns Mask::msdf(key) to the CompositeShape under construction. cx/cy/r are derived
	// from the path's own canvas-space bbox at author time -- this is different from (and does
	// NOT contradict) osgSlug::RenderMask deliberately refusing to derive this at render time;
	// slughorn's Canvas is the authoring layer, so bbox math here is the same kind of thing
	// _commitFill's origin handling already does, not a render-time inference.
	//
	// This calls Atlas::requestMSDF(key, range) itself (unconditionally -- independent of the
	// setMSDF() toggle above, which only affects ordinary fill()/text() layers, not masks) --
	// requestMSDF() is safe to call here even though mask() always runs pre-build, so there's no
	// separate post-build step for the caller to remember, unlike the old registerMSDF()-based
	// design (see ai/context-todo-mask.md for the "authoring vs after-build()" sharp edge that
	// motivated requestMSDF()'s existence).
	//
	// mask(const Mask&) - procedural types need no atlas registration at all; a one-line field
	// assignment staged on the Canvas (same pattern as setAdvance()) until finalize() moves it
	// onto the CompositeShape.
	// -------------------------------------------------------------------------

	Mask mask(slug_t range=0.1_cv, bool invert=false) {
		_consolidate();

		if(_path._pendingCurves.empty()) return Mask{};

		Key key = _key.next();

		if(!_commitShape(_path._pendingCurves, key, 1_cv, {}, _path.pendingContourStarts())) return Mask{};

		slug_t minX = std::numeric_limits<slug_t>::max();
		slug_t minY = std::numeric_limits<slug_t>::max();
		slug_t maxX = -std::numeric_limits<slug_t>::max();
		slug_t maxY = -std::numeric_limits<slug_t>::max();

		for(const auto& c : _path._pendingCurves) {
			minX = std::min({minX, c.x1, c.x2, c.x3});
			minY = std::min({minY, c.y1, c.y2, c.y3});
			maxX = std::max({maxX, c.x1, c.x2, c.x3});
			maxY = std::max({maxY, c.y1, c.y2, c.y3});
		}

		Mask m = Mask::msdf(key, invert);

		m.params[0] = (minX + maxX) * 0.5_cv;
		m.params[1] = (minY + maxY) * 0.5_cv;
		m.params[2] = std::max(maxX - minX, maxY - minY) * 0.5_cv;
		m.params[3] = range;

#ifdef SLUGHORN_HAS_MSDF
		_atlas.requestMSDF(key, range);
#endif

		_composite.mask = m;

		return m;
	}

	Canvas& mask(const Mask& m) {
		_composite.mask = m;

		return *this;
	}

	CompositeShape finalize() {
		CompositeShape result = std::move(_composite);

		beginComposite();

		return result;
	}

	void finalize(Key key) { _atlas.addCompositeShape(key, finalize()); }

	size_t layerCount() const { return _composite.layers.size(); }

private:
	// Core of textGlyph - takes world-space (tx, ty) directly so textOnPath can
	// call it with path.sample() coords without re-applying the CTM.
	Layer _textGlyphWorld(
		uint32_t cp,
		slug_t fontSize,
		slug_t tx, slug_t ty,
		slug_t angle,
		Color color,
		const FontMetrics& metrics,
		TextAnchorY anchorY,
		uint8_t mask=0 // see text()'s mask doc comment
	) {
		const auto info = _atlas.getShape(Key(cp, mask));

		if(!info || info->curves.empty()) return Layer{};

		// angle==0 needs no rotated/baked geometry -- reference the atlas glyph directly instead
		// of copying its curves under a fresh auto-key, exactly mirroring text()'s per-character
		// Layer construction (same em-space anchor math). Layer.transform has no rotation field
		// (see NEXT_SESSION.md's "Real gap #1"), so angle!=0 still has to bake -- but the common
		// unrotated case (e.g. every non-D4 die face in decalcube.cpp) no longer does.
		//
		// NOTE 2026-08-14: consumers that map a shape's declared em-bounds across a fixed-size
		// target (DecalDrawable's planar decals) need their OWN compensation for this -- see
		// NEXT_SESSION.md's DecalDrawable em-window writeup. Referencing the real glyph here is
		// correct; whether it LOOKS correct downstream depends on that consumer.
		if(angle == 0_cv) {
			slug_t dy = ty / fontSize;

			switch(anchorY) {
				case TextAnchorY::Baseline: break;
				case TextAnchorY::CapCenter: dy -= metrics.capHeightRatio * 0.5_cv; break;
				case TextAnchorY::CapTop: dy -= metrics.capHeightRatio; break;
				case TextAnchorY::XCenter: dy -= metrics.xHeightRatio * 0.5_cv; break;
			}

			Layer layer{
				.key = Key(cp, mask),
				.color = color,
				.transform = {.x = tx / fontSize, .y = dy},
				.scale = fontSize
			};

			_composite.layers.push_back(layer);

#ifdef SLUGHORN_HAS_MSDF
			// See text()'s own comment: this bypasses _commitFill(), so _applyMSDF() has no
			// addShape() call to piggyback on and must be called directly.
			_applyMSDF(Key(cp, mask));
#endif

			return layer;
		}

		slug_t anchorOffsetY = 0_cv;

		switch(anchorY) {
			case TextAnchorY::Baseline: break;
			case TextAnchorY::CapCenter: anchorOffsetY = metrics.capHeightRatio * 0.5_cv * fontSize; break;
			case TextAnchorY::CapTop: anchorOffsetY = metrics.capHeightRatio * fontSize; break;
			case TextAnchorY::XCenter: anchorOffsetY = metrics.xHeightRatio * 0.5_cv * fontSize; break;
		}

		const slug_t cosA = std::cos(angle), sinA = std::sin(angle);

		auto xform = [&](slug_t px, slug_t py, slug_t& wx, slug_t& wy) {
			const slug_t sx = px * fontSize;
			const slug_t sy = py * fontSize - anchorOffsetY;

			wx = sx * cosA - sy * sinA + tx;
			wy = sx * sinA + sy * cosA + ty;
		};

		Atlas::Curves baked;

		baked.reserve(info->curves.size());

		for(const auto& cv : info->curves) {
			slug_t wx1, wy1, wx2, wy2, wx3, wy3;

			xform(cv.x1, cv.y1, wx1, wy1);
			xform(cv.x2, cv.y2, wx2, wy2);
			xform(cv.x3, cv.y3, wx3, wy3);

			baked.push_back({wx1, wy1, wx2, wy2, wx3, wy3});
		}

		using Origin = Atlas::ShapeInfo::Origin;

		// `baked` is an element-wise world-space transform of info->curves (same size/order, see
		// the loop above) -- info->contourStarts (if the glyph's own source ever populated it)
		// carries over unchanged for the same reason _commitFill()'s own doc comment gives.
		return _commitFill(baked, color, 1_cv, _key.next(), Origin(Origin::Type::Centered), Matrix::identity(), info->contourStarts);
	}

	// Merge _activeCurves into _pendingCurves on the internal path (subpath markers along with it).
	void _consolidate() { _path._flushActiveIntoPending(); }

	// Return a merged (active + pending) copy of @p p's curves without consuming them.
	static Atlas::Curves _merged(const Path& p) {
		Atlas::Curves out = p._pendingCurves;

		for(const auto& c : p._activeCurves) out.push_back(c);

		return out;
	}

	// Core fill commit: scale, apply origin, register in atlas, push Layer.
	Layer _commitFill(
		const Atlas::Curves& curves,
		Color color,
		slug_t scale,
		Key key,
		Atlas::ShapeInfo::Origin origin,
		const Matrix& placement=Matrix::identity(),
		// See ShapeInfo::contourStarts's doc comment. `curves` here is whatever the caller passed
		// (_path._pendingCurves or a _merged() copy) BEFORE _scaleCurves()/_toLocalOrigin() run --
		// both are pure per-curve coordinate transforms (verified: same size, same order, no
		// reordering), so indices computed against the caller's original buffer stay valid against
		// the scaled/re-origined `local` stored below.
		const std::vector<size_t>& contourStarts={}
	) {
		if(curves.empty()) return Layer{};

		Atlas::Curves scaled = _scaleCurves(curves, scale);

		Atlas::ShapeInfo::Origin infoOrigin = origin;

		if(origin.type == Atlas::ShapeInfo::Origin::Type::Pivot && !scaled.empty()) {
			slug_t minX_em = std::numeric_limits<slug_t>::max();
			slug_t minY_em = std::numeric_limits<slug_t>::max();

			for(const auto& c : scaled) {
				minX_em = std::min({minX_em, c.x1, c.x2, c.x3});
				minY_em = std::min({minY_em, c.y1, c.y2, c.y3});
			}

			infoOrigin.x = origin.x * scale - minX_em;
			infoOrigin.y = origin.y * scale - minY_em;
		}

		else if(origin.type == Atlas::ShapeInfo::Origin::Type::Custom) {
			infoOrigin.x = origin.x * scale;
			infoOrigin.y = origin.y * scale;
		}

		Atlas::Curves local;

		Matrix transform = Matrix::identity();

		if(_autoMetrics) {
			auto result = _toLocalOrigin(scaled, origin, scale);

			local = std::move(result.first);
			transform = result.second;
		}

		else {
			// Keep curves in canvas space so the padding the author left around
			// the shape is symmetric on all sides (required for correct tiling).
			local = std::move(scaled);
		}

		if(local.empty()) return Layer{};

		Atlas::ShapeInfo info;

		info.curves = std::move(local);
		info.contourStarts = contourStarts;

		if(_autoMetrics) info.origin = infoOrigin;

		else {
			info.autoMetrics = false;
			info.bearingX = 0_cv;
			info.bearingY = 1_cv;
			info.width = 1_cv;
			info.height = 1_cv;
		}

		_applySplits(info);

		_atlas.addShape(key, info);

#ifdef SLUGHORN_HAS_MSDF
		_applyMSDF(key);
#endif

		Layer layer{ .key = key, .color = color };

		placement.apply(transform.dx, transform.dy, layer.transform.x, layer.transform.y);

		_composite.layers.push_back(layer);

		return layer;
	}

	// Shape-only commit: no Layer pushed, no color.
	bool _commitShape(
		const Atlas::Curves& curves,
		Key key,
		slug_t scale,
		Atlas::ShapeInfo::Origin origin,
		const std::vector<size_t>& contourStarts={}
	) {
		if(curves.empty()) return false;

		Atlas::Curves scaled = _scaleCurves(curves, scale);

		Atlas::ShapeInfo::Origin infoOrigin = origin;

		if(origin.type == Atlas::ShapeInfo::Origin::Type::Pivot && !scaled.empty()) {
			slug_t minX_em = std::numeric_limits<slug_t>::max();
			slug_t minY_em = std::numeric_limits<slug_t>::max();

			for(const auto& c : scaled) {
				minX_em = std::min({minX_em, c.x1, c.x2, c.x3});
				minY_em = std::min({minY_em, c.y1, c.y2, c.y3});
			}

			infoOrigin.x = origin.x * scale - minX_em;
			infoOrigin.y = origin.y * scale - minY_em;
		}

		else if(origin.type == Atlas::ShapeInfo::Origin::Type::Custom) {
			infoOrigin.x = origin.x * scale;
			infoOrigin.y = origin.y * scale;
		}

		Atlas::Curves local;

		if(_autoMetrics) local = _toLocalOrigin(scaled).first;

		else {
			// Keep curves in canvas space so the padding the author left around
			// the shape is symmetric on all sides (required for correct tiling).
			local = std::move(scaled);
		}

		if(local.empty()) return false;

		Atlas::ShapeInfo info;

		info.curves = std::move(local);
		info.contourStarts = contourStarts;

		if(_autoMetrics) info.origin = infoOrigin;

		else {
			info.autoMetrics = false;
			info.bearingX = 0_cv;
			info.bearingY = 1_cv;
			info.width = 1_cv;
			info.height = 1_cv;
		}

		_applySplits(info);
		_atlas.addShape(key, info);

		return true;
	}

	// Gradient fill commit.
	Layer _commitGradient(
		const Atlas::Curves& curves,
		const GradientHandle& handle,
		slug_t scale,
		Key key,
		Atlas::ShapeInfo::Origin origin,
		const Matrix& placement = Matrix::identity(),
		const std::vector<size_t>& contourStarts={}
	) {
		if(curves.empty()) return Layer{};

		Atlas::Curves scaled = _scaleCurves(curves, scale);

		slug_t minX = std::numeric_limits<slug_t>::max();
		slug_t minY = std::numeric_limits<slug_t>::max();

		for(const auto& c : scaled) {
			minX = std::min({minX, c.x1, c.x2, c.x3});
			minY = std::min({minY, c.y1, c.y2, c.y3});
		}

		Atlas::ShapeInfo::Origin infoOrigin = origin;

		if(origin.type == Atlas::ShapeInfo::Origin::Type::Pivot) {
			infoOrigin.x = origin.x * scale - minX;
			infoOrigin.y = origin.y * scale - minY;
		}

		else if(origin.type == Atlas::ShapeInfo::Origin::Type::Custom) {
			infoOrigin.x = origin.x * scale;
			infoOrigin.y = origin.y * scale;
		}

		auto [local, transform] = _toLocalOrigin(scaled, origin, scale);

		if(local.empty()) return Layer{};

		GradientInfo ginfo{
			.type = handle.type,
			.stops = handle.stops,
		};

		if(handle.type == GradientInfo::Type::Radial) {
			const slug_t cx = handle.x0 * scale - minX;
			const slug_t cy = handle.y0 * scale - minY;
			const slug_t r0 = handle.x1 * scale;
			const slug_t r1 = handle.y1 * scale;

			ginfo.innerRadius = r0;
			ginfo.transform = buildRadialGradientMatrix(cx, cy, r1);
		}

		else if(handle.type == GradientInfo::Type::Sweep) {
			const slug_t cx = handle.x0 * scale - minX;
			const slug_t cy = handle.y0 * scale - minY;

			ginfo.transform = buildSweepGradientMatrix(cx, cy, handle.x1, handle.y1 - handle.x1);
		}

		else {
			ginfo.transform = buildLinearGradientMatrix(
				handle.x0 * scale - minX, handle.y0 * scale - minY,
				handle.x1 * scale - minX, handle.y1 * scale - minY
			);
		}

		const uint32_t gid = _atlas.addGradient(ginfo);

		Atlas::ShapeInfo info;

		info.curves = std::move(local);
		info.contourStarts = contourStarts;
		info.origin = infoOrigin;

		_applySplits(info);
		_atlas.addShape(key, info);

#ifdef SLUGHORN_HAS_MSDF
		_applyMSDF(key);
#endif

		Layer layer{
			.key = key,
			.color = {},
			.gradientId = gid
		};

		placement.apply(transform.dx, transform.dy, layer.transform.x, layer.transform.y);

		_composite.layers.push_back(layer);

		return layer;
	}

	void _applySplits(Atlas::ShapeInfo& info) const {
		if(_splitStrategy) {
			auto [sx, sy] = _splitStrategy(info.curves);

			info.splitsX = std::move(sx);
			info.splitsY = std::move(sy);
		}

		else {
			info.splitsX = _splitsX;
			info.splitsY = _splitsY;
		}
	}

	static Atlas::Curves _scaleCurves(const Atlas::Curves& src, slug_t scale) {
		if(scale == 1_cv) return src;

		Atlas::Curves out;

		out.reserve(src.size());

		for(const auto& c : src) out.push_back({
			c.x1 * scale, c.y1 * scale,
			c.x2 * scale, c.y2 * scale,
			c.x3 * scale, c.y3 * scale
		});

		return out;
	}

	static std::pair<Atlas::Curves, Matrix> _toLocalOrigin(
		const Atlas::Curves& src,
		Atlas::ShapeInfo::Origin origin={},
		slug_t scale=1_cv
	) {
		if(src.empty()) return { {}, Matrix::identity() };

		slug_t minX = std::numeric_limits<slug_t>::max();
		slug_t minY = std::numeric_limits<slug_t>::max();
		slug_t maxX = -std::numeric_limits<slug_t>::max();
		slug_t maxY = -std::numeric_limits<slug_t>::max();

		for(const auto& c : src) {
			minX = std::min({minX, c.x1, c.x2, c.x3});
			minY = std::min({minY, c.y1, c.y2, c.y3});
			maxX = std::max({maxX, c.x1, c.x2, c.x3});
			maxY = std::max({maxY, c.y1, c.y2, c.y3});
		}

		if(maxX <= minX || maxY <= minY) return { {}, Matrix::identity() };

		Matrix transform = Matrix::identity();

		if(origin.type == Atlas::ShapeInfo::Origin::Type::Centered) {
			transform.dx = (minX + maxX) * 0.5_cv;
			transform.dy = (minY + maxY) * 0.5_cv;
		}

		else if(origin.type == Atlas::ShapeInfo::Origin::Type::Pivot) {
			transform.dx = origin.x * scale;
			transform.dy = origin.y * scale;
		}

		else if(origin.type == Atlas::ShapeInfo::Origin::Type::Custom) {
			// computeQuad subtracts originX/Y from transform, so pre-add it here so the
			// subtraction cancels out and the quad lands at the bbox corner (same as Default).
			// originX/Y still holds the raw user value for the shader.
			transform.dx = minX + origin.x * scale;
			transform.dy = minY + origin.y * scale;
		}

		else {
			transform.dx = minX;
			transform.dy = minY;
		}

		Atlas::Curves out;

		out.reserve(src.size());

		for(const auto& c : src) out.push_back({
			c.x1 - minX, c.y1 - minY,
			c.x2 - minX, c.y2 - minY,
			c.x3 - minX, c.y3 - minY
		});

		return { out, transform };
	}

	Atlas& _atlas;
	KeyIterator _key;
	Path _path;
	CompositeShape _composite;

	struct SavedState {
		Matrix ctm;

		bool autoMetrics;

		std::vector<slug_t> splitsX;
		std::vector<slug_t> splitsY;
	};

	Matrix _ctm = Matrix::identity();
	std::vector<SavedState> _ctmStack;
	std::vector<slug_t> _splitsX;
	std::vector<slug_t> _splitsY;
	Atlas::SplitStrategy _splitStrategy;
	bool _autoMetrics = true;

#ifdef SLUGHORN_HAS_MSDF
	// setMSDF() state - see its doc comment above. Applied by _applyMSDF(), called from every
	// layer-producing commit (_commitFill, _commitGradient, text()) -- same shape as
	// _applySplits() above: persisted per-Canvas toggle state, applied unconditionally at each
	// commit site, branching internally.
	bool _msdfEnabled = false;
	slug_t _msdfRange = 0.1_cv;
	Atlas::MSDFEdgeColoring _msdfColoring = Atlas::MSDFEdgeColoring::ByDistance;

	void _applyMSDF(Key key) {
		if(_msdfEnabled) _atlas.requestMSDF(key, _msdfRange, _msdfColoring);
	}
#endif
};

// ================================================================================================
// Debugging Helpers
// ================================================================================================

inline std::ostream& operator<<(std::ostream& os, const Path::Sample& s) {
	return os << "Sample(x=" << s.x << " y=" << s.y << " angle=" << s.angle << ")";
}

inline std::ostream& operator<<(std::ostream& os, const Path& p) {
	return os
		<< "Path(activeCurves=" << p._activeCurves.size()
		<< " pendingCurves=" << p._pendingCurves.size()
		<< " pen=(" << p._penX << "," << p._penY << ")"
		<< " ctm=" << p._ctm
		<< " ctmStack=" << p._ctmStack.size()
		<< " lutDirty=" << p._lutDirty
		<< " totalLength=" << p._totalLength << ")"
	;
}

inline std::ostream& operator<<(std::ostream& os, const Canvas::GradientHandle& g) {
	return os
		<< "GradientHandle(type=" << g.type
		<< " p0=(" << g.x0 << "," << g.y0 << ")"
		<< " p1=(" << g.x1 << "," << g.y1 << ")"
		<< " stops=" << g.stops.size() << ")"
	;
}

inline std::ostream& operator<<(std::ostream& os, const Canvas& c) {
	return os
		<< "Canvas(key=" << c._key
		<< " path=" << c._path
		<< " composite=" << c._composite << ")"
	;
}

}
}
