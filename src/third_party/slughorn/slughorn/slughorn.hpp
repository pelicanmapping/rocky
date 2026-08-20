#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <functional>
#include <numbers>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <ostream>

// TODO: Something like `slugi_t` for `uint32_t`, since it SEEMS to be the only non-float type
// we're using (beyond `size_t`).

inline constexpr uint8_t SLUGHORN_VERSION_MAJOR = 0;
inline constexpr uint8_t SLUGHORN_VERSION_MINOR = 0;
inline constexpr uint8_t SLUGHORN_VERSION_PATCH = 2;

namespace slughorn {

namespace detail {

// Streams each argument into an ostringstream via operator<<, in order, and returns the
// resulting string. Replaces the repeated "declare oss; oss << a << b << c; oss.str()"
// pattern used for one-off error/log messages throughout the codebase.
inline std::string to_sstr(const auto&... args) {
	std::ostringstream oss;

	((oss << args), ...);

	return oss.str();
}

}

// Intended to be used like this: `auto [major, minor, patch] = slughorn::versionNumbers();`
constexpr auto versionNumbers() {
	return std::make_tuple(
		SLUGHORN_VERSION_MAJOR,
		SLUGHORN_VERSION_MINOR,
		SLUGHORN_VERSION_PATCH
	);
}

inline std::string versionString() {
	return
		std::to_string(SLUGHORN_VERSION_MAJOR) + "." +
		std::to_string(SLUGHORN_VERSION_MINOR) + "." +
		std::to_string(SLUGHORN_VERSION_PATCH)
	;
}

using slug_t = float;

namespace literals {
	constexpr slug_t operator"" _cv(long double v) {
		return static_cast<slug_t>(v);
	}

	constexpr slug_t operator"" _cv(unsigned long long v) {
		return static_cast<slug_t>(v);
	}

	constexpr slug_t cv(auto x) {
		return static_cast<slug_t>(x);
	}
}

using namespace literals;

// Any type cv() can sensibly convert from - lets Scene (and future call sites) accept int
// literals, double, slug_t, etc. as query parameters without callers having to think about it.
template<typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;

inline constexpr slug_t PI_CV = cv(std::numbers::pi_v<slug_t>);
inline constexpr slug_t PI_2_CV = cv(std::numbers::pi_v<slug_t> / 2_cv);

// ================================================================================================
// Color
//
// Simple RGBA color in linear floating-point space (0.0 - 1.0). Used by Layer / CompositeShape and
// by the FT2 / Cairo / Skia headers. Convert to your graphics backend's native type at the
// boundary.
// ================================================================================================
struct Color {
	slug_t r = 0_cv;
	slug_t g = 0_cv;
	slug_t b = 0_cv;
	slug_t a = 1_cv;
};

// ================================================================================================
// Matrix
//
// Column-major 2-D affine transform (the same 6-float layout used by FreeType's FT_Matrix +
// FT_Vector, Cairo's cairo_matrix_t, and the font-unit matrices threaded through the COLRv1 paint
// graph).
//
// The transform maps a point (x, y) as:
//
// x' = xx * x + xy * y + dx
// y' = yx * x + yy * y + dy
//
// xx/yx/xy/yy are dimensionless ratios (already divided by 65536 for FreeType callers). dx/dy are
// in the same coordinate space as the points being transformed (em-units for glyph work).
// ================================================================================================
struct Matrix {
	slug_t xx = 1_cv, yx = 0_cv; // first column
	slug_t xy = 0_cv, yy = 1_cv; // second column
	slug_t dx = 0_cv, dy = 0_cv; // translation

	static Matrix identity() { return {}; }
	static Matrix translate(slug_t tx, slug_t ty) { return { .dx = tx, .dy = ty }; }
	static Matrix scale(slug_t sx, slug_t sy) { return { .xx = sx, .yy = sy }; }
	static Matrix rotate(slug_t angle) {
		const slug_t c = std::cos(angle), s = std::sin(angle);

		return { .xx = c, .yx = s, .xy = -s, .yy = c };
	}

	bool isIdentity() const {
		constexpr slug_t eps = 1e-6_cv;

		return
			std::abs(xx - 1_cv) < eps &&
			std::abs(yy - 1_cv) < eps &&
			std::abs(yx) < eps &&
			std::abs(xy) < eps &&
			std::abs(dx) < eps &&
			std::abs(dy) < eps
		;
	}

	// Apply this matrix to a point.
	//
	// TODO: Investigate some operator overload instead!
	void apply(slug_t x, slug_t y, slug_t& xo, slug_t& yo) const {
		xo = xx * x + xy * y + dx;
		yo = yx * x + yy * y + dy;
	}

	// Concatenate: returns (this * rhs), i.e. rhs is applied first.
	Matrix operator*(const Matrix& rhs) const {
		return {
			.xx = xx * rhs.xx + xy * rhs.yx,
			.yx = yx * rhs.xx + yy * rhs.yx,
			.xy = xx * rhs.xy + xy * rhs.yy,
			.yy = yx * rhs.xy + yy * rhs.yy,
			.dx = xx * rhs.dx + xy * rhs.dy + dx,
			.dy = yx * rhs.dx + yy * rhs.dy + dy,
		};
	}
};

// ================================================================================================
// Transform
//
// Placement descriptor for a Layer. Carries the world-space position of the layer (x, y) and
// an optional Z offset (z) for depth separation in 3D scenes. Replaces the old Matrix on Layer,
// which carried four unused linear fields (xx/xy/yx/yy) that computeQuad() explicitly ignored.
//
// GradientInfo continues to use Matrix because gradient geometry encoding genuinely uses the 2x2
// linear part (e.g. AffineRadial B-matrix).
// ================================================================================================
struct Transform {
	slug_t x = 0_cv, y = 0_cv, z = 0_cv;
};

inline std::ostream& operator<<(std::ostream& os, const Transform& t) {
	return os << "Transform(x=" << t.x << " y=" << t.y << " z=" << t.z << ")";
}

// ================================================================================================
// GradientStop / GradientInfo
//
// Describes a color ramp used for gradient fills. GradientInfo is registered with Atlas via
// addGradient() before build(); the returned ID is stored in Layer::gradientId to activate the
// gradient for that layer.
//
// The transform matrix encodes gradient geometry in local em-space:
//
// Linear: t = m.xx * emX + m.xy * emY + m.dx (build with buildLinearGradientMatrix)
// Radial: m.dx/dy = center; m.xx = outerRadius; innerRadius field = inner radius
//         t = (length(emCoord - center) - innerRadius) / (outerRadius - innerRadius)
//         (build with buildRadialGradientMatrix; set innerRadius separately)
// AffineRadial: m.dx/dy = center; m.xx/xy/yx/yy = 2x2 inverse affine B matrix
//               t = length(B * (emCoord - center)) - innerRadius (innerRadius in B-space)
//               (build with buildAffineRadialGradientMatrix; innerRadius usually 0)
//               B maps em-space deltas back to normalized gradient space; length(B*d)=1 at outer ellipse.
// ================================================================================================
struct GradientStop {
	// position along gradient axis [0, 1]
	slug_t t = 0_cv;

	Color color = {};
};

struct GradientInfo {
	enum class Type { Linear, Radial, Sweep, AffineRadial };

	Type type = Type::Linear;

	std::vector<GradientStop> stops = {};

	Matrix transform = {};

	// Radial: inner radius in local em-space (0 = point center). See buildRadialGradientMatrix.
	slug_t innerRadius = 0_cv;

	// Sweep only: arc range in turns [0, 1]. Default = full circle.
	slug_t startAngle = 0_cv;
	slug_t endAngle = 1_cv;
};

// ================================================================================================
// Scene
//
// Pixel <-> em-space measurement math. slughorn deals entirely in em-space (the normalized
// coordinate system shared by Atlas/Shape/Layer), but callers usually think in pixels: "make
// this stroke 1px wide", "size this shape to fill a 100x100 box", "give me a 2px margin". Scene
// is the single conversion factor (pixelsPerEm) plus the small set of arithmetic built on it.
//
// Scene is measurement math ONLY - it is not a scene graph. It does not know about cameras,
// transforms, or entity hierarchies; a consumer (e.g. osgSlug::scene()) derives
// pixelsPerEmX/Y from its own viewport + projection convention and hands back a calibrated Scene.
// ================================================================================================
enum class Fit { Contain, Cover }; // CSS object-fit semantics

struct Scene {
	slug_t pixelsPerEmX = 1_cv;
	slug_t pixelsPerEmY = 1_cv;

	// Conservative axis - safe for isotropic measurements like stroke width.
	slug_t pixelsPerEm() const { return std::min(pixelsPerEmX, pixelsPerEmY); }

	// Em-space value equivalent to n pixels (isotropic).
	template<Numeric T = slug_t>
	slug_t pixels(T n = T{1}) const { return cv(n) / pixelsPerEm(); }

	// Em-space halfWidth for a stroke of widthPx pixels total.
	template<Numeric T = slug_t>
	slug_t halfWidth(T widthPx = T{1}) const { return cv(widthPx) * 0.5_cv / pixelsPerEm(); }

	// Em-space halfWidth per axis - for oriented stamps or elliptical brushes.
	template<Numeric T = slug_t>
	slug_t halfWidthX(T widthPx = T{1}) const { return cv(widthPx) * 0.5_cv / pixelsPerEmX; }

	template<Numeric T = slug_t>
	slug_t halfWidthY(T widthPx = T{1}) const { return cv(widthPx) * 0.5_cv / pixelsPerEmY; }

	// Layer::scale that makes an em-space extent occupy targetPx pixels (single axis).
	template<Numeric T>
	slug_t scaleFor(slug_t emExtent, T targetPx) const {
		return cv(targetPx) / (emExtent * pixelsPerEm());
	}

	// Layer::scale fitting an em-space (emW x emH) box into (targetWpx x targetHpx), preserving
	// the shape's natural aspect ratio.
	template<Numeric TW, Numeric TH>
	slug_t scaleToFit(
		slug_t emW, slug_t emH,
		TW targetWpx, TH targetHpx,
		Fit mode = Fit::Contain
	) const {
		const slug_t sx = scaleFor(emW, targetWpx);
		const slug_t sy = scaleFor(emH, targetHpx);

		return mode == Fit::Contain ? std::min(sx, sy) : std::max(sx, sy);
	}

	// Em-size so cap-height glyphs render capHeightPx screen pixels tall at the current
	// pixelsPerEm rate. capHeightRatio comes from FontMetrics.
	template<Numeric T = slug_t>
	slug_t fromCapHeight(T capHeightPx, slug_t capHeightRatio) const {
		return pixels(capHeightPx) / capHeightRatio;
	}

	// Reverse: cap-height in screen pixels at the given em-space fontSize.
	slug_t toCapHeight(slug_t emSize, slug_t capHeightRatio) const {
		return emSize * capHeightRatio * pixelsPerEm();
	}
};

// ================================================================================================
// Quad
//
// Used both as a LITERAL "quad" for use in creating compatible 2D geometry and as a "bounding box"
// holder type.
// ================================================================================================
struct Quad {
	slug_t x0, y0; // bottom-left
	slug_t x1, y1; // top-right
};

// ================================================================================================
// Atlas (forward declaration - Key is needed by Layer)
// ================================================================================================
class Atlas;

// ================================================================================================
// Atlas::Key
//
// Discriminated union identifying a shape or composite shape in the Atlas. Two flavors:
//
// Key(uint32_t cp) - a Unicode codepoint (or any uint32_t ID).
// Key(const std::string&) - a named shape / composite ("logo", "axolotl", ...)
// Key(const char*) - string-literal convenience overload.
//
// The hash is computed once at construction and stored; KeyHash just returns it. operator== uses
// the hash as a fast pre-check, then falls back to value comparison. The two namespaces are kept
// disjoint by mixing the type tag into the hash seed, so a codepoint and a string that happen to
// produce the same raw hash will never collide.
// ================================================================================================
struct Key {
	enum class Type { Codepoint, Name };

