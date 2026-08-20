#pragma once

// ================================================================================================
// render.hpp - CPU-side Slug coverage emulator
//
// Mirrors the GPU fragment shader analytically, enabling:
//
// - Software rendering / visual validation without a GPU
// - SDF/MSDF tile generation (see buildSdfAtlas plan)
// - Post-build curve access for strokeText / glyphOutline
//
// Usage:
//
// slughorn::render::Sampler s = slughorn::render::decode(atlas, key);
// slughorn::render::Grid g = s.renderGrid(128);
// // g.data is row-major float32: g.data[row * g.width + col]
//
// decode() reads the packed curve/band textures produced by Atlas::build() and
// Atlas::packTextures(). It can be called any time after packTextures() returns.
//
// No GPU, no Python, no external dependencies beyond slughorn.hpp.
// ================================================================================================

#include "slughorn.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <stdexcept>
#include <unordered_map>
#include <vector>


#ifdef SLUGHORN_HAS_MSDF
#include <msdfgen.h>
#endif

namespace slughorn {
namespace render {

// ================================================================================================
// Sample - coverage result for a single em-space point
// ================================================================================================

struct Sample {
	slug_t fill = 0_cv;
	slug_t xcov = 0_cv;
	slug_t ycov = 0_cv;
	slug_t xwgt = 0_cv;
	slug_t ywgt = 0_cv;

	uint32_t iters = 0;
};

// ================================================================================================
// Grid - 2-D coverage result from renderGrid()
//
// data is row-major: data[row * width + col], values in [0, 1].
// ================================================================================================

struct Grid {
	uint32_t width = 0;
	uint32_t height = 0;

	std::vector<slug_t> data = {};

	slug_t at(uint32_t row, uint32_t col) const { return data[row * width + col]; }
};

// ================================================================================================
// Sampler - holds a decoded shape and provides coverage evaluation
// ================================================================================================

struct Sampler {
	Atlas::Shape shape;
	Atlas::Curves curves;

	std::vector<uint32_t> hbandOffsets;
	std::vector<uint32_t> hbandIndices;
	std::vector<uint32_t> vbandOffsets;
	std::vector<uint32_t> vbandIndices;
	std::array<uint8_t, Atlas::INDIRECTION_SIZE> indirY{};
	std::array<uint8_t, Atlas::INDIRECTION_SIZE> indirX{};

	// -------------------------------------------------------------------------
	// Em-space helpers
	// -------------------------------------------------------------------------

	std::pair<slug_t, slug_t> emOrigin() const {
		const slug_t ox = shape.bandScaleX != 0_cv ? -shape.bandOffsetX / shape.bandScaleX : 0_cv;
		const slug_t oy = shape.bandScaleY != 0_cv ? -shape.bandOffsetY / shape.bandScaleY : 0_cv;

		return {ox, oy};
	}

	std::pair<slug_t, slug_t> emSize() const {
		const slug_t sx = shape.bandScaleX != 0_cv
			? cv(Atlas::INDIRECTION_SIZE) / shape.bandScaleX
			: 0_cv
		;

		const slug_t sy = shape.bandScaleY != 0_cv
			? cv(Atlas::INDIRECTION_SIZE) / shape.bandScaleY
			: 0_cv
		;

		return {sx, sy};
	}

	std::pair<uint32_t, uint32_t> computeRenderSize(uint32_t sizeHint) const {
		const slug_t w = shape.width;
		const slug_t h = shape.height;

		if(w <= 0_cv || h <= 0_cv)
			throw std::runtime_error("Invalid shape dimensions for renderGrid()")
		;

		const slug_t scale = cv(sizeHint) / std::max(w, h);
		const auto outW = static_cast<uint32_t>(std::max(1_cv, cv(std::round(w * scale))));
		const auto outH = static_cast<uint32_t>(std::max(1_cv, cv(std::round(h * scale))));

		return {outW, outH};
	}

	// -------------------------------------------------------------------------
	// Coverage evaluation
	// -------------------------------------------------------------------------

