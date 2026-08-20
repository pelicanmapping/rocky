#pragma once

// =============================================================================
// serial.hpp - Atlas serialization (.slug JSON / .slugb binary)
//
// Stb-style single-header companion to slughorn.hpp.
// Define SLUGHORN_SERIAL_IMPLEMENTATION in exactly one .cpp before including.
//
// Depends on:
//   slughorn.hpp
//   nlohmann/json.hpp (expected at $git/ext/json)
//
// Formats
// -------
//   .slug JSON + base64-encoded texture blobs.  Human-readable, single file.
//         Analogous to glTF's .gltf format.
//
//   .slugb Binary container: 12-byte file header, JSON chunk, BIN chunk.
//          Compact, single file, fast to load; analogous to glTF's .glb format.
//
// Container layout (.slugb)
// -------------------------
//   [magic:   4 bytes  "SLUG"     ]
//   [version: uint32_t 1          ]
//   [length:  uint32_t total bytes]
//
//   Chunk 0 - JSON
//     [chunk_length: uint32_t          ]  byte count of JSON data (before padding)
//     [chunk_type:   uint32_t 0x4E4F534A "JSON"]
//     [data:         chunk_length bytes, UTF-8 ]
//     [padding:      0-3 bytes 0x20 (space) to reach 4-byte alignment]
//
//   Chunk 1 - BIN
//     [chunk_length: uint32_t          ]  byte count of binary data (before padding)
//     [chunk_type:   uint32_t 0x004E4942 "BIN\0"]
//     [data:         chunk_length bytes ]
//     [padding:      0-3 bytes 0x00 to reach 4-byte alignment]
//
// JSON schema
// -----------
//   {
//     "asset": { "version": "1.0", "generator": "slughorn" },
//     "tex_width": 512,
//     "packing_stats": {
//       "curve_texels_used": 1652,
//       "curve_texels_padding": 0,
//       "curve_texels_total": 2048,
//       "band_texels_used": 3791,
//       "band_texels_padding": 29,
//       "band_texels_total": 4096
//     },
//     "bufferViews": [
//       { "byteOffset": 0,    "byteLength": N, "format": "RGBA32F",  "width": 512, "height": H },
//       { "byteOffset": N,    "byteLength": M, "format": "RGBA16UI", "width": 512, "height": H },
//       { "byteOffset": ...,  "byteLength": P, "format": "RGBA8",    "width": 512, "height": H },  // optional gradient
//       { "byteOffset": ...,  "byteLength": Q, "format": "RGB32F",   "width": T,   "height": T, "depth": L } // optional MSDF array
//     ],
//     "curve_texture": 0,
//     "band_texture":  1,
//     "gradient_texture": 2,   // optional
//     "msdf_texture": 2,       // optional (index depends on whether gradient is present)
//     "shapes": [
//       {
//         "key": { "type": "codepoint", "value": 70 },
//         "band_tex_x": 0, "band_tex_y": 0,
//         "band_max_x": 1, "band_max_y": 1,
//         "band_scale_x": 2.0, "band_scale_y": 2.0,
//         "band_offset_x": 0.0, "band_offset_y": 0.0,
//         "bearing_x": 0.0, "bearing_y": 0.7,
//         "width": 1.0, "height": 0.7, "advance": 1.0,
//         "msdf_layer": 0, "msdf_range": 0.1  // optional; only when requestMSDF() was called
//       }
//     ],
//     "composites": [
//       {
//         "key": { "type": "name", "value": "PIXEL" },
//         "advance": 5.2,
//         "layers": [
//           {
//             "key": { "type": "codepoint", "value": 80 },
//             "color": [1.0, 0.0, 0.0, 1.0],
//             "transform": [1.0, 0.0, 0.0, 1.0, 0.0, 0.0],
//             "effect_id": 0
//           }
//         ]
//       }
//     ]
//   }
//
//   In .slug: each bufferView gains "data": "<base64>" and byteOffset is 0.
//   In .slugb: byteOffset is a real byte offset into the BIN chunk; no "data".
//
// Usage
// -----
//   // Write
//   slughorn::serial::write(atlas, "logo.slug"); // JSON
//   slughorn::serial::write(atlas, "logo.slugb"); // binary
//
//   // Read (format auto-detected)
//   slughorn::Atlas atlas = slughorn::serial::read("logo.slug");
//   slughorn::Atlas atlas = slughorn::serial::read("logo.slugb");
// =============================================================================