	// Bit layout for Type::Codepoint keys -- packed into the same _codepoint field, no new struct
	// member: [0..20] real Unicode codepoint (max valid U+10FFFF fits in 21 bits), [21..28] an
	// opt-in caller-defined "mask" (namespace) that defaults to 0, [29..31] reserved for
	// KeyIterator::AUTO_KEY_START's auto-key range. Key(48) is bit-identical to Key(48, 0).
	static constexpr uint32_t CODEPOINT_BITS = 21;
	static constexpr uint32_t CODEPOINT_MASK = (1u << CODEPOINT_BITS) - 1; // 0x1FFFFF
	static constexpr uint32_t MASK_SHIFT = CODEPOINT_BITS;
	static constexpr uint32_t MASK_BITS = 8;
	static constexpr uint32_t MASK_MASK = ((1u << MASK_BITS) - 1) << MASK_SHIFT; // 0x1FE00000

	// With this in the public section:
	Key(): _type(Type::Codepoint), _codepoint(0), _hash(_hashCp(0)) {}

	Key(uint32_t cp): _type(Type::Codepoint), _codepoint(cp), _hash(_hashCp(cp)) {}

	// Opt-in namespacing: packs a caller-defined mask (0-255) alongside the real codepoint so two
	// unrelated sources (e.g. two fonts) can register the same raw codepoint without silently
	// aliasing one atlas entry onto the other's -- see NEXT_SESSION.md's "Key mask" design note.
	Key(uint32_t cp, uint8_t mask):
		_type(Type::Codepoint),
		_codepoint((cp & CODEPOINT_MASK) | (uint32_t(mask) << MASK_SHIFT)),
		_hash(_hashCp((cp & CODEPOINT_MASK) | (uint32_t(mask) << MASK_SHIFT)))
	{}

	Key(const std::string& name): _type(Type::Name), _name(name), _hash(_hashStr(name)) {}
	Key(const char* name): _type(Type::Name), _name(name), _hash(_hashStr(name)) {}

	// Accessors

	Type type() const { return _type; }

	// Only valid when type() == Codepoint. Returns the full raw packed value (mask bits included,
	// if any) -- Key(k.codepoint()) reconstructs an identical Key.
	uint32_t codepoint() const { return _codepoint; }

	// Only valid when type() == Codepoint. Real Unicode codepoint with any mask bits stripped.
	uint32_t realCodepoint() const { return _codepoint & CODEPOINT_MASK; }

	// Only valid when type() == Codepoint. Caller-defined namespace; 0 if unset.
	uint8_t mask() const { return uint8_t((_codepoint & MASK_MASK) >> MASK_SHIFT); }

	// Only valid when type() == Name.
	const std::string& name() const { return _name; }

	size_t hash() const { return _hash; }

	// Equality
	bool operator==(const Key& o) const {
		if(_hash != o._hash) return false;
		if(_type != o._type) return false;
		if(_type == Type::Codepoint) return _codepoint == o._codepoint;

		return _name == o._name;
	}

	bool operator!=(const Key& o) const { return !(*this == o); }

private:
	static size_t _hashCp(uint32_t cp) {
		// Mix a type tag (0) into the seed so the codepoint and name namespaces
		// cannot collide even if their raw hashes match.
		size_t h = std::hash<uint32_t>{}(cp);

		h ^= std::hash<size_t>{}(0) + 0x9e3779b9 + (h << 6) + (h >> 2);

		return h;
	}

	static size_t _hashStr(const std::string& s) {
		size_t h = std::hash<std::string>{}(s);

		h ^= std::hash<size_t>{}(1) + 0x9e3779b9 + (h << 6) + (h >> 2);

		return h;
	}

	Type _type = Type::Codepoint;

	uint32_t _codepoint = 0;
	std::string _name;

	size_t _hash = 0;
};

struct KeyHash {
	size_t operator()(const Key& k) const { return k.hash(); }
};

struct KeyIterator {
	// Default start is well above the valid Unicode codepoint range (max U+10FFFF = 1,114,111) so
	// an unprefixed counter can never collide with a real font glyph's Key(codepoint) -- see
	// AUTO_KEY_START's own comment. Passing an explicit _counter opts out of that protection (e.g.
	// deliberately reconstructing a specific sequence) -- on the caller's head at that point.
	static constexpr uint32_t AUTO_KEY_START = 0x20000000;

	KeyIterator(uint32_t _counter=AUTO_KEY_START): counter(_counter) {}

	// Prefixed iterators always produce Type::Name keys (Key(prefix + "_" + to_string(counter++))),
	// which can never collide with a Type::Codepoint key regardless of the number (Key's hash mixes
	// in a type tag; operator== checks _type first) -- so unlike the unprefixed constructor above,
	// there's no reason to inherit AUTO_KEY_START's offset here. Starts at 0 for readable names
	// ("prefix_0", "prefix_1", ...).
	KeyIterator(const char* _prefix, bool _force=false): prefix(_prefix), counter(0), force(_force) {}
	KeyIterator(std::string _prefix, bool _force=false): prefix(std::move(_prefix)), counter(0), force(_force) {}

	Key next() {
		if(prefix.empty()) return Key(counter++);

		return Key(prefix + "_" + std::to_string(counter++));
	}

	std::string prefix;

	// freetype.hpp's loadGlyphRange() registers codepoints as low as 32 (space) by default via
	// Key(uint32_t) -- the SAME type/constructor an unprefixed counter starting at 0 would use, so
	// it WILL eventually reach and silently overwrite a loaded glyph's atlas entry in any scene
	// with enough auto-keyed commits (mask()/textGlyph()/fill()/stroke()). AUTO_KEY_START gives
	// ~4000x headroom past U+10FFFF, still ~3.9 billion keys of runway. Deliberately NOT switched
	// to Type::Name (which can't collide with ANY numeric key by construction, codepoint or
	// otherwise): Name keys pay for std::string construction + std::hash<std::string> on every
	// single auto-keyed commit, real cost in a scene with many of them, for a collision class this
	// offset already fully closes.
	uint32_t counter = AUTO_KEY_START;

	// When true, backends should use next() even when a source element provides its own id.
	bool force = false;
};

enum class DrawMode: uint8_t {
	Visible = 0, // normal render - must stay 0 forever (backward compat)
	Hidden = 1, // in atlas, no draw call; temporary toggle
	Geometry = 2, // curves in atlas, no quad ever; path source for sampling/animation
	Mask = 3, // stencil source for subsequent layers (reserved; lands with masking)
};

enum class BlendMode: uint8_t {
	// Porter-Duff
	SrcOver = 0, // default; normal alpha composite - must stay 0 forever (backward compat)
	Src = 1,
	Dst = 2,
	SrcIn = 3,
	DstIn = 4,
	SrcOut = 5,
	DstOut = 6,
	SrcAtop = 7,
	DstAtop = 8,
	Xor = 9,
	Clear = 10,
	DstOver = 11,

	// Photoshop-style (require framebuffer read - handled via GL_KHR_blend_equation_advanced)
	Multiply = 20,
	Screen = 21,
	Overlay = 22,
	Darken = 23,
	Lighten = 24,
	ColorDodge = 25,
	ColorBurn = 26,
	HardLight = 27,
	SoftLight = 28,
	Difference = 29,
	Exclusion = 30
};

// ================================================================================================
// Mask
//
// Per-layer mask specification. Exactly one of two sources drives shape coverage:
//
// - type == MSDF -> frontend samples the MSDF tile baked for key
// - type != MSDF -> frontend evaluates a closed-form analytical SDF
//
// Like effectId/effectParam, slughorn stores intent and parameters; the frontend interprets them.
// An optional gradient modulates coverage multiplicatively (future field; placeholder comment).
// ================================================================================================
struct Mask {
	enum class Type : uint8_t {
		MSDF = 0, // key must be set; frontend samples the baked MSDF tile
		Circle, // params: cx, cy, r
		Rect, // params: x, y, w, h
		Capsule, // params: ax, ay, bx, by, r
		Arc, // params: cx, cy, r, angle_start, angle_end
		ArcBand, // params: cx, cy, r, angle_start, angle_end, stroke_half_width
		Hexagon, // params: cx, cy, r, rotation
		Octagon, // params: cx, cy, r, rotation
		Star, // params: cx, cy, r, points, inner_ratio, rotation
		// Future: Scanline, Slug
	};

	std::optional<Key> key = {};
	Type type = Type::MSDF;
	slug_t params[6] = {};
	bool invert = false;

	static Mask msdf(Key k, bool inv=false) {
		return {
			.key = std::move(k),
			.type = Type::MSDF,
			.invert = inv,
		};
	}

	static Mask circle(slug_t cx, slug_t cy, slug_t r, bool inv=false) {
		return {
			.type = Type::Circle,
			.params = { cx, cy, r },
			.invert = inv,
		};
	}

	static Mask rect(slug_t x, slug_t y, slug_t w, slug_t h, bool inv=false) {
		return {
			.type = Type::Rect,
			.params = { x, y, w, h },
			.invert = inv,
		};
	}

	static Mask capsule(slug_t ax, slug_t ay, slug_t bx, slug_t by, slug_t r, bool inv=false) {
		return {
			.type = Type::Capsule,
			.params = { ax, ay, bx, by, r },
			.invert = inv,
		};
	}

	static Mask arc(slug_t cx, slug_t cy, slug_t r, slug_t a0, slug_t a1, bool inv=false) {
		return {
			.type = Type::Arc,
			.params = { cx, cy, r, a0, a1 },
			.invert = inv,
		};
	}

	static Mask arcBand(slug_t cx, slug_t cy, slug_t r, slug_t a0, slug_t a1, slug_t rb, bool inv=false) {
		return {
			.type = Type::ArcBand,
			.params = { cx, cy, r, a0, a1, rb },
			.invert = inv,
		};
	}

	static Mask hexagon(slug_t cx, slug_t cy, slug_t r, slug_t rotation=0_cv, bool inv=false) {
		return {
			.type = Type::Hexagon,
			.params = { cx, cy, r, rotation },
			.invert = inv,
		};
	}

	static Mask octagon(slug_t cx, slug_t cy, slug_t r, slug_t rotation=0_cv, bool inv=false) {
		return {
			.type = Type::Octagon,
			.params = { cx, cy, r, rotation },
			.invert = inv,
		};
	}

	static Mask star(
		slug_t cx, slug_t cy, slug_t r,
		slug_t points, slug_t innerRatio, slug_t rotation=0_cv,
		bool inv=false
	) {
		return {
			.type = Type::Star,
			.params = { cx, cy, r, points, innerRatio, rotation },
			.invert = inv,
		};
	}
};

// ================================================================================================
// Layer
//
// Represents the "state" of a single `Shape` instance, and includes all of the information the
// caller will need to contextually process the `Shape` in whatever setting slughorn is being used.
// ================================================================================================
struct Layer {
	Key key = Key(0u);

	Color color{};

	// x/y place the shape in world space via Shape::computeQuad().
	// z is a depth offset for 3D scene placement (default 0).
	Transform transform = {};

	// TODO: Document WHEN and HOW this "should" be used!
	slug_t scale = 1_cv;

	// Shader effect to apply when rendering this layer; this value isn't used anywhere directly in
	// slughorn itself, but rather passed wholesale to the frontend.
	uint32_t effectId = 0;