	Sample renderSample(slug_t rx, slug_t ry, slug_t ppeX, slug_t ppeY) const {
		Sample out;

		for(const auto& c : curves) {
			out.iters++;

			const slug_t x1 = c.x1 - rx, y1 = c.y1 - ry;
			const slug_t x2 = c.x2 - rx, y2 = c.y2 - ry;
			const slug_t x3 = c.x3 - rx, y3 = c.y3 - ry;

			uint32_t code = _calcRootCode(y1, y2, y3);

			if(code) {
				auto [r1, r2] = _solveHorizPoly(x1, y1, x2, y2, x3, y3);

				r1 *= ppeX; r2 *= ppeX;

				if(code & 0x01u) {
					out.xcov += _clamp(r1 + 0.5_cv, 0_cv, 1_cv);
					out.xwgt = std::max(out.xwgt, _clamp(1_cv - std::abs(r1) * 2_cv, 0_cv, 1_cv));
				}

				if(code & 0x100u) {
					out.xcov -= _clamp(r2 + 0.5_cv, 0_cv, 1_cv);
					out.xwgt = std::max(out.xwgt, _clamp(1_cv - std::abs(r2) * 2_cv, 0_cv, 1_cv));
				}
			}

			code = _calcRootCode(x1, x2, x3);

			if(code) {
				auto [r1, r2] = _solveVertPoly(x1, y1, x2, y2, x3, y3);

				r1 *= ppeY; r2 *= ppeY;

				if(code & 0x01u) {
					out.ycov -= _clamp(r1 + 0.5_cv, 0_cv, 1_cv);
					out.ywgt = std::max(out.ywgt, _clamp(1_cv - std::abs(r1) * 2_cv, 0_cv, 1_cv));
				}

				if(code & 0x100u) {
					out.ycov += _clamp(r2 + 0.5_cv, 0_cv, 1_cv);
					out.ywgt = std::max(out.ywgt, _clamp(1_cv - std::abs(r2) * 2_cv, 0_cv, 1_cv));
				}
			}
		}

		out.fill = _calcCoverage(out.xcov, out.ycov, out.xwgt, out.ywgt);

		return out;
	}

	Sample renderSampleBanded(slug_t rx, slug_t ry, slug_t ppeX, slug_t ppeY) const {
		Sample out;

		if(hbandOffsets.size() < 2 || vbandOffsets.size() < 2) return out;

		const uint32_t bandX = _lookupBandIndir(rx * shape.bandScaleX + shape.bandOffsetX, indirX);
		const uint32_t bandY = _lookupBandIndir(ry * shape.bandScaleY + shape.bandOffsetY, indirY);

		auto process = [&](uint32_t ci, bool horizontal) -> bool {
			out.iters++;

			const auto& c = curves[ci];

			const slug_t x1 = c.x1 - rx, y1 = c.y1 - ry;
			const slug_t x2 = c.x2 - rx, y2 = c.y2 - ry;
			const slug_t x3 = c.x3 - rx, y3 = c.y3 - ry;

			if(horizontal) {
				if(std::max({x1, x2, x3}) * ppeX < -0.5_cv) return false;

				const uint32_t code = _calcRootCode(y1, y2, y3);

				if(!code) return true;

				auto [r1, r2] = _solveHorizPoly(x1, y1, x2, y2, x3, y3);

				r1 *= ppeX; r2 *= ppeX;

				if(code & 0x01u) {
					out.xcov += _clamp(r1 + 0.5_cv, 0_cv, 1_cv);
					out.xwgt = std::max(out.xwgt, _clamp(1_cv - std::abs(r1) * 2_cv, 0_cv, 1_cv));
				}

				if(code & 0x100u) {
					out.xcov -= _clamp(r2 + 0.5_cv, 0_cv, 1_cv);
					out.xwgt = std::max(out.xwgt, _clamp(1_cv - std::abs(r2) * 2_cv, 0_cv, 1_cv));
				}
			}

			else {
				if(std::max({y1, y2, y3}) * ppeY < -0.5_cv) return false;

				const uint32_t code = _calcRootCode(x1, x2, x3);

				if(!code) return true;

				auto [r1, r2] = _solveVertPoly(x1, y1, x2, y2, x3, y3);

				r1 *= ppeY; r2 *= ppeY;

				if(code & 0x01u) {
					out.ycov -= _clamp(r1 + 0.5_cv, 0_cv, 1_cv);
					out.ywgt = std::max(out.ywgt, _clamp(1_cv - std::abs(r1) * 2_cv, 0_cv, 1_cv));
				}

				if(code & 0x100u) {
					out.ycov += _clamp(r2 + 0.5_cv, 0_cv, 1_cv);
					out.ywgt = std::max(out.ywgt, _clamp(1_cv - std::abs(r2) * 2_cv, 0_cv, 1_cv));
				}
			}

			return true;
		};

		if(bandY + 1 < hbandOffsets.size()) {
			for(uint32_t i = hbandOffsets[bandY]; i < hbandOffsets[bandY + 1]; i++) {
				if(!process(hbandIndices[i], true)) break;
			}
		}

		if(bandX + 1 < vbandOffsets.size()) {
			for(uint32_t i = vbandOffsets[bandX]; i < vbandOffsets[bandX + 1]; i++) {
				if(!process(vbandIndices[i], false)) break;
			}
		}

		out.fill = _calcCoverage(out.xcov, out.ycov, out.xwgt, out.ywgt);

		return out;
	}