#include "slughorn.hpp"
#include "render.hpp"

#ifdef SLUGHORN_SERIAL_IMPLEMENTATION
#include "nlohmann/json.hpp"
#endif

#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace slughorn::serial {

// =============================================================================
// Public API
// =============================================================================

// Write as JSON + base64 (.slug).
// pretty=true produces human-readable indented output (recommended default).
void writeJSON(const Atlas& atlas, std::ostream& out, bool pretty=true);

// Write as binary container (.slugb).
void writeBinary(const Atlas& atlas, std::ostream& out);

// Convenience: write to file path.
// Extension determines format: .slug -> JSON, .slugb -> binary.
// Throws std::runtime_error if the file cannot be opened.
void write(const Atlas& atlas, const std::string& path);

// Read either format - auto-detected ('{' -> JSON, 'S' -> binary).
// Returns a fully-built Atlas (is_built() == true).
// Throws std::runtime_error on parse errors or unknown format.
Atlas read(std::istream& in);

// Convenience: read from file path.
// Throws std::runtime_error if the file cannot be opened.
Atlas read(const std::string& path);

// =============================================================================
// Implementation
// =============================================================================

#ifdef SLUGHORN_SERIAL_IMPLEMENTATION

namespace {

using json = nlohmann::json;

// -----------------------------------------------------------------------------
// Base64
// -----------------------------------------------------------------------------

static constexpr char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64Encode(const std::vector<uint8_t>& data) {
	std::string out;
	out.reserve(((data.size() + 2) / 3) * 4);

	size_t i = 0;
	const size_t n = data.size();

	while(i < n) {
		const uint32_t a = data[i++];
		const uint32_t b = (i < n) ? data[i++] : 0u;
		const uint32_t c = (i < n) ? data[i++] : 0u;
		const uint32_t triple = (a << 16) | (b << 8) | c;

		out += B64[(triple >> 18) & 0x3F];
		out += B64[(triple >> 12) & 0x3F];
		out += B64[(triple >> 6) & 0x3F];
		out += B64[(triple >> 0) & 0x3F];
	}

	// Pad
	const size_t mod = data.size() % 3;

	if(mod >= 1) out[out.size() - 1] = '=';
	if(mod == 1) out[out.size() - 2] = '=';

	return out;
}

std::vector<uint8_t> base64Decode(const std::string& s) {
	// Build reverse lookup table
	static uint8_t lut[256] = {};
	static bool lutReady = false;

	if(!lutReady) {
		for(int i = 0; i < 64; i++) lut[(uint8_t)B64[i]] = (uint8_t)i;

		lutReady = true;
	}

	const size_t len = s.size();

	if(len % 4 != 0) throw std::runtime_error("slughorn-serial: base64 length not a multiple of 4");

	size_t padding = 0;

	if(len >= 1 && s[len - 1] == '=') padding++;
	if(len >= 2 && s[len - 2] == '=') padding++;

	std::vector<uint8_t> out;

	out.reserve((len / 4) * 3 - padding);

	for(size_t i = 0; i < len; i += 4) {
		const uint32_t triple =
			// TODO: Good grief this is ... ugly.
			((uint32_t)lut[(uint8_t)s[i]] << 18) |
			((uint32_t)lut[(uint8_t)s[i + 1]] << 12) |
			((uint32_t)lut[(uint8_t)s[i + 2]] << 6) |
			((uint32_t)lut[(uint8_t)s[i + 3]])
		;

		out.push_back((triple >> 16) & 0xFF);

		if(s[i + 2] != '=') out.push_back((triple >> 8) & 0xFF);
		if(s[i + 3] != '=') out.push_back((triple) & 0xFF);
	}

	return out;
}

// -----------------------------------------------------------------------------
// Key serialization helpers
// -----------------------------------------------------------------------------

json keyToJson(const Key& k) {
	if(k.type() == Key::Type::Codepoint) return {
		{"type", "codepoint"}, {"value", k.codepoint()}
	};

	return { {"type", "name"}, {"value", k.name()} };
}

Key keyFromJson(const json& j) {
	const std::string type = j.at("type");

	if(type == "codepoint") return Key(j.at("value").get<uint32_t>());
	if(type == "name") return Key(j.at("value").get<std::string>());

	throw std::runtime_error("slughorn-serial: unknown key type '" + type + "'");
}

json packingStatsToJson(const Atlas::PackingStats& p) {
	return {
		{"curve_texels_used", p.curveTexelsUsed},
		{"curve_texels_padding", p.curveTexelsPadding},
		{"curve_texels_total", p.curveTexelsTotal},
		{"band_texels_used", p.bandTexelsUsed},
		{"band_texels_padding", p.bandTexelsPadding},
		{"band_texels_total", p.bandTexelsTotal},
		{"band_max_count", p.bandMaxCount},
		{"band_max_offset", p.bandMaxOffset},
		{"gradient_count", p.gradientCount},
		{"gradient_texels_total", p.gradientTexelsTotal},
		{"sdf_tile_count", p.sdfTileCount},
		{"sdf_texels_used", p.sdfTexelsUsed},
		{"sdf_texels_padding", p.sdfTexelsPadding},
		{"sdf_texels_total", p.sdfTexelsTotal},
		{"msdf_layer_count", p.msdfLayerCount},
		{"msdf_tile_size", p.msdfTileSize},
		{"msdf_texels_total", p.msdfTexelsTotal}
	};
}

Atlas::PackingStats packingStatsFromJson(const json& j) {
	Atlas::PackingStats p;

	p.curveTexelsUsed = j.at("curve_texels_used").get<uint32_t>();
	p.curveTexelsPadding = j.at("curve_texels_padding").get<uint32_t>();
	p.curveTexelsTotal = j.at("curve_texels_total").get<uint32_t>();
	p.bandTexelsUsed = j.at("band_texels_used").get<uint32_t>();
	p.bandTexelsPadding = j.at("band_texels_padding").get<uint32_t>();
	p.bandTexelsTotal = j.at("band_texels_total").get<uint32_t>();
	p.bandMaxCount = j.value("band_max_count", 0u);
	p.bandMaxOffset = j.value("band_max_offset", 0u);
	p.gradientCount = j.value("gradient_count", 0u);
	p.gradientTexelsTotal = j.value("gradient_texels_total", 0u);
	p.sdfTileCount = j.value("sdf_tile_count", 0u);
	p.sdfTexelsUsed = j.value("sdf_texels_used", 0u);
	p.sdfTexelsPadding = j.value("sdf_texels_padding", 0u);
	p.sdfTexelsTotal = j.value("sdf_texels_total", 0u);
	p.msdfLayerCount = j.value("msdf_layer_count", 0u);
	p.msdfTileSize = j.value("msdf_tile_size", 0u);
	p.msdfTexelsTotal = j.value("msdf_texels_total", 0u);

	return p;
}

// -----------------------------------------------------------------------------
// Shared JSON builder (used by both writeJSON and writeBinary)
//
// embedBase64=true -> .slug: bufferViews contain "data" field, byteOffset=0
// embedBase64=false -> .slugb: bufferViews contain byteOffset into BIN chunk
// -----------------------------------------------------------------------------

json gradientTypeToString(GradientInfo::Type t) {
	switch(t) {
		case GradientInfo::Type::Linear:       return "linear";
		case GradientInfo::Type::Radial:       return "radial";
		case GradientInfo::Type::Sweep:        return "sweep";
		case GradientInfo::Type::AffineRadial: return "affine_radial";
	}

	return "linear";
}

GradientInfo::Type gradientTypeFromString(const std::string& s) {
	if(s == "radial")        return GradientInfo::Type::Radial;
	if(s == "sweep")         return GradientInfo::Type::Sweep;
	if(s == "affine_radial") return GradientInfo::Type::AffineRadial;

	return GradientInfo::Type::Linear;
}

json buildJson(
	const Atlas& atlas,
	bool embedBase64,
	uint32_t curveByteOffset=0,
	uint32_t bandByteOffset=0,
	uint32_t gradByteOffset=0,
	uint32_t msdfByteOffset=0
) {
	const Atlas::TextureData& curve = atlas.getCurveTextureData();
	const Atlas::TextureData& band = atlas.getBandTextureData();
	const Atlas::TextureData& grad = atlas.getGradientTextureData();
	const bool hasGradients = !grad.bytes.empty();
#ifdef SLUGHORN_HAS_MSDF
	const Atlas::TextureData& msdf = atlas.getMSDFTextureData();
	const bool hasMSDF = !msdf.bytes.empty();
#else
	const bool hasMSDF = false;
#endif

	json j;

	// Asset block
	j["asset"] = {
		{"version", "1.0"},
		{"generator", "slughorn"}
	};

	j["tex_width"] = atlas.getTextureWidth();
	j["packing_stats"] = packingStatsToJson(atlas.getPackingStats());

	// Buffer views
	json bv0 = {
		{"byteLength", curve.bytes.size()},
		{"format", "RGBA32F"},
		{"width", curve.width},
		{"height", curve.height}
	};

	json bv1 = {
		{"byteLength", band.bytes.size()},
		{"format", "RGBA16UI"},
		{"width", band.width},
		{"height", band.height}
	};

	if(embedBase64) {
		bv0["byteOffset"] = 0;
		bv0["data"] = base64Encode(curve.bytes);
		bv1["byteOffset"] = 0;
		bv1["data"] = base64Encode(band.bytes);
	}

	else {
		bv0["byteOffset"] = curveByteOffset;
		bv1["byteOffset"] = bandByteOffset;
	}

	json bufferViews = json::array({ bv0, bv1 });

	if(hasGradients) {
		json bv2 = {
			{"byteLength", grad.bytes.size()},
			{"format", "RGBA8"},
			{"width", grad.width},
			{"height", grad.height}
		};

		if(embedBase64) {
			bv2["byteOffset"] = 0;
			bv2["data"] = base64Encode(grad.bytes);
		}

		else bv2["byteOffset"] = gradByteOffset;

		bufferViews.push_back(bv2);

		j["gradient_texture"] = 2;
	}

#ifdef SLUGHORN_HAS_MSDF
	if(hasMSDF) {
		const size_t msdfBvIdx = bufferViews.size();

		json bvMSDF = {
			{"byteLength", msdf.bytes.size()},
			{"format", "RGB32F"},
			{"width", msdf.width},
			{"height", msdf.height},
			{"depth", msdf.depth}
		};

		if(embedBase64) {
			bvMSDF["byteOffset"] = 0;
			bvMSDF["data"] = base64Encode(msdf.bytes);
		}

		else bvMSDF["byteOffset"] = msdfByteOffset;

		bufferViews.push_back(bvMSDF);
		j["msdf_texture"] = msdfBvIdx;
	}
#endif

	j["bufferViews"] = bufferViews;
	j["curve_texture"] = 0;
	j["band_texture"] = 1;

	// Gradient metadata
	if(hasGradients) {
		json gradients = json::array();

		for(const auto& gi : atlas.getGradients()) {
			json stops = json::array();

			for(const auto& s : gi.stops) {
				json jstop;
				jstop["t"] = s.t;
				jstop["color"] = json::array({s.color.r, s.color.g, s.color.b, s.color.a});
				stops.push_back(jstop);
			}

			json jgi;

			jgi["type"] = gradientTypeToString(gi.type);
			jgi["transform"] = json::array({
				gi.transform.xx, gi.transform.yx,
				gi.transform.xy, gi.transform.yy,
				gi.transform.dx, gi.transform.dy
			});
			jgi["inner_radius"] = gi.innerRadius;
			jgi["start_angle"] = gi.startAngle;
			jgi["end_angle"] = gi.endAngle;
			jgi["stops"] = stops;

			gradients.push_back(jgi);
		}

		j["gradients"] = gradients;
	}

	// Shapes
	json shapes = json::array();

	for(const auto& [key, shape] : atlas.getShapes()) {
		const std::string originType = detail::to_sstr(shape.origin.type);

		json jshape = {
			{"key", keyToJson(key)},
			{"band_tex_x", shape.bandTexX},
			{"band_tex_y", shape.bandTexY},
			{"band_max_x", shape.bandMaxX},
			{"band_max_y", shape.bandMaxY},
			{"band_scale_x", shape.bandScaleX},
			{"band_scale_y", shape.bandScaleY},
			{"band_offset_x",shape.bandOffsetX},
			{"band_offset_y",shape.bandOffsetY},
			{"bearing_x", shape.bearingX},
			{"bearing_y", shape.bearingY},
			{"width", shape.width},
			{"height", shape.height},
			{"advance", shape.advance},
			{"origin_x", shape.originX},
			{"origin_y", shape.originY},
			{"origin", {{"type", originType}, {"x", shape.origin.x}, {"y", shape.origin.y}}}
		};

		if(shape.msdfLayer >= 0) {
			jshape["msdf_layer"] = shape.msdfLayer;
			jshape["msdf_range"] = shape.msdfRange;
		}

		shapes.push_back(std::move(jshape));
	}

	j["shapes"] = shapes;

	// Composites
	json composites = json::array();

	for(const auto& [key, composite] : atlas.getCompositeShapes()) {
		json layers = json::array();

		for(const auto& layer : composite.layers) {
			layers.push_back({
				{"key", keyToJson(layer.key)},
				{"color", {layer.color.r, layer.color.g, layer.color.b, layer.color.a}},
				{"transform", {layer.transform.x, layer.transform.y, layer.transform.z}},
				{"effect_id", layer.effectId},
				{"effect_param", static_cast<float>(layer.effectParam)},
				{"gradient_id", layer.gradientId},
				{"draw_mode", static_cast<uint8_t>(layer.drawMode)},
				{"blend_mode", static_cast<uint8_t>(layer.blendMode)}
			});
		}

		composites.push_back({
			{"key", keyToJson(key)},
			{"advance", composite.advance},
			{"layers", layers}
		});
	}

	j["composites"] = composites;

	return j;
}

// -----------------------------------------------------------------------------
// JSON - Atlas reconstruction (shared by read paths)
// -----------------------------------------------------------------------------

Atlas atlasFromJson(
	const json& j,
	const std::vector<uint8_t>* binChunk=nullptr // nullptr -> use base64 "data" fields
) {
	Atlas atlas;

	// Textures
	const json& bufferViews = j.at("bufferViews");

	auto loadTexture = [&](size_t index) -> Atlas::TextureData {
		const json& bv = bufferViews.at(index);

		Atlas::TextureData td;

		td.width = bv.at("width");
		td.height = bv.at("height");
		td.depth = bv.value("depth", 0u);

		const std::string fmt = bv.at("format");

		if (fmt == "RGBA32F") td.format = Atlas::TextureData::Format::RGBA32F;
		else if(fmt == "RGBA16UI") td.format = Atlas::TextureData::Format::RGBA16UI;
		else if(fmt == "RGBA8") td.format = Atlas::TextureData::Format::RGBA8;
		else if(fmt == "RGB32F") td.format = Atlas::TextureData::Format::RGB32F;
		else throw std::runtime_error("slughorn-serial: unknown texture format '" + fmt + "'");

		if(binChunk) {
			const uint32_t offset = bv.at("byteOffset");
			const uint32_t length = bv.at("byteLength");

			if(offset + length > binChunk->size()) throw
				std::runtime_error("slughorn-serial: bufferView out of range")
			;

			td.bytes.assign(
				binChunk->begin() + offset,
				binChunk->begin() + offset + length
			);
		}

		else td.bytes = base64Decode(bv.at("data").get<std::string>());

		return td;
	};

	// We need to inject pre-built texture data and the shape map into the
	// Atlas without re-running build(). We do this by calling the internal
	// reconstruction path exposed via the serial friend.
	Atlas::SerialData sd;

	sd.curveData = loadTexture(j.at("curve_texture"));
	sd.bandData = loadTexture(j.at("band_texture"));
	sd.packingStats = packingStatsFromJson(j.at("packing_stats"));

	if(j.contains("gradient_texture")) sd.gradientData = loadTexture(j.at("gradient_texture"));
	if(j.contains("msdf_texture")) sd.msdfData = loadTexture(j.at("msdf_texture"));

	if(j.contains("gradients")) {
		for(const json& jg : j.at("gradients")) {
			GradientInfo gi;

			gi.type = gradientTypeFromString(jg.value("type", "linear"));
			gi.innerRadius = jg.value("inner_radius", 0_cv);
			gi.startAngle = jg.value("start_angle", 0_cv);
			gi.endAngle = jg.value("end_angle", 1_cv);

			if(jg.contains("transform")) {
				const auto& jt = jg.at("transform");
				gi.transform = {
					jt[0], jt[1], jt[2], jt[3], jt[4], jt[5]
				};
			}

			if(jg.contains("stops")) {
				for(const json& js : jg.at("stops")) {
					GradientStop s;
					s.t = js.at("t").get<slug_t>();
					const auto& jc = js.at("color");
					s.color = { jc[0], jc[1], jc[2], jc[3] };
					gi.stops.push_back(s);
				}
			}

			sd.gradients.push_back(gi);
		}
	}

	// Shapes
	for(const json& js : j.at("shapes")) {
		Key key = keyFromJson(js.at("key"));

		Atlas::Shape shape;

		shape.bandTexX = js.at("band_tex_x");
		shape.bandTexY = js.at("band_tex_y");
		shape.bandMaxX = js.at("band_max_x");
		shape.bandMaxY = js.at("band_max_y");
		shape.bandScaleX = js.at("band_scale_x");
		shape.bandScaleY = js.at("band_scale_y");
		shape.bandOffsetX= js.at("band_offset_x");
		shape.bandOffsetY= js.at("band_offset_y");
		shape.bearingX = js.at("bearing_x");
		shape.bearingY = js.at("bearing_y");
		shape.width = js.at("width");
		shape.height = js.at("height");
		shape.advance = js.at("advance");
		shape.originX = js.value("origin_x", 0_cv);
		shape.originY = js.value("origin_y", 0_cv);
		shape.msdfLayer = js.value("msdf_layer", -1);
		shape.msdfRange = js.value("msdf_range", 0_cv);

		if(js.contains("origin")) {
			const auto& jo = js.at("origin");
			const std::string t = jo.value("type", "Default");
			Atlas::ShapeInfo::Origin::Type ot =
				t == "Centered" ? Atlas::ShapeInfo::Origin::Type::Centered :
				t == "Pivot"    ? Atlas::ShapeInfo::Origin::Type::Pivot    :
				t == "Custom"   ? Atlas::ShapeInfo::Origin::Type::Custom   :
				                  Atlas::ShapeInfo::Origin::Type::Default;
			shape.origin = Atlas::ShapeInfo::Origin(ot, jo.value("x", 0_cv), jo.value("y", 0_cv));
		}

		sd.shapes[key] = shape;

		// Repopulate curves from packed texture data so Shape::curves is valid
		// after load, matching the invariant established by Atlas::build().
		try {
			sd.shapes[key].curves = render::decode(
				sd.shapes[key], sd.curveData, sd.bandData
			).curves;
		}
		catch(...) {}
	}

	// Composites
	for(const json& jc : j.at("composites")) {
		Key key = keyFromJson(jc.at("key"));

		CompositeShape composite;
		composite.advance = jc.at("advance");

		for(const json& jl : jc.at("layers")) {
			Layer layer;

			layer.key = keyFromJson(jl.at("key"));
			layer.effectId = jl.at("effect_id");
			layer.effectParam = jl.value("effect_param", 0.0f);
			layer.gradientId = jl.value("gradient_id", 0u);
			layer.drawMode = static_cast<DrawMode>(jl.value("draw_mode", uint8_t(0)));
			layer.blendMode = static_cast<BlendMode>(jl.value("blend_mode", uint8_t(0)));

			const auto& jcolor = jl.at("color");
			layer.color = { jcolor[0], jcolor[1], jcolor[2], jcolor[3] };

			const auto& jxform = jl.at("transform");
			layer.transform = {jxform[0], jxform[1], jxform[2]}; // x, y, z

			composite.layers.push_back(layer);
		}

		sd.composites[key] = std::move(composite);
	}

	atlas.loadFromSerial(std::move(sd));

	return atlas;
}

// -----------------------------------------------------------------------------
// Binary container helpers
// -----------------------------------------------------------------------------

static constexpr uint32_t SLUG_MAGIC = 0x47554C53; // "SLUG"
static constexpr uint32_t SLUG_VERSION = 1;
static constexpr uint32_t SLUG_CHUNK_TYPE_JSON = 0x4E4F534A; // "JSON"
static constexpr uint32_t SLUG_CHUNK_TYPE_BIN = 0x004E4942; // "BIN\0"

void writeU32LE(std::ostream& out, uint32_t v) {
	const uint8_t bytes[4] = {
		uint8_t(v),
		uint8_t(v >> 8),
		uint8_t(v >> 16),
		uint8_t(v >> 24)
	};

	out.write(reinterpret_cast<const char*>(bytes), 4);
}

uint32_t readU32LE(std::istream& in) {
	uint8_t bytes[4];

	in.read(reinterpret_cast<char*>(bytes), 4);

	return
		uint32_t(bytes[0]) |
		uint32_t(bytes[1]) << 8 |
		uint32_t(bytes[2]) << 16 |
		uint32_t(bytes[3]) << 24
	;
}

// Pad a buffer to a multiple of 4 bytes.
// JSON chunks pad with spaces (0x20); BIN chunks pad with zeros.
void padTo4(std::vector<uint8_t>& buf, uint8_t padByte) {
	while(buf.size() % 4 != 0) buf.push_back(padByte);
}

} // anonymous namespace