	// Per-layer float hint passed to the frontend vertex shader (e.g. rotation speed, scale factor).
	// Like effectId, slughorn itself does not interpret this value.
	slug_t effectParam = 0_cv;

	// Gradient fill; 0 = flat color (layer.color used). Non-zero = 1-based index into the atlas
	// gradient list (registered via addGradient()). When non-zero, layer.color.rgb is ignored and
	// layer.color.a acts as a global opacity multiplier.
	uint32_t gradientId = 0;

	// Extra em-space CONTENT margin on each side of the quad, for effects that intentionally
	// draw outside the shape's true bounds (outer glow, drop shadow, MSDF spread). Named after
	// print's bleed: ink that extends past the trim line. This is NOT an antialiasing margin -
	// the rasterization/AA margin is the renderer's responsibility, computed live in the vertex
	// stage at pixel scale, never a CPU-side value. The authored quad is ground truth; a
	// renderer may only fudge rasterization space (outward, at the last moment), never the
	// content or the alignment/positioning the user authored.
	slug_t bleed = 0_cv;

	// TODO: Explain this more, once it settles.
	DrawMode drawMode = DrawMode::Visible;

	// TODO: Explain this more TOO, once it settles.
	BlendMode blendMode = BlendMode::SrcOver;
};

// ================================================================================================
// CompositeShape
//
// A simple container that combines/organizes multiple `Layer` instances into an entity that should
// be conceptually treated as a SINGLE `Shape`.
// ================================================================================================
struct CompositeShape {
	// TODO: Should there be an alias for this; something like `slughorn::Layers`?
	std::vector<Layer> layers = {};

	// Usage is obvious in text situations; in "pure shape" modes, can be used to help arrange
	// groups of `Shape` instances horizontally.
	slug_t advance = 0_cv;

	// Optional mask applied to the composited output of all layers. Mirrors how SVG <mask> and
	// CSS mask work: layers composite first, then the mask gates the result as a whole.
	std::optional<Mask> mask = std::nullopt;

	// --------------------------------------------------------------------------------------------
	// Layer access
	// --------------------------------------------------------------------------------------------

	size_t size() const { return layers.size(); }
	bool empty() const { return layers.empty(); }

	Layer& operator[](size_t i) { return layers[i]; }
	const Layer& operator[](size_t i) const { return layers[i]; }

	// Named lookup - throws std::out_of_range if key is not present.
	// Intended for post-finalize configuration (e.g. setting effectId on a named hand shape).
	// Because the atlas is a separate argument to boundingBox(), the same CompositeShape can be
	// reused with any compatible atlas - useful for font/convention swaps without re-authoring.
	Layer& layer(Key key) {
		for(auto& l : layers) if(l.key == key) return l;

		throw std::out_of_range("CompositeShape::layer: key not found");
	}

	const Layer& layer(Key key) const {
		for(const auto& l : layers) if(l.key == key) return l;

		throw std::out_of_range("CompositeShape::layer: key not found");
	}

	// --------------------------------------------------------------------------------------------
	// Geometry
	// --------------------------------------------------------------------------------------------

	// Returns the union of all layer quads in world space. Skips layers whose key is not in the
	// atlas (pre-build or mismatched atlas). Returns nullopt if no layer resolves.
	// Atlas is a parameter - not a member - so that CompositeShapes are atlas-agnostic and can
	// be used with any compatible atlas without re-authoring.
	std::optional<Quad> boundingBox(const Atlas& atlas) const;

	// --------------------------------------------------------------------------------------------
	// Future ideas (deferred until we have real use cases):
	//
	// TODO: merge(const CompositeShape&) - append another composite's layers into this one;
	//       useful for assembling scenes from independently-authored parts.
	//
	// TODO: filter(pred) -> CompositeShape - return a view/copy containing only layers that
	//       match a predicate; e.g. all layers with effectId != 0.
	//
	// TODO: reorder(std::span<Key>) - reorder layers by key list for z-order adjustment
	//       without rebuilding; only safe when shapes don't overlap significantly.
	//
	// TODO: hasLayer(Key) -> bool - safe pre-check before layer(Key) when existence is
	//       uncertain (optional clock hands, conditional HUD elements, etc.)
	//
	// TODO: setEffectId(Key, uint32_t) / setColor(Key, Color) - convenience setters that
	//       combine the layer() lookup + field assignment into one call.
	// --------------------------------------------------------------------------------------------
};

// ================================================================================================
// FontMetrics
//
// Dimensionless em-space ratios for a typeface. All ratio fields are in [0, 1] and are fractions of
// the em-square. Multiply by fontSize (em-size in world units) to get world-space distances at any
// render size. Produced by slughorn::freetype::loadFontMetrics / readFontMetrics; consumed by
// canvas::Canvas::text() and any other backend-agnostic layout code.
//
// DPI is deliberately absent; slughorn is display-agnostic.
// ================================================================================================

struct FontMetrics {
	// raw em units (e.g. 1000 or 2048); not a ratio
	slug_t unitsPerEM = 0_cv;

	// OS/2 sCapHeight / unitsPerEM (~0.72 for Latin)
	slug_t capHeightRatio = 0_cv;

	// OS/2 sxHeight / unitsPerEM (~0.53)
	slug_t xHeightRatio = 0_cv;

	// ascender / unitsPerEM (~0.80)
	slug_t ascenderRatio = 0_cv;

	// |descender| / unitsPerEM (~0.20)
	slug_t descenderRatio = 0_cv;

	// recommended line gap / unitsPerEM (0 if none)
	// lineHeight = fontSize * (1 + lineGapRatio)
	slug_t lineGapRatio = 0_cv;
};

// ================================================================================================
// Atlas
//
// Owns the two raw pixel buffers required by the Slug rendering algorithm (Lengyel 2017):
//
// - Curve texture (RGBA32F): packed quadratic Bezier control points
// - Band texture (RGBA16UI): band headers + curve index lists
//
// Atlas is completely backend-agnostic; it has no dependencies on OSG, VSG, raw OpenGL, or any
// other graphics library. After build() the caller retrieves TextureData descriptors and hands them
// to whatever graphics layer it is using (see osgSlug::Atlas for the OSG adapter).
//
// Keys are Atlas::Key values - either a codepoint (uint32_t, implicitly convertible) or a named
// string. Both shapes and composite shapes share the same key namespace; do not register the same
// key under both addShape() and addCompositeShape().
// ================================================================================================
class Atlas {
public:
	static constexpr uint32_t DEFAULT_TEXTURE_WIDTH = 512;

	// Atlas();
	Atlas(uint32_t texWidth=DEFAULT_TEXTURE_WIDTH);
	virtual ~Atlas();

	// A single quadratic Bezier segment: p1 -> p2 (control) -> p3.
	struct Curve {
		slug_t x1, y1; // start point
		slug_t x2, y2; // off-curve control point
		slug_t x3, y3; // end point
	};

	using Curves = std::vector<Curve>;
	using Contours = std::vector<Curves>;

	// Descriptor passed to addShape().
	//
	// Curves must be in em-normalized coordinates (same convention as FreeType's FT_LOAD_NO_SCALE
	// path, divided by units_per_EM).
	//
	// Set autoMetrics = true (the default) to derive width/height/bearing/advance automatically
	// from the curve bounding box. Set it to false and fill in the metric fields when you need
	// precise control (e.g. when forwarding FreeType's own metrics for a font glyph).
	//
	// numBandsX / numBandsY control the band grid dimensions independently.
	// 0 means "pick automatically" for that axis. Negative values are invalid.
	// Set both to the same value to get a square grid (equivalent to the old numBands scalar).
	struct ShapeInfo {
		Curves curves = {};

		// Subpath-start indices into `curves` (index 0 always explicit, unlike canvas::Path's
		// internal storage convention -- see canvas::Path::pendingContourStarts()). Populated by
		// Canvas's commit verbs, which know their own boundaries exactly; empty means "unknown,"
		// in which case getShapeContours() falls back to inferring boundaries from a coordinate
		// gap (the only option for callers that build ShapeInfo::curves directly, e.g.
		// freetype.hpp/nanosvg.hpp/cairo.hpp/blend2d.hpp/skia.hpp).
		std::vector<size_t> contourStarts = {};

		bool autoMetrics = true;

		slug_t bearingX = 0, bearingY = 0;
		slug_t width = 0, height = 0;
		slug_t advance = 0;

		// Signed: 0 means "pick automatically"; negative values are invalid.
		int numBandsX = 0;
		int numBandsY = 0;

		// Interior X split positions as normalized [0,1] fractions of the shape's X range (sorted
		// ascending). If non-empty, overrides numBandsX; resulting numBands = splitsX.size() + 1.
		std::vector<slug_t> splitsX = {};

		// Interior Y split positions as normalized [0,1] fractions of the shape's Y range (sorted
		// ascending). If non-empty, overrides numBandsY; resulting numBands = splitsY.size() + 1.
		std::vector<slug_t> splitsY = {};

		// Controls where the transform origin (Layer::transform.dx/dy) is placed relative to the
		// shape geometry.
		//
		// Origin() - Default: bbox corner (existing behavior).
		// Origin(Type) - type-only: Centered, or any future named variant (UpperLeft, etc.)
		// that needs no explicit coordinates.
		// Origin(px, py) - Custom: caller-specified pivot in authoring space; unambiguously
		// Custom because no other variant takes coordinates.
		//
		// Layer::transform.dx/dy will equal (px, py) scaled to em-space,
		// giving the GPU the correct rotation pivot.
		struct Origin {
			// Default - quad placed at bbox corner; originData = (0, 0).
			// Centered - quad placed at bbox center; originData = (width/2, height/2) in em-space.
			// Pivot - quad placed at (px, py) in authoring space; originData = pivot in local
			//         em-space (bbox-min subtracted). Use with osgSlug_Vertex_Rotate and similar
			//         helpers that expect a local-em-space anchor.
			// Custom - quad placed at bbox corner (same as Default); originData = (px, py) * scale
			//          stored verbatim. Use when the shader treats origin as raw user data (e.g. an
			//          ejection direction vector) and you own the coordinate math.
			enum class Type { Default, Centered, Pivot, Custom };

			Type type;

			slug_t x, y;

			Origin(): type(Type::Default), x(0_cv), y(0_cv) {}
			Origin(Type t): type(t), x(0_cv), y(0_cv) {}
			Origin(slug_t px, slug_t py): type(Type::Pivot), x(px), y(py) {}
			Origin(Type t, slug_t px, slug_t py): type(t), x(px), y(py) {}

			bool operator==(const Origin& o) const { return type == o.type && x == o.x && y == o.y; }
			bool operator!=(const Origin& o) const { return !(*this == o); }
		};

		Origin origin = {};
	};

	// Everything the renderer needs to draw one shape. Populated by build() and returned by
	// getShape().
	struct Shape {
		// Location of this shape's band header block in the band texture (texel coords)
		uint32_t bandTexX = 0, bandTexY = 0;

		// Band index clamping limits (numBands - 1)
		uint32_t bandMaxX = 0, bandMaxY = 0;

		// Band-space transform: bandCoord = emPos * bandScale + bandOffset
		slug_t bandScaleX = 0, bandScaleY = 0;
		slug_t bandOffsetX = 0, bandOffsetY = 0;

		// Metrics in em-space (normalized to the font's em square, or to the
		// bounding box of the curves when autoMetrics = true)
		slug_t bearingX = 0, bearingY = 0;
		slug_t width = 0, height = 0;
		slug_t advance = 0;

		// Em-space pivot offset. Shader reads this as the rotation/effect origin.
		// Computed from ShapeInfo::Origin during build(); see `origin` below for the input spec.
		slug_t originX = 0_cv, originY = 0_cv;