	Grid renderGrid(
		uint32_t sizeHint=128,
		slug_t margin=0_cv,
		bool banded=true,
		bool parallel=false
	) const {
		const auto [width, height] = computeRenderSize(sizeHint);

		auto [ox, oy] = emOrigin();
		auto [sx, sy] = emSize();

		ox -= margin * sx;
		oy -= margin * sy;
		sx *= (1_cv + 2_cv * margin);
		sy *= (1_cv + 2_cv * margin);

		Grid grid{width, height, std::vector<slug_t>(width * height, 0_cv)};

		const slug_t ppeX = cv(width);
		const slug_t ppeY = cv(height);

		auto renderRow = [&](uint32_t j) {
			for(uint32_t i = 0; i < width; i++) {
				const slug_t u = (cv(i) + 0.5_cv) / cv(width);
				const slug_t v = (cv(j) + 0.5_cv) / cv(height);
				const slug_t ex = ox + u * sx;
				const slug_t ey = oy + v * sy;

				const auto result = banded
					? renderSampleBanded(ex, ey, ppeX, ppeY)
					: renderSample(ex, ey, ppeX, ppeY)
				;

				grid.data[j * width + i] = result.fill;
			}
		};

#ifdef SLUGHORN_HAS_PARALLEL
		if(parallel) {
			#pragma omp parallel for schedule(static)
			for(uint32_t j = 0; j < height; j++) renderRow(j);
			return grid;
		}
#else
		static_cast<void>(parallel);
#endif

		for(uint32_t j = 0; j < height; j++) renderRow(j);

		return grid;
	}

private:
	static constexpr slug_t EPS = 1_cv / 65536_cv;

	static uint32_t _floatBitsToUint32(slug_t x) { return std::bit_cast<uint32_t>(x); }

	static slug_t _clamp(slug_t x, slug_t lo, slug_t hi) {
		return x < lo ? lo : (x > hi ? hi : x);
	}

	static uint32_t _calcRootCode(slug_t y1, slug_t y2, slug_t y3) {
		const uint32_t i1 = _floatBitsToUint32(y1) >> 31;
		const uint32_t i2 = _floatBitsToUint32(y2) >> 30;
		const uint32_t i3 = _floatBitsToUint32(y3) >> 29;

		uint32_t shift = (i2 & 0x2u) | (i1 & ~0x2u);

		shift = (i3 & 0x4u) | (shift & ~0x4u);

		return (0x2E74u >> shift) & 0x0101u;
	}

	static std::pair<slug_t, slug_t> _solveHorizPoly(
		slug_t x1, slug_t y1,
		slug_t x2, slug_t y2,
		slug_t x3, slug_t y3
	) {
		const slug_t ax = x1 - 2_cv * x2 + x3, ay = y1 - 2_cv * y2 + y3;
		const slug_t bx = x1 - x2, by = y1 - y2;

		if(std::abs(ay) < EPS) {
			const slug_t t = std::abs(by) >= EPS ? y1 * (0.5_cv / by) : 0_cv;
			const slug_t x = (ax * t - 2_cv * bx) * t + x1;

			return {x, x};
		}

		const slug_t d = std::sqrt(std::max(by * by - ay * y1, 0_cv));
		const slug_t t1 = (by - d) / ay, t2 = (by + d) / ay;

		return {(ax * t1 - 2_cv * bx) * t1 + x1, (ax * t2 - 2_cv * bx) * t2 + x1};
	}