// =============================================================================
// writeJSON
// =============================================================================

void writeJSON(const Atlas& atlas, std::ostream& out, bool pretty) {
	if(!atlas.isBuilt()) throw
		std::runtime_error("slughorn-serial: Atlas must be built before writing")
	;

	const json j = buildJson(atlas, /*embedBase64=*/true);

	out << (pretty ? j.dump(2) : j.dump());
}

// =============================================================================
// writeBinary
// =============================================================================

void writeBinary(const Atlas& atlas, std::ostream& out) {
	if(!atlas.isBuilt()) throw
		std::runtime_error("slughorn-serial: Atlas must be built before writing")
	;

	const Atlas::TextureData& curve = atlas.getCurveTextureData();
	const Atlas::TextureData& band = atlas.getBandTextureData();
	const Atlas::TextureData& grad = atlas.getGradientTextureData();

	// Build BIN chunk: curve, band, then optional gradient bytes (each naturally aligned)
	std::vector<uint8_t> binData;

	binData.insert(binData.end(), curve.bytes.begin(), curve.bytes.end());

	const uint32_t curveByteOffset = 0;
	const uint32_t bandByteOffset = static_cast<uint32_t>(curve.bytes.size());

	binData.insert(binData.end(), band.bytes.begin(), band.bytes.end());

	const uint32_t gradByteOffset = static_cast<uint32_t>(binData.size());

	if(!grad.bytes.empty()) binData.insert(binData.end(), grad.bytes.begin(), grad.bytes.end());

#ifdef SLUGHORN_HAS_MSDF
	const Atlas::TextureData& msdfBin = atlas.getMSDFTextureData();
	const uint32_t msdfByteOffset = static_cast<uint32_t>(binData.size());
	if(!msdfBin.bytes.empty()) binData.insert(binData.end(), msdfBin.bytes.begin(), msdfBin.bytes.end());
#else
	const uint32_t msdfByteOffset = 0;
#endif

	padTo4(binData, 0x00);

	// Build JSON chunk
	const json j = buildJson(atlas, /*embedBase64=*/false, curveByteOffset, bandByteOffset, gradByteOffset, msdfByteOffset);
	const std::string jsonStr = j.dump();

	std::vector<uint8_t> jsonData(jsonStr.begin(), jsonStr.end());

	padTo4(jsonData, 0x20); // pad with spaces

	// File header: magic + version + total_length
	// 12 bytes file header
	// + 8 bytes JSON chunk header + jsonData.size()
	// + 8 bytes BIN chunk header + binData.size()
	const uint32_t totalLength =
		12 +
		8 + static_cast<uint32_t>(jsonData.size()) +
		8 + static_cast<uint32_t>(binData.size())
	;

	// Write file header
	writeU32LE(out, SLUG_MAGIC);
	writeU32LE(out, SLUG_VERSION);
	writeU32LE(out, totalLength);

	// Write JSON chunk
	writeU32LE(out, static_cast<uint32_t>(jsonData.size()));
	writeU32LE(out, SLUG_CHUNK_TYPE_JSON);

	out.write(reinterpret_cast<const char*>(jsonData.data()), static_cast<std::streamsize>(jsonData.size()));

	// Write BIN chunk
	writeU32LE(out, static_cast<uint32_t>(binData.size()));
	writeU32LE(out, SLUG_CHUNK_TYPE_BIN);

	out.write(reinterpret_cast<const char*>(binData.data()), static_cast<std::streamsize>(binData.size()));
}