		// The origin spec supplied by the caller at addShape()/canvas time.
		// Preserved post-build for diagnostics (slughorn info) and for computeQuad
		// branching (Task 0 clean fix: Custom must not subtract originX/Y from transform).
		ShapeInfo::Origin origin;

		// Scanline Sweeper: start texel and curve count in the scanline curve texture.
		// scanlineCurveStart is an absolute texel index; each curve occupies 2 texels.
		// Both are 0 for shapes with no geometry.
		uint32_t scanlineCurveStart = 0;
		uint32_t scanlineCurveCount = 0;

		// Layer index in the MSDF Texture2DArray; -1 = no MSDF tile rendered for this shape yet
		// (either never requested, or requested pre-build and still queued -- see
		// Atlas::requestMSDF()). Set once Atlas::requestMSDF() actually renders the tile.
		int msdfLayer = -1;

		// Em-space SDF range used when this shape's tile was generated.
		// Packed alongside msdfLayer into effectData.z for the shader (16 bits per value).
		slug_t msdfRange = 0_cv;

		// Original em-space curves, retained post-build and post-load.
		// Enables canvas::glyphOutline() and strokeText() without re-running font backends.
		// Populated by Atlas::build() and by serial::read() via render::decode().
		Curves curves;

		// Same convention as ShapeInfo::contourStarts (index 0 explicit, empty = unknown).
		// Populated by Atlas::addShape() from ShapeInfo::contourStarts -- NEVER by serial::read():
		// render::decode() reconstructs `curves` from packed band/curve texture data on load,
		// which does not preserve original authoring order, so any persisted contourStarts would
		// silently split the WRONG boundaries. getShapeContours() falls back to its coordinate-gap
		// heuristic for any atlas loaded from disk, same as before this field existed.
		std::vector<size_t> contourStarts;

		// Compute the world-space bounding quad for this shape.
		//
		// transform.dx/dy places the shape in world space. scale converts the
		// shape's em-space metrics into world units.
		//
		// The returned quad is the TRUE authored quad - no padding, no margin, ever. This is
		// an API promise: the quad the user asks for IS the quad they get, so anchoring a
		// shape against another coordinate can never drift. Renderers needing extra
		// rasterization room (AA fringes) or content room (Layer::bleed) must add it
		// downstream, at render time, without disturbing these coordinates.
		//
		// The returned quad is relative to (0,0) - scene placement is the
		// caller's responsibility (e.g. osg::MatrixTransform).
		Quad computeQuad(const Transform& transform, slug_t scale=1_cv) const {
			const slug_t ox = (transform.x - originX) * scale;
			const slug_t oy = (transform.y - originY) * scale;

			return {
				ox + bearingX * scale,
				oy + (bearingY - height) * scale,
				ox + (bearingX + width) * scale,
				oy + bearingY * scale
			};
		}
	};

	// Size of the per-shape indirection table (both axes). Each shape's band block begins with two
	// consecutive blocks of this many texels: first Y, then X. Each texel's R channel holds the
	// band index for that quantized em-coordinate slot, giving O(1) lookup in the fragment shader.
	// Must match SLUG_INDIRECTION_SIZE in the fragment shader.
	//
	// TODO: The constants need to always be in-sync with this!
	static constexpr uint32_t INDIRECTION_SIZE = 32;

	// Width (in texels) of each gradient color strip. 256 gives 8-bit t precision; hardware
	// bilinear filtering smooths between texels. One row per gradient.
	static constexpr uint32_t GRADIENT_STRIP_WIDTH = 256;

	// --------------------------------------------------------------------------------------------
	// Raw texture descriptor returned after build() is called.
	//
	// The `bytes` member holds the complete pixel data in row-major order, ready to be uploaded to
	// a GPU texture (width/height are in texels); `format` tells the graphics backend how to
	// interpret the bytes:
	//
	// RGBA32F - four 32-bit floats per texel (curve texture)
	// RGBA16UI - four 16-bit unsigned ints per texel (band texture)
	// --------------------------------------------------------------------------------------------
	struct TextureData {
		enum class Format { RGBA32F, RGBA16UI, RGBA8, RGB32F };

		std::vector<uint8_t> bytes;

		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t depth = 0; // >0: array texture (number of layers); 0: 2D texture

		Format format = Format::RGBA32F;

		bool empty() const { return bytes.empty(); }
	};

	// --------------------------------------------------------------------------------------------
	// Packing statistics - populated by build(), valid after isBuilt() == true.
	//
	// Tracks texture memory usage and alignment waste introduced by
	// alignCursorForSpan() - the mechanism that ensures band curve lists never
	// straddle a row boundary (required for correct Slug shader behaviour).
	//
	// curveTexelsUsed + curveTexelsPadding <= curveTexelsTotal
	// (total may exceed their sum due to unused space at the end of the last row)
	// --------------------------------------------------------------------------------------------
	// Options for SDF/MSDF atlas rasterization. Pass to setSDFOptions() before build().
	// msdf=false: single-channel SDF (R replicated to RGB8).
	// msdf=true: 3-channel MSDF (RGB8, reconstruct with median(r,g,b) in shader).
	// Both paths require SLUGHORN_MSDF=ON (msdfgen). Without it, rasterizeSDFAtlas() is a no-op.
	struct SDFOptions {
		uint32_t tileSize = 128;
		slug_t range = 0.1_cv;
		uint32_t atlasWidth = 1024;
		bool msdf = false;
	};

	// UV record for one shape tile inside the packed SDF atlas texture.
	struct SDFRecord {
		// top-left corner (texels)
		uint32_t atlasX = 0, atlasY = 0;
		// tile dimensions (texels)
		uint32_t tileW = 0, tileH = 0;
	};

	// Packed SDF/MSDF texture + per-shape UV records.
	// Populated by build() when setSDFOptions() was called beforehand.
	struct SDFAtlas {
		TextureData texture; // RGB8
		std::unordered_map<Key, SDFRecord, KeyHash> recs;

		bool empty() const { return recs.empty(); }
	};

	struct PackingStats {
		// Curve texture (RGBA32F, 16 bytes/texel)
		uint32_t curveTexelsUsed = 0; // texels written with actual curve data
		uint32_t curveTexelsPadding = 0; // texels wasted to row-alignment bumps
		uint32_t curveTexelsTotal = 0; // width * height (allocated)

		// Band texture (RGBA16UI, 8 bytes/texel)
		uint32_t bandTexelsUsed = 0;
		uint32_t bandTexelsPadding = 0;
		uint32_t bandTexelsTotal = 0;

		// Real headroom indicators against the hard uint16_t limits enforced in
		// Atlas::build()'s band-packing loop (see the "Atlas band-truncation bug" writeup) --
		// curveUtilization()/bandUtilization() below CANNOT reveal how close a build came to
		// these, since curveTexelsTotal/bandTexelsTotal are sized to just barely fit whatever
		// data exists at any texWidth, so they read ~100% almost regardless of headroom.
		// bandMaxCount: largest single band's curve-index list across all shapes (limit 65535).
		// bandMaxOffset: largest per-shape cumulative band-data span, i.e. cursor - shapeStart,
		// across all shapes (limit 65535).
		uint32_t bandMaxCount = 0;
		uint32_t bandMaxOffset = 0;

		// Gradient texture (RGBA8, 4 bytes/texel; one GRADIENT_STRIP_WIDTH-wide row per
		// gradient; no padding)
		uint32_t gradientCount = 0;
		uint32_t gradientTexelsTotal = 0;

		// Shelf-packed SDF/MSDF atlas (RGBA8, 4 bytes/texel) - populated by rasterizeSDFAtlas()
		// when setSDFOptions() was called beforehand. All fields stay 0 otherwise.
		uint32_t sdfTileCount = 0; // number of shapes with a packed tile
		uint32_t sdfTexelsUsed = 0; // sum of each tile's w * h
		uint32_t sdfTexelsPadding = 0; // shelf-packing waste (atlas area not covered by a tile)
		uint32_t sdfTexelsTotal = 0; // atlas width * height (allocated)

		// Per-shape MSDF Texture2DArray (RGB32F, 12 bytes/texel) - updated incrementally by
		// requestMSDF() as shapes opt in. All fields stay 0 if never called.
		uint32_t msdfLayerCount = 0; // number of registered layers
		uint32_t msdfTileSize = 0; // width == height of each layer
		uint32_t msdfTexelsTotal = 0; // tileSize * tileSize * msdfLayerCount

		// Scanline Sweeper curve texture (RGBA32F, same format as curveTexels; no band indirection).
		// Sequential: 2 texels per monotonic quadratic, no alignment gaps.
		uint32_t scanlineTexelsTotal = 0;

		// Fraction of allocated texels actually containing live data [0, 1].
		slug_t curveUtilization() const {
			return curveTexelsTotal
				? cv(curveTexelsUsed) / cv(curveTexelsTotal)
				: 0.f
			;
		}

		slug_t bandUtilization() const {
			return bandTexelsTotal
				? cv(bandTexelsUsed) / cv(bandTexelsTotal)
				: 0.f
			;
		}

		slug_t sdfUtilization() const {
			return sdfTexelsTotal
				? cv(sdfTexelsUsed) / cv(sdfTexelsTotal)
				: 0.f
			;
		}

		// Fraction of live texels that are padding (not curve data) [0, 1]. High values suggest
		// band count or shape ordering could be improved.
		slug_t curvePaddingRatio() const {
			const uint32_t live = curveTexelsUsed + curveTexelsPadding;

			return live ? cv(curveTexelsPadding) / cv(live) : 0.f;
		}

		slug_t bandPaddingRatio() const {
			const uint32_t live = bandTexelsUsed + bandTexelsPadding;

			return live ? cv(bandTexelsPadding) / cv(live) : 0.f;
		}

		slug_t sdfPaddingRatio() const {
			const uint32_t live = sdfTexelsUsed + sdfTexelsPadding;

			return live ? cv(sdfTexelsPadding) / cv(live) : 0.f;
		}

		// GPU bytes allocated per channel, derived from each channel's fixed texture format.
		size_t curveBytes() const { return size_t(curveTexelsTotal) * 16; } // RGBA32F
		size_t bandBytes() const { return size_t(bandTexelsTotal) * 8; } // RGBA16UI
		size_t gradientBytes() const { return size_t(gradientTexelsTotal) * 4; } // RGBA8
		size_t sdfBytes() const { return size_t(sdfTexelsTotal) * 4; } // RGBA8
		size_t msdfBytes() const { return size_t(msdfTexelsTotal) * 12; } // RGB32F
		size_t scanlineBytes() const { return size_t(scanlineTexelsTotal) * 16; } // RGBA32F

		// Total GPU memory across every channel.
		size_t totalBytes() const {
			return
				curveBytes() + bandBytes() + gradientBytes() +
				sdfBytes() + msdfBytes() + scanlineBytes()
			;
		}
	};