	static std::pair<slug_t, slug_t> _solveVertPoly(
		slug_t x1, slug_t y1,
		slug_t x2, slug_t y2,
		slug_t x3, slug_t y3
	) {
		const slug_t ax = x1 - 2_cv * x2 + x3, ay = y1 - 2_cv * y2 + y3;
		const slug_t bx = x1 - x2, by = y1 - y2;

		if(std::abs(ax) < EPS) {
			const slug_t t = std::abs(bx) >= EPS ? x1 * (0.5_cv / bx) : 0_cv;
			const slug_t y = (ay * t - 2_cv * by) * t + y1;

			return {y, y};
		}

		const slug_t d = std::sqrt(std::max(bx * bx - ax * x1, 0_cv));
		const slug_t t1 = (bx - d) / ax, t2 = (bx + d) / ax;

		return {(ay * t1 - 2_cv * by) * t1 + y1, (ay * t2 - 2_cv * by) * t2 + y1};
	}

	static slug_t _calcCoverage(slug_t xcov, slug_t ycov, slug_t xwgt, slug_t ywgt) {
		const slug_t weighted = std::abs(xcov * xwgt + ycov * ywgt) / std::max(xwgt + ywgt, EPS);
		const slug_t conservative = std::min(std::abs(xcov), std::abs(ycov));

		return _clamp(std::max(weighted, conservative), 0_cv, 1_cv);
	}

	static uint32_t _lookupBandIndir(
		slug_t coordScaled,
		const std::array<uint8_t, Atlas::INDIRECTION_SIZE>& indir
	) {
		const auto q = static_cast<uint32_t>(_clamp(coordScaled, 0_cv, cv(Atlas::INDIRECTION_SIZE - 1)));

		return indir[q];
	}
};

// ================================================================================================
// decode() - reconstruct a Sampler from packed atlas textures
//
// Reads the curve and band textures produced by Atlas::build() + packTextures().
// Throws std::out_of_range if the key is not found, std::runtime_error on texture
// format or bounds violations.
// ================================================================================================

inline Sampler decode(
	const Atlas::Shape& shape,
	const Atlas::TextureData& curveTex,
	const Atlas::TextureData& bandTex
);

inline Sampler decode(const Atlas& atlas, Key key) {
	const auto info = atlas.getShape(key);

	if(!info) throw std::out_of_range("Key not found in atlas (or atlas not built yet)");

	return decode(*info, atlas.getCurveTextureData(), atlas.getBandTextureData());
}

inline Sampler decode(
	const Atlas::Shape& shape,
	const Atlas::TextureData& curveTex,
	const Atlas::TextureData& bandTex
) {

	if(curveTex.format != Atlas::TextureData::Format::RGBA32F)
		throw std::runtime_error("Unexpected curve texture format")
	;

	if(bandTex.format != Atlas::TextureData::Format::RGBA16UI)
		throw std::runtime_error("Unexpected band texture format")
	;

	Sampler out;

	out.shape = shape;

	if(shape.bandScaleX == 0_cv || shape.bandScaleY == 0_cv) {
		out.hbandOffsets = {0, 0};
		out.vbandOffsets = {0, 0};

		return out;
	}

	const auto* curveData = reinterpret_cast<const float*>(curveTex.bytes.data());
	const auto* bandData = reinterpret_cast<const uint16_t*>(bandTex.bytes.data());

	const uint32_t shapeStart = shape.bandTexY * bandTex.width + shape.bandTexX;
	const uint32_t numHBands = shape.bandMaxY + 1;
	const uint32_t numVBands = shape.bandMaxX + 1;
	const uint32_t numBandHdrs = numHBands + numVBands;
	const uint32_t indirSize = numBandHdrs > 0 ? 2 * Atlas::INDIRECTION_SIZE : 0;

	auto readBandTexel = [&](uint32_t texelIndex) -> const uint16_t* {
		if(texelIndex >= bandTex.width * bandTex.height)
			throw std::runtime_error("Band texture read out of bounds")
		;

		return bandData + size_t{texelIndex} * 4;
	};

	for(uint32_t q = 0; q < Atlas::INDIRECTION_SIZE; q++) {
		out.indirY[q] = static_cast<uint8_t>(readBandTexel(shapeStart + q)[0]);
		out.indirX[q] = static_cast<uint8_t>(readBandTexel(shapeStart + Atlas::INDIRECTION_SIZE + q)[0]);
	}

	struct Header { uint32_t count = 0, offset = 0; };

	std::vector<Header> headers(numBandHdrs);

	for(uint32_t i = 0; i < numBandHdrs; i++) {
		const auto* texel = readBandTexel(shapeStart + indirSize + i);

		headers[i].count = texel[0];
		headers[i].offset = texel[1];
	}

	std::vector<uint32_t> globalIndices;

	globalIndices.reserve(64);

	auto decodeBandList = [&](
		uint32_t headerIndex,
		uint32_t numBands,
		std::vector<uint32_t>& offsets,
		std::vector<uint32_t>& indices
	) {
		offsets.clear();
		indices.clear();
		offsets.push_back(0);

		for(uint32_t i = 0; i < numBands; i++) {
			const auto& h = headers[headerIndex + i];

			for(uint32_t j = 0; j < h.count; j++) {
				const auto* texel = readBandTexel(shapeStart + h.offset + j);
				const uint32_t cx = texel[0];
				const uint32_t cy = texel[1];
				const uint32_t idx = (cy * curveTex.width + cx) / 2;

				indices.push_back(idx);
				globalIndices.push_back(idx);
			}

			offsets.push_back(static_cast<uint32_t>(indices.size()));
		}
	};

	decodeBandList(0, numHBands, out.hbandOffsets, out.hbandIndices);
	decodeBandList(numHBands, numVBands, out.vbandOffsets, out.vbandIndices);

	std::sort(globalIndices.begin(), globalIndices.end());

	globalIndices.erase(
		std::unique(globalIndices.begin(), globalIndices.end()),
		globalIndices.end()
	);

	std::unordered_map<uint32_t, uint32_t> remap;

	remap.reserve(globalIndices.size());

	out.curves.reserve(globalIndices.size());

	for(uint32_t globalIndex : globalIndices) {
		const uint32_t texel0 = globalIndex * 2;
		const uint32_t texel1 = texel0 + 1;

		if(texel1 >= curveTex.width * curveTex.height)
			throw std::runtime_error("Curve texture read out of bounds")
		;

		const float* t0 = curveData + size_t{texel0} * 4;
		const float* t1 = curveData + size_t{texel1} * 4;

		remap[globalIndex] = static_cast<uint32_t>(out.curves.size());

		out.curves.push_back({t0[0], t0[1], t0[2], t0[3], t1[0], t1[1]});
	}

	for(auto& idx : out.hbandIndices) idx = remap.at(idx);
	for(auto& idx : out.vbandIndices) idx = remap.at(idx);

	return out;
}

// ================================================================================================
// MSDF support - only available when built with -DSLUGHORN_MSDF=ON
// ================================================================================================

#ifdef SLUGHORN_HAS_MSDF

// Multi-channel SDF grid: 3 floats per pixel (R, G, B channels).
// Reconstruct in shader: float sd = median(d.r, d.g, d.b);
struct MSDFGrid {
	uint32_t width = 0;
	uint32_t height = 0;