// =============================================================================
// write (path, format inferred from extension)
// =============================================================================

void write(const Atlas& atlas, const std::string& path) {
	const bool binary =
		path.size() >= 6 &&
		path.substr(path.size() - 6) == ".slugb"
	;

	std::ofstream f(path, binary ? (std::ios::out | std::ios::binary) : std::ios::out);

	if(!f) throw std::runtime_error("slughorn-serial: cannot open '" + path + "' for writing");

	if(binary) writeBinary(atlas, f);

	else writeJSON(atlas, f);
}

// =============================================================================
// read (stream)
// =============================================================================

Atlas read(std::istream& in) {
	// Peek at first byte to detect format
	const int first = in.peek();

	if(first == '{') {
		// .slug (JSON)
		json j = json::parse(in);

		return atlasFromJson(j, nullptr);
	}

	if(first == 'S') {
		// .slugb (binary container)
		const uint32_t magic = readU32LE(in);
		const uint32_t version = readU32LE(in);
		// TODO: This is related to the TODO below; why aren't we validating?
		[[maybe_unused]] const uint32_t length = readU32LE(in);

		if(magic != SLUG_MAGIC) throw
			std::runtime_error("slughorn-serial: not a .slugb file (bad magic)")
		;

		if(version != SLUG_VERSION) throw
			std::runtime_error("slughorn-serial: unsupported .slugb version " +
			std::to_string(version))
		;

		// Read JSON chunk
		const uint32_t jsonLength = readU32LE(in);
		const uint32_t jsonType = readU32LE(in);

		if(jsonType != SLUG_CHUNK_TYPE_JSON) throw
			std::runtime_error("slughorn-serial: expected JSON chunk first")
		;

		std::string jsonStr(jsonLength, '\0');

		in.read(jsonStr.data(), jsonLength);

		// Skip JSON padding (stream position must be 4-byte aligned)
		const uint32_t jsonPad = (4 - (jsonLength % 4)) % 4;

		in.seekg(jsonPad, std::ios::cur);

		// Read BIN chunk
		const uint32_t binLength = readU32LE(in);
		const uint32_t binType = readU32LE(in);

		if(binType != SLUG_CHUNK_TYPE_BIN) throw
			std::runtime_error("slughorn-serial: expected BIN chunk second")
		;

		std::vector<uint8_t> binData(binLength);

		in.read(reinterpret_cast<char*>(binData.data()), binLength);

		// TODO: Why?
		// (void)length; // total file length - used for validation if desired

		json j = json::parse(jsonStr);

		return atlasFromJson(j, &binData);
	}

	throw std::runtime_error(
		"slughorn-serial: unrecognised format (expected '{' for .slug or 'S' for .slugb)"
	);
}

// =============================================================================
// read (path)
// =============================================================================

Atlas read(const std::string& path) {
	const bool binary =
		path.size() >= 6 &&
		path.substr(path.size() - 6) == ".slugb"
	;

	std::ifstream f(path, binary ? (std::ios::in | std::ios::binary) : std::ios::in);

	if(!f) throw std::runtime_error("slughorn-serial: cannot open '" + path + "' for reading");

	return read(f);
}

#endif

}