	// --------------------------------------------------------------------------------------------
	// Split position constants
	//
	// The indirection table has INDIRECTION_SIZE (32) slots per axis. Valid interior split
	// positions are multiples of 1/32; any other value is snapped to the nearest slot edge.
	// SPLIT_01 through SPLIT_31 name every meaningful position. 0.0 and 1.0 are the implicit
	// band boundaries and must not be passed as splits.
	// --------------------------------------------------------------------------------------------
	static constexpr slug_t SPLIT_01 = 0.03125_cv;
	static constexpr slug_t SPLIT_02 = 0.06250_cv;
	static constexpr slug_t SPLIT_03 = 0.09375_cv;
	static constexpr slug_t SPLIT_04 = 0.12500_cv;
	static constexpr slug_t SPLIT_05 = 0.15625_cv;
	static constexpr slug_t SPLIT_06 = 0.18750_cv;
	static constexpr slug_t SPLIT_07 = 0.21875_cv;
	static constexpr slug_t SPLIT_08 = 0.25000_cv;
	static constexpr slug_t SPLIT_09 = 0.28125_cv;
	static constexpr slug_t SPLIT_10 = 0.31250_cv;
	static constexpr slug_t SPLIT_11 = 0.34375_cv;
	static constexpr slug_t SPLIT_12 = 0.37500_cv;
	static constexpr slug_t SPLIT_13 = 0.40625_cv;
	static constexpr slug_t SPLIT_14 = 0.43750_cv;
	static constexpr slug_t SPLIT_15 = 0.46875_cv;
	static constexpr slug_t SPLIT_16 = 0.50000_cv;
	static constexpr slug_t SPLIT_17 = 0.53125_cv;
	static constexpr slug_t SPLIT_18 = 0.56250_cv;
	static constexpr slug_t SPLIT_19 = 0.59375_cv;
	static constexpr slug_t SPLIT_20 = 0.62500_cv;
	static constexpr slug_t SPLIT_21 = 0.65625_cv;
	static constexpr slug_t SPLIT_22 = 0.68750_cv;
	static constexpr slug_t SPLIT_23 = 0.71875_cv;
	static constexpr slug_t SPLIT_24 = 0.75000_cv;
	static constexpr slug_t SPLIT_25 = 0.78125_cv;
	static constexpr slug_t SPLIT_26 = 0.81250_cv;
	static constexpr slug_t SPLIT_27 = 0.84375_cv;
	static constexpr slug_t SPLIT_28 = 0.87500_cv;
	static constexpr slug_t SPLIT_29 = 0.90625_cv;
	static constexpr slug_t SPLIT_30 = 0.93750_cv;
	static constexpr slug_t SPLIT_31 = 0.96875_cv;

	// --------------------------------------------------------------------------------------------
	// SplitStrategy
	//
	// A callable that accepts the shape's curves and returns {splitsX, splitsY} as normalized
	// [0,1] fraction vectors. Band count and placement algorithm are entirely encapsulated by
	// the callable; the backend never sees or passes band counts.
	//
	// Pass {} or nullptr to skip explicit splits and let addShape() use its
	// normal band-count path without an explicit placement strategy object.
	//
	// Example:
	//
	// Atlas::SplitStrategy adaptive = [](const Atlas::Curves& c) {
	//    return Atlas::computeAdaptiveSplits(c, 8, 8);
	// };
	using SplitStrategy = std::function<
		std::pair<std::vector<slug_t>, std::vector<slug_t>>(const Curves&)
	>;

	// --------------------------------------------------------------------------------------------
	// Split placement strategies
	//
	// All compute*Splits functions share the same contract:
	//
	// - Take curves + band counts, return numBands-1 normalized [0,1] fractions sorted ascending.
	// - Assign {splitsX, splitsY} directly to ShapeInfo::splitsX / splitsY before addShape().
	// - numBands <= 1 for either axis returns an empty vector for that axis.
	// --------------------------------------------------------------------------------------------

	// Sweep-line valley placement: projects each curve's bounding box onto each axis, builds a
	// curve-density profile, then places splits at lowest-density positions (valleys) so band
	// boundaries fall where fewest curves cross, minimizing per-fragment shader iterations.
	static std::pair<std::vector<slug_t>, std::vector<slug_t>> computeAdaptiveSplits(
		const Curves& curves,
		int numBandsX,
		int numBandsY
	);

	// Uniform placement: evenly-spaced fractions (i+1)/numBands. curves is unused but present
	// for interface consistency with other compute*Splits strategies.
	//
	// All paths (uniform and adaptive) route through the indirection table; there is no separate
	// fast path. Use this for inspection, the band editor's "Reset to uniform" action, or
	// benchmarking uniform vs adaptive placement quality.
	static std::pair<std::vector<slug_t>, std::vector<slug_t>> computeUniformSplits(
		const Curves& curves,
		int numBandsX,
		int numBandsY
	);

	// --------------------------------------------------------------------------------------------
	// Population (call before build())
	// --------------------------------------------------------------------------------------------

	// Register a gradient. Returns a 1-based ID (0 = error / atlas already built).
	// The ID is stored in Layer::gradientId to activate the gradient for that layer.
	// Must be called before build(). Gradients are rasterized into the gradient atlas texture
	// during build(); adding one after build() has no effect on rendering.
	uint32_t addGradient(const GradientInfo& info);

	// Opt in to SDF/MSDF atlas generation. Must be called before build().
	// When set, build() will call rasterizeSDFAtlas() after packTextures(), producing a packed
	// RGB8 texture retrievable via getSDFAtlasData(). No-op if called after build().
	void setSDFOptions(const SDFOptions& opts) { if(!_built) _sdfOptions = opts; }

	// Opt in to scanline curve texture generation. Must be called before build().
	// When set, build() decomposes every shape's curves into monotonic segments and packs them
	// into a flat RGBA32F texture retrievable via getScanlineCurveTextureData().
	// No-op if called after build().
	void enableScanlineData() { if(!_built) _scanlineEnabled = true; }

	// Register a geometry shape under @p key.
	//
	// Must be called before build(). Calling addShape() with an already-registered key silently
	// replaces the previous definition.
	//
	// Shapes with empty curve lists are stored as metric-only entries (useful for whitespace
	// characters that need an advance but no visible geometry).
	void addShape(Key key, const ShapeInfo& desc);

	// Register a composite shape (ordered layer stack) under @p key.
	//
	// CompositeShapes are stored separately from geometry shapes and are not processed by build().
	// They can be registered before or after build() - the atlas does not need to be rebuilt when
	// new composites are added. Calling addCompositeShape() with an already-registered key silently
	// replaces the previous definition.
	void addCompositeShape(Key key, CompositeShape composite);

	// Force all shapes in @p keys to share the same em-space bounding box.
	//
	// Must be called after addShape() but before build(). Keys not present in the atlas
	// or shapes with no curves (e.g. whitespace) are silently skipped.
	//
	// When all valid shapes share the same advance (tabular/monospaced), the cell width
	// equals that advance and height/bearingY are the union of all shapes. When advances
	// differ, the cell is the axis-aligned union of all per-shape bounding boxes.
	//
	// Only the layout fields (bearingX/Y, width, height, advance) are updated. Per-shape
	// band transforms (bandScale*, bandOffset*, bandMax*) are left intact; the GPU quad
	// is determined by the shared metrics while each shape's coverage is still computed
	// from its own band data. This is the correct invariant for setLayerShapeIndex cycling.
	void normalizeShapeMetrics(const std::vector<Key>& keys);

#if 0
	// Override the em-space layout metrics for a single shape.
	//
	// Must be called after addShape() but before build(). The key must be present in the atlas;
	// if not, this is a no-op.
	//
	// Use this when a shape must cover a known em-space extent; e.g. for GPU tiling where
	// emCoords must span exactly [0,1]x[0,1]: call setShapeMetrics(key, 0, 1, 1, 1).
	//
	// Only the layout fields (bearingX/Y, width, height) are updated. Band transforms and
	// advance are left intact.
	void setShapeMetrics(const Key& key, slug_t bearingX, slug_t bearingY, slug_t width, slug_t height);
#endif

	// --------------------------------------------------------------------------------------------
	// Build (call once, then the atlas is frozen for geometry)
	// --------------------------------------------------------------------------------------------

	// Pack all registered shapes into the raw pixel buffers.
	//
	// After build() returns, addShape() must not be called again. Calling build() a second time is
	// a no-op (guarded internally). addCompositeShape() may still be called after build().
	void build();

	bool isBuilt() const { return _built; }

	// --------------------------------------------------------------------------------------------
	// Accessors (geometry valid after build(); composites valid any time after registration)
	// --------------------------------------------------------------------------------------------

	const CompositeShape* getCompositeShape(Key key) const;

	// Returns all info for a key - layout metrics, curves, origin - searching _shapes
	// (post-build) then _build (pre-build). Works at any build lifecycle stage.
	// Returns std::nullopt if the key is not registered.
	std::optional<Shape> getShape(Key key) const;

	// Returns em-space curves split into closed contours (outer outline, counters, etc.).
	// Contour breaks are detected where p3 of curve[i] != p1 of curve[i+1].
	// Returns an empty vector if the key is not registered. Works at any build lifecycle stage.
	Contours getShapeContours(Key key) const;

	const TextureData& getCurveTextureData() const { return _curveData; }
	const TextureData& getBandTextureData() const { return _bandData; }
	const TextureData& getGradientTextureData() const { return _gradientData; }
	const TextureData& getScanlineCurveTextureData() const { return _scanlineCurveData; }
	const SDFAtlas& getSDFAtlasData() const { return _sdfAtlas; }

	// --------------------------------------------------------------------------------------------
	// Per-shape MSDF opt-in (requires SLUGHORN_MSDF=ON)
	//
	// Call setMSDFTileSize() once before the first requestMSDF() call to set the tile dimensions
	// for the entire atlas. All layers in a sampler2DArray must be identical - mixing sizes
	// is a hard GPU constraint. Defaults to 128 if never called.
	//
	// Call requestMSDF() for each shape whenever it's convenient -- authoring time (before
	// build(), the common case: right after addShape()/Canvas::fill()/Canvas::mask() commits the
	// shape) or after build(), same as the old registerMSDF() name required. Pre-build calls are
	// queued and actually rendered inside build() itself (tile rendering needs each shape's final
	// position in the packed atlas texture, which build() computes); post-build calls render
	// immediately, same as before. Either way there is no second "remember to come back after
	// build()" step -- requestMSDF() is safe to call exactly once, wherever it's naturally reached
	// in authoring order.
	// range controls the em-space spread of the distance gradient (default 0.1).
	// coloring selects the msdfgen edge-coloring algorithm: ByDistance eliminates corner
	// spike artifacts at the cost of slightly more CPU work; Simple is faster but prone
	// to artifacts at convex corners.
	//
	// getMSDFTextureData() packs registered tiles into a single RGB32F TextureData on first
	// call (lazy). depth == number of layers; width == height == tileSize.
	//
	// Shape::msdfLayer is updated in-place once a tile is actually rendered, so callers can read
	// it from getShape() without a separate lookup -- but note that for a pre-build requestMSDF()
	// call, that update doesn't happen until build() drains the queue, not at the requestMSDF()
	// call site itself.
	// --------------------------------------------------------------------------------------------

#ifdef SLUGHORN_HAS_MSDF
	enum class MSDFEdgeColoring { Simple, ByDistance };

	void setMSDFTileSize(uint32_t tileSize);
	uint32_t getMSDFTileSize() const;

	// Returns the assigned layer index once rendered, or -1 if the call was queued (pre-build)
	// rather than rendered immediately -- read Shape::msdfLayer via getShape() after build() to
	// recover it in that case.
	int requestMSDF(
		Key key,
		slug_t range=0.1_cv,
		MSDFEdgeColoring coloring=MSDFEdgeColoring::ByDistance
	);

	void requestMSDF(
		const std::vector<Key>& keys,
		slug_t range=0.1_cv,
		MSDFEdgeColoring coloring=MSDFEdgeColoring::ByDistance
	);

	const TextureData& getMSDFTextureData() const;
#endif

	// Always available; returns -1 when SLUGHORN_MSDF is off or the shape was not registered.
	int getMSDFLayer(Key key) const {
#ifdef SLUGHORN_HAS_MSDF
		auto it = _msdfLayerMap.find(key);

		if(it != _msdfLayerMap.end()) return it->second;
#endif
		return -1;
	}