	std::vector<float> data = {}; // row-major, 3 floats per pixel

	float at(uint32_t row, uint32_t col, uint32_t ch) const {
		return data[(row * width + col) * 3 + ch];
	}
};

// Build an msdfgen::Shape from the atlas contours for key.
// Coordinates are in em-space (same space as Atlas::Curves).
// Returns an empty Shape if the key has no geometry.
//
// Slughorn's curve winding appears CW to msdfgen (Y-up, CCW=filled convention).
// Each contour is reversed so that filled regions become CCW, then orientContours()
// assigns the correct fill/hole role for compound shapes.
inline msdfgen::Shape toMSDFShape(const Atlas& atlas, Key key) {
	msdfgen::Shape shape;

	for(const auto& contour : atlas.getShapeContours(key)) {
		auto& c = shape.addContour();

		// Each Atlas::Curve is a quadratic Bezier: p1=start, p2=off-curve, p3=end.
		for(const auto& curve : contour) c.addEdge(msdfgen::EdgeHolder(
			{curve.x1, curve.y1},
			{curve.x2, curve.y2},
			{curve.x3, curve.y3}
		));

		c.reverse();
	}

	if(!shape.contours.empty()) shape.orientContours();

	return shape;
}

// Build the SDFTransformation for a shape + tile dimensions.
// range is in em-space units: distance from edge that maps to 0.0 or 1.0 in the output.
//
// msdfgen Projection formula: pixel = scale * (shape + translate)
// - unproject: shape = pixel/scale - translate
// To map b.l (shape) -> 0 (pixel): 0 = scale*(b.l + translate) -> translate = -b.l
// Translate is em-space, NOT pixel-space.
inline msdfgen::SDFTransformation msdfTransform(
	const msdfgen::Shape::Bounds& b,
	uint32_t tileW,
	uint32_t tileH,
	slug_t range
) {
	const double bw = b.r - b.l;
	const double bh = b.t - b.b;

	// Fill the tile in both dimensions independently so UV [0,1] maps exactly to
	// [b.l,b.r] x [b.b,b.t]. renderSDF/renderMSDF already aspect-ratio-match their
	// tiles, so scaleX ~= scaleY there. renderMSDFTile (always square) needs this to
	// avoid letterboxing narrow/tall glyphs, which would make tileUV in the shader
	// overshoot into empty exterior space.
	const msdfgen::Projection proj({tileW / bw, tileH / bh}, {-b.l, -b.b});

	// Distance mapping: [-range, range] em-units -> [0, 1]; edge pixel -> 0.5
	const msdfgen::DistanceMapping dmap(msdfgen::Range(2.0 * range));

	return {proj, dmap};
}

// Generate a single-channel SDF tile for the given key.
// tileSize: longest axis in texels. range: em-space spread (e.g. 0.1 = 10% of shape).
// Returns a Grid with values in [0, 1]; edge pixels are ~0.5.
inline Grid renderSDF(
	const Atlas& atlas,
	Key key,
	uint32_t tileSize=128,
	slug_t range=0.1_cv
) {
	msdfgen::Shape msdfShape = toMSDFShape(atlas, key);

	if(msdfShape.contours.empty()) {
		return Grid{tileSize, tileSize, std::vector<slug_t>(tileSize * tileSize, 0_cv)};
	}

	const auto bounds = msdfShape.getBounds(range);
	const double bw = bounds.r - bounds.l, bh = bounds.t - bounds.b;
	const double scale = tileSize / std::max(bw, bh);
	const auto tileW = static_cast<uint32_t>(std::max(1.0, std::round(bw * scale)));
	const auto tileH = static_cast<uint32_t>(std::max(1.0, std::round(bh * scale)));

	std::vector<float> buf(tileW * tileH);

	msdfgen::BitmapSection<float, 1> bmp(buf.data(), static_cast<int>(tileW), static_cast<int>(tileH));
	msdfgen::generateSDF(bmp, msdfShape, msdfTransform(bounds, tileW, tileH, range));

	// msdfgen is Y-up (row 0 = bottom); flip to Y-down (row 0 = top) for Grid.
	// Values are clamped to [0, 1]: edge = 0.5, interior > 0.5, exterior < 0.5.
	Grid grid{tileW, tileH, std::vector<slug_t>(tileW * tileH)};

	for(uint32_t row = 0; row < tileH; row++) {
		const uint32_t src = tileH - 1 - row;

		for(uint32_t col = 0; col < tileW; col++) grid.data[row * tileW + col] = std::clamp(
			buf[src * tileW + col],
			0.f,
			1.f
		);
	}

	return grid;
}

// Generate a square tileSize x tileSize MSDF tile for the given key.
// Unlike renderMSDF(), dimensions are always exactly tileSize x tileSize - shapes are
// letterboxed/pillarboxed as needed. Use this when building a sampler2DArray where all
// layers must have identical dimensions.
inline MSDFGrid renderMSDFTile(
	const Atlas& atlas,
	Key key,
	uint32_t tileSize=128,
	slug_t range=0.1_cv,
	Atlas::MSDFEdgeColoring coloring=Atlas::MSDFEdgeColoring::ByDistance
) {
	msdfgen::Shape msdfShape = toMSDFShape(atlas, key);

	if(msdfShape.contours.empty()) {
		return MSDFGrid{tileSize, tileSize, std::vector<float>(tileSize * tileSize * 3, 0.f)};
	}

	if(coloring == Atlas::MSDFEdgeColoring::ByDistance) msdfgen::edgeColoringByDistance(msdfShape, 3.0);
	else msdfgen::edgeColoringSimple(msdfShape, 3.0);

	// Expand bounds by range on all sides so the tile edge is deeply exterior (SDF << 0.5).
	// Without this, curves touching the tight bbox boundary leave edge texels at SDF ~= 0.5, which
	// CLAMP_TO_EDGE propagates into the quad's margin band (AA margin + layer bleed) as ghost
	// fringes. The shader's
	// tileUV accounts for this margin: (emCoord - emOrigin + range) / (emSize + 2*range).
	const auto bounds = msdfShape.getBounds(range);
	std::vector<float> buf(tileSize * tileSize * 3);

	msdfgen::BitmapSection<float, 3> bmp(
		buf.data(),
		static_cast<int>(tileSize),
		static_cast<int>(tileSize)
	);
	msdfgen::generateMSDF(bmp, msdfShape, msdfTransform(bounds, tileSize, tileSize, range));

	// msdfgen's DistanceMapping linearly maps [-range, range] -> [0, 1] with NO clamp; any point
	// whose true distance exceeds range (always true somewhere in the padded margin
	// renderMSDFTile's own bounds computation above creates) comes out <0 or >1. The GL_RGB32F
	// texture this feeds is real floats with no automatic clamping (unlike a normalized 8-bit
	// format), so that overshoot reaches the shader verbatim, and a legitimately-very-exterior
	// point can sample as negative, indistinguishable from a "no MSDF tile" sentinel unless the
	// caller checks v_msdfLayer directly instead of the sampled value's sign. Clamping here
	// (matching renderSDF's existing pattern) keeps the data itself well-defined regardless of how
	// callers interpret it.
	for(float& v : buf) v = std::clamp(v, 0.0f, 1.0f);

	// No Y-flip. This function has exactly one purpose: feeding the GPU sampler2DArray
	// via Atlas::requestMSDF() -> packTextures(). For that path all three conventions
	// agree without a flip, and a flip would break all of them simultaneously:
	//
	// msdfgen native : row 0 = bottom of shape (Y-up coordinate space)
	// osg::Image/GL : row 0 -> V=0 -> bottom of texture (OpenGL convention)
	// v_uv in shader : V=0 = bottom of shape bounding box
	//
	// There is intentionally no "flip" parameter here. A boolean that controls Y
	// orientation would push the burden of knowing the GPU contract onto every call
	// site, and both values would never be equally correct for this function's single
	// purpose. If you need Y-down (row 0 = top) for CPU display or image export, use
	// renderMSDF() - it keeps the flip for exactly that use case.
	MSDFGrid grid{tileSize, tileSize, std::vector<float>(tileSize * tileSize * 3)};

	std::memcpy(grid.data.data(), buf.data(), buf.size() * sizeof(float));

	return grid;
}

// Generate a multi-channel SDF tile for the given key.
// Returns an MSDFGrid with 3 floats per pixel; reconstruct with median(r,g,b) in shader.
inline MSDFGrid renderMSDF(
	const Atlas& atlas,
	Key key,
	uint32_t tileSize=128,
	slug_t range=0.1_cv
) {
	msdfgen::Shape msdfShape = toMSDFShape(atlas, key);

	if(msdfShape.contours.empty()) {
		return MSDFGrid{tileSize, tileSize, std::vector<float>(tileSize * tileSize * 3, 0.f)};
	}

	msdfgen::edgeColoringSimple(msdfShape, 3.0);

	const auto bounds = msdfShape.getBounds(range);
	const double bw = bounds.r - bounds.l, bh = bounds.t - bounds.b;
	const double scale = tileSize / std::max(bw, bh);
	const auto tileW = static_cast<uint32_t>(std::max(1.0, std::round(bw * scale)));
	const auto tileH = static_cast<uint32_t>(std::max(1.0, std::round(bh * scale)));

	std::vector<float> buf(tileW * tileH * 3);

	msdfgen::BitmapSection<float, 3> bmp(
		buf.data(),
		static_cast<int>(tileW),
		static_cast<int>(tileH)
	);
	msdfgen::generateMSDF(bmp, msdfShape, msdfTransform(bounds, tileW, tileH, range));

	// Flip Y
	MSDFGrid grid{tileW, tileH, std::vector<float>(tileW * tileH * 3)};

	for(uint32_t row = 0; row < tileH; row++) {
		const uint32_t src = tileH - 1 - row;

		for(uint32_t col = 0; col < tileW; col++) {
			for(uint32_t ch = 0; ch < 3; ch++) {
				grid.data[(row * tileW + col) * 3 + ch] = buf[(src * tileW + col) * 3 + ch];
			}
		}
	}

	return grid;
}

#endif

}
}