	// Gradient list (valid after build() if any gradients were registered).
	const std::vector<GradientInfo>& getGradients() const { return _gradients; }

	// Valid after build(). Reports texture utilization and alignment padding waste.
	// See PackingStats for field documentation.
	const PackingStats& getPackingStats() const { return _packingStats; }

	// Bulk accessors - primarily for serialization (slughorn/serial.hpp) and
	// diagnostics. Prefer getShape() / getCompositeShape() for normal use.
	const std::unordered_map<Key, Shape, KeyHash>& getShapes() const {
		return _shapes;
	}

	const std::unordered_map<Key, CompositeShape, KeyHash>& getCompositeShapes() const {
		return _compositeShapes;
	}

	bool hasKey(Key key) const {
		return _build.count(key) || _shapes.count(key) || _compositeShapes.count(key);
	}

	// --------------------------------------------------------------------------------------------
	// Serial reconstruction (used by slughorn/serial.hpp - not for general use)
	//
	// Injects pre-built texture data and shape/composite maps directly, bypassing build().
	// The Atlas is marked as built on return; calling build() afterwards is a no-op.
	// --------------------------------------------------------------------------------------------
	struct SerialData {
		TextureData curveData;
		TextureData bandData;
		TextureData gradientData;
		TextureData msdfData;
		PackingStats packingStats;
		std::unordered_map<Key, Shape, KeyHash> shapes;
		std::unordered_map<Key, CompositeShape, KeyHash> composites;
		std::vector<GradientInfo> gradients;
	};

	void loadFromSerial(SerialData&& sd) {
		// _texWidth must match whatever width the atlas was actually BUILT with, not
		// whatever DEFAULT_TEXTURE_WIDTH happens to be at deserialization time -- every
		// row/column texel calculation (including the shader's band-offset bit-shift math)
		// depends on it. curveData/bandData both carry the real width from the file.
		_texWidth = sd.curveData.width;

		_curveData = std::move(sd.curveData);
		_bandData = std::move(sd.bandData);
		_gradientData = std::move(sd.gradientData);
		_gradients = std::move(sd.gradients);
		_packingStats = sd.packingStats;
		_shapes = std::move(sd.shapes);
		_compositeShapes = std::move(sd.composites);
#ifdef SLUGHORN_HAS_MSDF
		if(!sd.msdfData.bytes.empty()) {
			_msdfData = std::move(sd.msdfData);
			_msdfDirty = false;
			_msdfTileSize = _packingStats.msdfTileSize;

			for(const auto& [key, shape] : _shapes) {
				if(shape.msdfLayer >= 0) _msdfLayerMap[key] = shape.msdfLayer;
			}
		}
#endif
		_built = true;
	}

	// Get the texture width used for this atlas
	uint32_t getTextureWidth() const { return _texWidth; }

private:
	// --------------------------------------------------------------------------------------------
	// Internal build structures (discarded after build())
	// --------------------------------------------------------------------------------------------
	struct BandEntry {
		uint16_t curveCount = 0;
		std::vector<size_t> curveIndices;
	};

	struct ShapeBuild {
		Shape metrics;
		Curves curves;
		std::vector<BandEntry> hbands;
		std::vector<BandEntry> vbands;
		// Input splits forwarded from ShapeInfo; consumed by buildShapeBands().
		std::vector<slug_t> splitsY;
		std::vector<slug_t> splitsX;
		// Indirection tables built by buildShapeBands(). Each has INDIRECTION_SIZE entries;
		// entry q holds the band index for em-coordinate fraction q/INDIRECTION_SIZE.
		std::vector<uint8_t> indirY;
		std::vector<uint8_t> indirX;
	};

	// --------------------------------------------------------------------------------------------
	// Internal pipeline
	// --------------------------------------------------------------------------------------------
	void buildShapeBands(
		Key key,
		ShapeBuild& build,
		uint32_t numBandsX,
		uint32_t numBandsY,
		bool overrideMetrics,
		ShapeInfo::Origin origin=ShapeInfo::Origin{}
	);

	void packTextures();
	void rasterizeGradients();
	void rasterizeSDFAtlas();

#ifdef SLUGHORN_HAS_MSDF
	// Shared core of requestMSDF()'s immediate path and build()'s drain of _pendingMSDF: filters
	// already-registered keys, renders remaining tiles (parallel when SLUGHORN_HAS_PARALLEL),
	// commits serially for deterministic layer ordering. Assumes every key already exists in
	// _shapes -- callers are responsible for that check (differs by pre/post-build call site).
	void _commitMSDF(const std::vector<Key>& keys, slug_t range, MSDFEdgeColoring coloring);
#endif

	// --------------------------------------------------------------------------------------------
	// Data
	// --------------------------------------------------------------------------------------------
	std::unordered_map<Key, ShapeBuild, KeyHash> _build; // discarded after build()
	std::unordered_map<Key, Shape, KeyHash> _shapes; // live after build()
	std::unordered_map<Key, CompositeShape, KeyHash> _compositeShapes; // live always

	TextureData _curveData;
	TextureData _bandData;
	TextureData _gradientData;
	TextureData _scanlineCurveData;

	std::vector<GradientInfo> _gradients;

	std::optional<SDFOptions> _sdfOptions;
	SDFAtlas _sdfAtlas;

#ifdef SLUGHORN_HAS_MSDF
	std::unordered_map<Key, int, KeyHash> _msdfLayerMap;
	std::vector<std::vector<slug_t>> _msdfTileData; // raw RGB32F floats per registered tile
	uint32_t _msdfTileSize = 0;
	mutable TextureData _msdfData; // packed lazily by getMSDFTextureData()
	mutable bool _msdfDirty = false; // set true once a tile is actually rendered, cleared on pack

	// requestMSDF() calls made before build() -- rendered has no valid atlas-texture position for
	// a shape until build()'s packTextures() runs, so these wait and get drained there instead.
	struct PendingMSDF { Key key; slug_t range; MSDFEdgeColoring coloring; };
	std::vector<PendingMSDF> _pendingMSDF;
#endif

	PackingStats _packingStats; // populated by packTextures()

	bool _built = false;
	bool _scanlineEnabled = false;

	uint32_t _texWidth;
};

// ================================================================================================
// buildLinearGradientMatrix
//
// Converts two em-space endpoints into the affine matrix that maps the gradient axis onto [0,1]:
//
// t = m.xx * emX + m.xy * emY + m.dx
//
// The result should be stored in GradientInfo::transform and is consumed directly by the
// a_gradientXform vertex attribute (x=m.xx, y=m.xy, z=m.dx).
//
// Returns Matrix::identity() for degenerate (zero-length) inputs.
// ================================================================================================
inline Matrix buildLinearGradientMatrix(slug_t x0, slug_t y0, slug_t x1, slug_t y1) {
	const slug_t dx = x1 - x0;
	const slug_t dy = y1 - y0;
	const slug_t lenSq = dx * dx + dy * dy;

	if(lenSq < 1e-12_cv) return Matrix::identity();

	const slug_t invLenSq = 1_cv / lenSq;

	// yx, yy, dy are unused for linear gradient t computation
	return {
		.xx = dx * invLenSq,
		.xy = dy * invLenSq,
		.dx = -(x0 * dx + y0 * dy) * invLenSq,
	};
}

// ================================================================================================
// buildRadialGradientMatrix
//
// Encodes the center and outer radius of a concentric radial gradient into GradientInfo::transform:
//
// m.dx = cx (center X in local em-space)
// m.dy = cy (center Y in local em-space)
// m.xx = r1 (outer radius in local em-space)
//
// The inner radius is stored separately in GradientInfo::innerRadius. Drawable.cpp reads both to
// compute the GPU-packed xform: (cx, cy, r0*invDR, invDR) where invDR = 1/(r1 - r0).
//
// Shader formula: t = length(emCoord - center) * invDR - r0 * invDR
//                   = (length(emCoord - center) - r0) / (r1 - r0), clamped to [0, 1].
// ================================================================================================
inline Matrix buildRadialGradientMatrix(slug_t cx, slug_t cy, slug_t r1) {
	return { .xx = r1, .dx = cx, .dy = cy };
}

// ================================================================================================
// buildAffineRadialGradientMatrix
//
// Encodes a full 2x2 inverse affine B matrix for an elliptical radial gradient:
//
// m.dx = cx, m.dy = cy (center in local em-space)
// m.xx = B[0,0], m.xy = B[0,1], m.yx = B[1,0], m.yy = B[1,1]
//
// B maps em-space deltas back to normalized gradient space.
// length(B * (emCoord - center)) == 1 at the outer ellipse.
//
// Used by the NanoSVG backend where objectBoundingBox gradients are elliptical.
// Canvas/FreeType circular radials continue to use buildRadialGradientMatrix.
// ================================================================================================
inline Matrix buildAffineRadialGradientMatrix(
	slug_t cx, slug_t cy,
	slug_t b00, slug_t b01,
	slug_t b10, slug_t b11
) {
	return {
		.xx = b00,
		.yx = b10,
		.xy = b01,
		.yy = b11,
		.dx = cx,
		.dy = cy,
	};
}

// ================================================================================================
// buildSweepGradientMatrix
//
// Encodes the center and angular range of a sweep (conic) gradient into GradientInfo::transform:
//
// m.dx = cx (center X in local em-space)
// m.dy = cy (center Y in local em-space)
// m.xx = startAngle (radians, measured from +X axis)
// m.xy = arcSpan (endAngle - startAngle, radians; must be > 0)
//
// Drawable.cpp packs: (cx, cy, startAngle, -invArcSpan) where invArcSpan = 1/arcSpan.
// The negative w value is the type discriminator (w==0 = linear, w>0 = radial, w<0 = sweep).
//
// Shader formula: t = (atan2(emCoord.y - cy, emCoord.x - cx) - startAngle) / arcSpan
//
// For a seam-free full-circle gradient, use startAngle = -a and arcSpan = 2a. atan2 returns
// values in [-a, a], which exactly covers the sweep, so t goes cleanly from 0 to 1.
// ================================================================================================
inline Matrix buildSweepGradientMatrix(slug_t cx, slug_t cy, slug_t startAngle, slug_t arcSpan) {
	return { .xx = startAngle, .xy = arcSpan, .dx = cx, .dy = cy };
}

// ================================================================================================
// CurveDecomposer
//
// Stateful helper that accepts path commands (moveTo / lineTo / quadTo / cubicTo) and appends the
// equivalent quadratic Bezier segments to a Curves vector.
//
// Cubic segments are handled adaptively via De Casteljau subdivision: each cubic is recursively
// split at its midpoint until the segment is flat enough (both control points within `tolerance`
// of the p0->p3 chord), at which point it is emitted as two quadratics - one for each half of the
// flattened cubic. This produces far fewer curves than blind subdivision for gentle cubics while
// faithfully tracking tight arcs and S-curves.
//
// The two-quadratic leaf matches NanoSVG's own output convention (e.g. a triangle in NanoSVG
// produces 6 quadratic curves, not 3), which keeps curve-texture alignment predictable.
//
// `tolerance` is in the same coordinate space as the curves (em-units by default). The default
// value (1e-4) is suitable for em-normalized [0,1] geometry. Scale it proportionally if your
// authoring space uses different units - e.g. for a 1000-unit em square use 0.1f.
//
// All backends (Cairo, NanoSVG, Skia, Canvas) route cubics through cubicTo and therefore benefit
// from this improvement automatically. Callers that want coarser/faster decomposition can raise
// `tolerance`; callers that want higher fidelity (e.g. very small text) can lower it.
// ================================================================================================
static constexpr slug_t TOLERANCE_DRAFT = 1e-2f; // fast, visible only at large sizes
static constexpr slug_t TOLERANCE_BALANCED = 1e-3f; // good default for screen work
static constexpr slug_t TOLERANCE_FINE = 1e-4f; // high-DPI / print / export

// This is the DEFAULT value, and always produces the predictable (fast, lowest-quality)
// "two-leafs" from a cubiz Bezier.
static constexpr slug_t TOLERANCE_EXACT = std::numeric_limits<slug_t>::max();

struct CurveDecomposer {
	Atlas::Curves& curves;

	// Flatness threshold in curve-space units. A cubic is considered flat enough to emit when both
	// interior control points are within this distance of the p0->p3 chord. Default suits
	// em-normalized [0,1] geometry.
	slug_t tolerance = TOLERANCE_EXACT;

	slug_t _x = 0_cv;
	slug_t _y = 0_cv;
	slug_t _sx = 0_cv;
	slug_t _sy = 0_cv;

	CurveDecomposer(Atlas::Curves& c): curves(c) {}

	void moveTo(slug_t x, slug_t y) {
		_requireFinite({x, y});

		_x = _sx = x;
		_y = _sy = y;
	}

	void lineTo(slug_t x3, slug_t y3) {
		_requireFinite({x3, y3});

		curves.push_back({
			_x, _y,
			(_x + x3) * 0.5_cv,
			(_y + y3) * 0.5_cv,
			x3, y3
		});

		_x = x3;
		_y = y3;
	}

	void quadTo(slug_t cx, slug_t cy, slug_t x3, slug_t y3) {
		_requireFinite({cx, cy, x3, y3});

		curves.push_back({_x, _y, cx, cy, x3, y3});

		_x = x3;
		_y = y3;
	}

	// Adaptive cubic -> quadratic decomposition via De Casteljau subdivision.
	//
	// Recursively splits the cubic until flat enough, then emits two quadratics
	// at the leaf. MAX_DEPTH caps recursion for degenerate inputs. Backends that
	// previously called cubicTo() get improved fidelity automatically; no call
	// site changes required.
	void cubicTo(
		slug_t c1x, slug_t c1y,
		slug_t c2x, slug_t c2y,
		slug_t x3, slug_t y3
	) {
		_requireFinite({c1x, c1y, c2x, c2y, x3, y3});

		_cubicAdaptive(_x, _y, c1x, c1y, c2x, c2y, x3, y3, 0);

		_x = x3;
		_y = y3;
	}

	// Close the current subpath by drawing a line back to the subpath start.
	// No-op if the current position is already at the start (degenerate close).
	void close() {
		constexpr slug_t eps = 1e-6_cv;

		if(std::abs(_x - _sx) > eps || std::abs(_y - _sy) > eps) lineTo(_sx, _sy);
	}

	// Snapshot the current write position for a later reverseFrom() call.
	size_t mark() const { return curves.size(); }

	// Reverse the winding of curves[begin..end) in-place: swap each curve's
	// endpoints (control point stays put) then reverse the sequence order.
	// Works for any contiguous range, e.g. a single sub-path captured via mark().
	static void reverseCurves(Atlas::Curves& curves, size_t begin, size_t end) {
		for(size_t i = begin; i < end; i++) {
			auto& c = curves[i];

			std::swap(c.x1, c.x3);
			std::swap(c.y1, c.y3);
		}

		std::reverse(
			curves.begin() + static_cast<std::ptrdiff_t>(begin),
			curves.begin() + static_cast<std::ptrdiff_t>(end)
		);
	}

	// Reverse all curves appended since mark().
	void reverseFrom(size_t pos) { reverseCurves(curves, pos, curves.size()); }

private:
	// Rejects non-finite (NaN/Inf) coordinates at the ingestion boundary. A NaN reaching
	// buildShapeBands' band-sort comparator violates strict weak ordering (all comparisons
	// against NaN are false), which is undefined behavior for std::sort.
	static void _requireFinite(std::initializer_list<slug_t> coords) {
		for(slug_t v : coords) {
			if(!std::isfinite(v)) {
				throw std::runtime_error("CurveDecomposer: non-finite coordinate in path data");
			}
		}
	}

	// Maximum recursion depth. Prevents infinite loops on degenerate/malformed
	// input. At depth 8 the maximum chord error is already <0.4% of the original
	// cubic's bounding box, which is well below any practical rendering threshold.
	static constexpr int MAX_DEPTH = 8;

	// Squared distance from point (px,py) to the infinite line through (ax,ay)->(bx,by).
	// Using squared distance avoids a sqrt in the hot path; we compare against tolerance2.
	static slug_t _pointToLineDistSq(
		slug_t px, slug_t py,
		slug_t ax, slug_t ay,
		slug_t bx, slug_t by
	) {
		const slug_t dx = bx - ax;
		const slug_t dy = by - ay;
		const slug_t lenSq = dx*dx + dy*dy;

		if(lenSq < 1e-12_cv) {
			// Degenerate chord - just return distance to the start point.
			const slug_t ex = px - ax;
			const slug_t ey = py - ay;

			return ex*ex + ey*ey;
		}

		// |cross(b-a, a-p)| / |b-a| - perpendicular distance
		const slug_t cross = dx*(ay - py) - dy*(ax - px);

		return (cross * cross) / lenSq;
	}

	// Returns true when both interior control points of the cubic lie within
	// `tolerance` of the p0->p3 chord - i.e. the cubic is visually flat.
	bool _flatEnough(
		slug_t p0x, slug_t p0y,
		slug_t p1x, slug_t p1y,
		slug_t p2x, slug_t p2y,
		slug_t p3x, slug_t p3y
	) const {
		const slug_t tolSq = tolerance * tolerance;

		return
			_pointToLineDistSq(p1x, p1y, p0x, p0y, p3x, p3y) <= tolSq &&
			_pointToLineDistSq(p2x, p2y, p0x, p0y, p3x, p3y) <= tolSq
		;
	}

	// Emit the flat-enough (or max-depth) cubic as two quadratics via midpoint split.
	// This is the leaf operation - matches the original cubicTo behavior and NanoSVG convention.
	void _emitTwoQuads(
		slug_t p0x, slug_t p0y,
		slug_t p1x, slug_t p1y,
		slug_t p2x, slug_t p2y,
		slug_t p3x, slug_t p3y
	) {
		const slug_t midx = (p0x + 3_cv * p1x + 3_cv * p2x + p3x) * 0.125_cv;
		const slug_t midy = (p0y + 3_cv * p1y + 3_cv * p2y + p3y) * 0.125_cv;

		curves.push_back({
			p0x, p0y,
			(p0x + 3_cv * p1x) * 0.25_cv,
			(p0y + 3_cv * p1y) * 0.25_cv,
			midx, midy
		});

		curves.push_back({
			midx, midy,
			(3_cv * p2x + p3x) * 0.25_cv,
			(3_cv * p2y + p3y) * 0.25_cv,
			p3x, p3y
		});
	}

	// Recursive De Casteljau subdivision. Splits at the midpoint and recurses into each half until
	// flat or MAX_DEPTH is reached.
	void _cubicAdaptive(
		slug_t p0x, slug_t p0y,
		slug_t p1x, slug_t p1y,
		slug_t p2x, slug_t p2y,
		slug_t p3x, slug_t p3y,
		size_t depth
	) {
		if(depth >= MAX_DEPTH || _flatEnough(p0x, p0y, p1x, p1y, p2x, p2y, p3x, p3y)) {
			_emitTwoQuads(p0x, p0y, p1x, p1y, p2x, p2y, p3x, p3y);

			return;
		}

		// De Casteljau split at t=0.5
		const slug_t m01x = (p0x + p1x) * 0.5_cv;
		const slug_t m01y = (p0y + p1y) * 0.5_cv;

		const slug_t m12x = (p1x + p2x) * 0.5_cv;
		const slug_t m12y = (p1y + p2y) * 0.5_cv;

		const slug_t m23x = (p2x + p3x) * 0.5_cv;
		const slug_t m23y = (p2y + p3y) * 0.5_cv;

		const slug_t m012x = (m01x + m12x) * 0.5_cv;
		const slug_t m012y = (m01y + m12y) * 0.5_cv;

		const slug_t m123x = (m12x + m23x) * 0.5_cv;
		const slug_t m123y = (m12y + m23y) * 0.5_cv;

		const slug_t m0123x = (m012x + m123x) * 0.5_cv;
		const slug_t m0123y = (m012y + m123y) * 0.5_cv;

		_cubicAdaptive(p0x, p0y, m01x, m01y, m012x, m012y, m0123x, m0123y, depth + 1);
		_cubicAdaptive(m0123x, m0123y, m123x, m123y, m23x, m23y, p3x, p3y, depth + 1);
	}
};

// ================================================================================================
// Monotonic Decomposition (Scanline Sweeper preprocessing)
//
// toMonotonicCurves() decomposes one quadratic Bezier into at most 4 sub-curves that are each
// monotonic in both x and y. Required preprocessing for the Scanline Sweeper rendering algorithm.
// Each call appends sub-curves to `out`; initialize `out` before the first call.
//
// Horizontal linear segments (zero y-range in all three control points) are silently dropped:
// they contribute zero swept area and would cause a divide-by-zero in the vertical fast-path.
// ================================================================================================

namespace detail {

// De Casteljau split of a quadratic Bezier at parameter t in (0,1).
inline std::pair<Atlas::Curve, Atlas::Curve> quadSplitAt(const Atlas::Curve& c, slug_t t) {
	const slug_t m01x = c.x1 + t * (c.x2 - c.x1);
	const slug_t m01y = c.y1 + t * (c.y2 - c.y1);
	const slug_t m12x = c.x2 + t * (c.x3 - c.x2);
	const slug_t m12y = c.y2 + t * (c.y3 - c.y2);
	const slug_t midx = m01x + t * (m12x - m01x);
	const slug_t midy = m01y + t * (m12y - m01y);

	return {
		{c.x1, c.y1, m01x, m01y, midx, midy},
		{midx, midy, m12x, m12y, c.x3, c.y3}
	};
}

// Returns t* in (0,1) where a quadratic has an extremum in the x (useY=false) or y (useY=true)
// dimension, or -1 if the curve is already monotonic in that dimension.
inline slug_t quadExtrema(const Atlas::Curve& c, bool useY) {
	const slug_t c0 = useY ? c.y1 : c.x1;
	const slug_t c1 = useY ? c.y2 : c.x2;
	const slug_t c2 = useY ? c.y3 : c.x3;
	const slug_t denom = c0 - 2_cv * c1 + c2;

	if(std::abs(denom) < 1e-6_cv) return -1.0_cv;

	const slug_t t = (c0 - c1) / denom;

	return (t > 0_cv && t < 1_cv) ? t : -1.0_cv;
}

}

// Decomposes one quadratic Bezier into monotonic-in-both-axes sub-curves (at most 4).
// Appends results to `out`. Horizontal linear segments are dropped.
inline void toMonotonicCurves(const Atlas::Curve& c, Atlas::Curves& out) {
	constexpr slug_t eps = 1e-6_cv;

	// Horizontal linear segments: all y-coordinates equal; zero swept area.
	if(std::abs(c.y3 - c.y1) < eps && std::abs(c.y2 - c.y1) < eps) return;

	// Split at y-extremum first to get y-monotonic halves, then check x per half.
	const slug_t ty = detail::quadExtrema(c, true);

	if(ty < 0.0_cv) {
		// Already y-monotonic; check x only.
		const slug_t tx = detail::quadExtrema(c, false);

		if(tx < 0.0_cv) out.push_back(c);

		else {
			auto [l, r] = detail::quadSplitAt(c, tx);

			out.push_back(l);
			out.push_back(r);
		}
	}

	else {
		auto [ly, ry] = detail::quadSplitAt(c, ty);

		for(const Atlas::Curve* half : {&ly, &ry}) {
			const slug_t tx = detail::quadExtrema(*half, false);

			if(tx < 0.0_cv) out.push_back(*half);

			else {
				auto [lx, rx] = detail::quadSplitAt(*half, tx);

				out.push_back(lx);
				out.push_back(rx);
			}
		}
	}
}

// ================================================================================================
// Debugging Helpers
// ================================================================================================

inline std::ostream& operator<<(std::ostream& os, const Key& k) {
	if(k.type() == Key::Type::Codepoint) return os << "Key(0x"
		<< std::hex << k.codepoint() << std::dec << ")"
	;

	return os << "Key(\"" << k.name() << "\")";
}

inline std::ostream& operator<<(std::ostream& os, const Color& c) {
	return os << "Color(r=" << c.r << " g=" << c.g << " b=" << c.b << " a=" << c.a << ")";
}

inline std::ostream& operator<<(std::ostream& os, const Matrix& m) {
	return os
		<< "Matrix(xx=" << m.xx << " yx=" << m.yx
		<< " xy=" << m.xy << " yy=" << m.yy
		<< " dx=" << m.dx << " dy=" << m.dy << ")"
	;
}

inline std::ostream& operator<<(std::ostream& os, const Quad& q) {
	return os
		<< "Quad("
		<< "(" << q.x0 << "," << q.y0 << ")"
		<< " -> (" << q.x1 << "," << q.y1 << ")"
		<< ")"
	;
}

inline std::ostream& operator<<(std::ostream& os, const GradientStop& s) {
	return os << "GradientStop(t=" << s.t << " color=" << s.color << ")";
}

inline std::ostream& operator<<(std::ostream& os, GradientInfo::Type type) {
	switch(type) {
		case GradientInfo::Type::Linear: return os << "Linear";
		case GradientInfo::Type::Radial: return os << "Radial";
		case GradientInfo::Type::Sweep: return os << "Sweep";
		case GradientInfo::Type::AffineRadial: return os << "AffineRadial";
	}

	return os << "GradientInfo::Type(?)";
}

inline std::ostream& operator<<(std::ostream& os, const GradientInfo& g) {
	return os
		<< "GradientInfo(type=" << g.type
		<< " stops=" << g.stops.size()
		<< " transform=" << g.transform
		<< " innerRadius=" << g.innerRadius
		<< " startAngle=" << g.startAngle
		<< " endAngle=" << g.endAngle << ")"
	;
}

inline std::ostream& operator<<(std::ostream& os, const KeyIterator& k) {
	return os << "KeyIterator(prefix=\"" << k.prefix << "\" counter=" << k.counter << ")";
}

inline std::ostream& operator<<(std::ostream& os, const Mask& mk) {
	os << "Mask(type=" << static_cast<int>(mk.type);
	if(mk.key) os << " key=" << *mk.key;
	os << " params=[";
	for(size_t i = 0; i < 6; ++i) { if(i) os << ","; os << mk.params[i]; }
	os << "]";
	if(mk.invert) os << " invert";
	return os << ")";
}

inline std::ostream& operator<<(std::ostream& os, const Layer& l) {
	return os
		<< "Layer(" << l.key
		<< " color=" << l.color
		<< " transform=" << l.transform
		<< " scale=" << l.scale
		<< " effectId=" << l.effectId
		<< " gradientId=" << l.gradientId
		<< " bleed=" << l.bleed
		<< " drawMode=" << static_cast<int>(l.drawMode)
		<< " blendMode=" << static_cast<int>(l.blendMode) << ")"
	;
}

inline std::ostream& operator<<(std::ostream& os, const Atlas::Curve& c) {
	return os
		<< "Curve("
		<< "(" << c.x1 << "," << c.y1 << ")"
		<< " -> (" << c.x2 << "," << c.y2 << ")"
		<< " -> (" << c.x3 << "," << c.y3 << ")"
		<< ")"
	;
}

inline std::ostream& operator<<(std::ostream& os, Atlas::ShapeInfo::Origin::Type type) {
	switch(type) {
		case Atlas::ShapeInfo::Origin::Type::Default: return os << "Default";
		case Atlas::ShapeInfo::Origin::Type::Centered: return os << "Centered";
		case Atlas::ShapeInfo::Origin::Type::Pivot: return os << "Pivot";
		case Atlas::ShapeInfo::Origin::Type::Custom: return os << "Custom";
	}

	return os << "Origin::Type(?)";
}

inline std::ostream& operator<<(std::ostream& os, const Atlas::ShapeInfo::Origin& origin) {
	return os << "Origin(type=" << origin.type << " x=" << origin.x << " y=" << origin.y << ")";
}

inline std::ostream& operator<<(std::ostream& os, const Atlas::Shape& s) {
	return os
		<< "Shape(w=" << s.width << " h=" << s.height
		<< " bearing=" << s.bearingX << "/" << s.bearingY
		<< " origin=" << s.originX << "/" << s.originY
		<< " bandScale=" << s.bandScaleX << "/" << s.bandScaleY
		<< " bandOffset=" << s.bandOffsetX << "/" << s.bandOffsetY << ")"
	;
}

inline std::ostream& operator<<(std::ostream& os, const Atlas::ShapeInfo& info) {
	return os
		<< "ShapeInfo(curves=" << info.curves.size()
		<< " autoMetrics=" << info.autoMetrics
		<< " bearing=" << info.bearingX << "/" << info.bearingY
		<< " size=" << info.width << "x" << info.height
		<< " advance=" << info.advance
		<< " bands=" << info.numBandsX << "x" << info.numBandsY
		<< " splits=" << info.splitsX.size() << "/" << info.splitsY.size()
		<< " origin=" << info.origin << ")"
	;
}

inline std::ostream& operator<<(std::ostream& os, const Atlas::PackingStats& p) {
	return os
		<< "PackingStats("
		<< "curve: " << p.curveTexelsUsed << " used"
		<< " + " << p.curveTexelsPadding << " padding"
		<< " / " << p.curveTexelsTotal << " total"
		<< " (" << int(p.curveUtilization() * 100.f) << "% util,"
		<< " " << int(p.curvePaddingRatio() * 100.f) << "% pad)"
		<< " | band: " << p.bandTexelsUsed << " used"
		<< " + " << p.bandTexelsPadding << " padding"
		<< " / " << p.bandTexelsTotal << " total"
		<< " (" << int(p.bandUtilization() * 100.f) << "% util,"
		<< " " << int(p.bandPaddingRatio() * 100.f) << "% pad,"
		<< " max band=" << p.bandMaxCount << "/65535,"
		<< " max offset=" << p.bandMaxOffset << "/65535)"
		<< " | gradient: " << p.gradientCount
		<< " gradient" << (p.gradientCount != 1 ? "s" : "")
		<< " (" << Atlas::GRADIENT_STRIP_WIDTH << "x" << p.gradientCount
		<< " RGBA8, " << p.gradientTexelsTotal << " texels)"
		<< " | sdf: " << p.sdfTileCount << " tile" << (p.sdfTileCount != 1 ? "s" : "")
		<< " (" << p.sdfTexelsUsed << " used"
		<< " + " << p.sdfTexelsPadding << " padding"
		<< " / " << p.sdfTexelsTotal << " total"
		<< " RGBA8, " << int(p.sdfUtilization() * 100.f) << "% util)"
		<< " | msdf: " << p.msdfLayerCount << " layer" << (p.msdfLayerCount != 1 ? "s" : "")
		<< " (" << p.msdfTileSize << "x" << p.msdfTileSize << " RGB32F, "
		<< p.msdfTexelsTotal << " texels)"
		<< " | scanline: " << p.scanlineTexelsTotal << " texels RGBA32F"
		<< " | total: " << (static_cast<double>(p.totalBytes()) / 1024.0 / 1024.0) << " MiB"
		<< ")"
	;
}

inline std::ostream& operator<<(std::ostream& os, Atlas::TextureData::Format format) {
	switch(format) {
		case Atlas::TextureData::Format::RGBA32F: return os << "RGBA32F";
		case Atlas::TextureData::Format::RGBA16UI: return os << "RGBA16UI";
		case Atlas::TextureData::Format::RGBA8: return os << "RGBA8";
		case Atlas::TextureData::Format::RGB32F: return os << "RGB32F";
	}

	return os << "TextureData::Format(?)";
}

inline std::ostream& operator<<(std::ostream& os, const Atlas::TextureData& t) {
	return os
		<< "TextureData(width=" << t.width
		<< " height=" << t.height
		<< " format=" << t.format
		<< " bytes=" << t.bytes.size() << ")"
	;
}

inline std::ostream& operator<<(std::ostream& os, const CompositeShape& c) {
	return os << "CompositeShape(layers=" << c.layers.size() << " advance=" << c.advance << ")";
}

inline std::optional<Quad> CompositeShape::boundingBox(const Atlas& atlas) const {
	slug_t minX = std::numeric_limits<slug_t>::max();
	slug_t minY = std::numeric_limits<slug_t>::max();
	slug_t maxX = std::numeric_limits<slug_t>::lowest();
	slug_t maxY = std::numeric_limits<slug_t>::lowest();

	bool any = false;

	for(const auto& l : layers) {
		const auto shape = atlas.getShape(l.key);

		if(!shape) continue;

		const auto q = shape->computeQuad(l.transform, l.scale);

		// bleed is rendered content (glow/shadow spread), so it belongs in the bounds.
		const slug_t b = l.bleed * l.scale;

		minX = std::min(minX, q.x0 - b);
		minY = std::min(minY, q.y0 - b);
		maxX = std::max(maxX, q.x1 + b);
		maxY = std::max(maxY, q.y1 + b);

		any = true;
	}

	return any ? std::optional<Quad>({minX, minY, maxX, maxY}) : std::nullopt;
}

inline std::ostream& operator<<(std::ostream& os, const FontMetrics& m) {
	return os
		<< "FontMetrics(unitsPerEM=" << m.unitsPerEM
		<< " capHeight=" << m.capHeightRatio
		<< " xHeight=" << m.xHeightRatio
		<< " ascender=" << m.ascenderRatio
		<< " descender=" << m.descenderRatio
		<< " lineGap=" << m.lineGapRatio << ")"
	;
}

}

#if defined(__clang__)
	#define SLUGHORN_PRAGMA(x) _Pragma(#x)
	#define SLUGHORN_DIAGNOSTIC_PUSH() SLUGHORN_PRAGMA(clang diagnostic push)
	#define SLUGHORN_DIAGNOSTIC_POP() SLUGHORN_PRAGMA(clang diagnostic pop)
	#define SLUGHORN_IGNORE(w) SLUGHORN_PRAGMA(clang diagnostic ignored w)

#elif defined(__GNUC__)
	#define SLUGHORN_PRAGMA(x) _Pragma(#x)
	#define SLUGHORN_DIAGNOSTIC_PUSH() SLUGHORN_PRAGMA(GCC diagnostic push)
	#define SLUGHORN_DIAGNOSTIC_POP() SLUGHORN_PRAGMA(GCC diagnostic pop)
	#define SLUGHORN_IGNORE(w) SLUGHORN_PRAGMA(GCC diagnostic ignored w)

#else
	#define SLUGHORN_DIAGNOSTIC_PUSH()
	#define SLUGHORN_DIAGNOSTIC_POP()
	#define SLUGHORN_IGNORE(w)
#endif
