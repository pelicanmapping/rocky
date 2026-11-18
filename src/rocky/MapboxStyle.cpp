/** rocky c++; Copyright 2026 Pelican Mapping; MIT License */
#include "MapboxStyle.h"
#include "json.h"
#include <set>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <limits>
#include <cctype>

using namespace ROCKY_NAMESPACE;

namespace
{
    //! Parse the CSS colors used by standard map styles, without Rocky's legacy HSL hue convention.
    glm::dvec4 cssColor(const json& value)
    {
        if (value.is_array() && value.size() == 4)
            return {value[0].get<double>(), value[1].get<double>(), value[2].get<double>(), value[3].get<double>()};
        if (!value.is_string())
            throw std::runtime_error("expected a CSS color");
        auto s = value.get<std::string>();
        s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c); }), s.end());
        if (s == "transparent")
            return {0, 0, 0, 0};
        if (s == "black")
            return {0, 0, 0, 1};
        if (s == "white")
            return {1, 1, 1, 1};
        if (s.size() > 1 && s[0] == '#' && (s.size() == 4 || s.size() == 7 || s.size() == 9))
        {
            if (!std::all_of(s.begin() + 1, s.end(), [](unsigned char c) { return std::isxdigit(c); }))
                throw std::runtime_error("invalid hex color");
            return glm::dvec4(Color(s));
        }
        bool hsl = s.compare(0, 3, "hsl") == 0;
        bool rgb = s.compare(0, 3, "rgb") == 0;
        auto left = s.find('('), right = s.find(')');
        if ((!hsl && !rgb) || left == std::string::npos || right != s.size() - 1)
            throw std::runtime_error("unsupported CSS color: " + s);
        std::vector<std::string> parts;
        std::istringstream stream(s.substr(left + 1, right - left - 1));
        std::string part;
        while (std::getline(stream, part, ','))
            parts.push_back(part);
        if (parts.size() != 3 && parts.size() != 4)
            throw std::runtime_error("invalid CSS color components");
        glm::dvec4 c(0, 0, 0, 1);
        for (unsigned i = 0; i < parts.size(); ++i)
        {
            c[i] = std::stod(parts[i]);
            if (!std::isfinite(c[i]))
                throw std::runtime_error("nonfinite CSS color");
        }
        if (parts.size() == 4 && parts[3].back() == '%')
            c.a /= 100;
        if (hsl)
        {
            double h = std::fmod(c.r, 360.0) / 60.0;
            if (h < 0)
                h += 6;
            const double sat = std::clamp(c.g / 100.0, 0.0, 1.0), light = std::clamp(c.b / 100.0, 0.0, 1.0);
            const double chroma = (1 - std::abs(2 * light - 1)) * sat, x = chroma * (1 - std::abs(std::fmod(h, 2.0) - 1));
            glm::dvec3 v = h < 1   ? glm::dvec3(chroma, x, 0)
                           : h < 2 ? glm::dvec3(x, chroma, 0)
                           : h < 3 ? glm::dvec3(0, chroma, x)
                           : h < 4 ? glm::dvec3(0, x, chroma)
                           : h < 5 ? glm::dvec3(x, 0, chroma)
                                   : glm::dvec3(chroma, 0, x);
            c = glm::dvec4(v + (light - chroma / 2), c.a);
        }
        else
            for (unsigned i = 0; i < 3; ++i)
                c[i] /= parts[i].back() == '%' ? 100.0 : 255.0;
        return glm::clamp(c, glm::dvec4(0), glm::dvec4(1));
    }

    //! Convert a typed feature attribute to the expression value domain.
    json field(const Feature& f, const std::string& name)
    {
        if (!f.hasField(name))
            return nullptr;
        const auto& v = f.field(name);
        return std::visit(
            [](const auto& x) -> json
            {
                using T = std::decay_t<decltype(x)>;
                if constexpr (std::is_same_v<T, std::monostate>)
                    return nullptr;
                else
                    return x;
            },
            static_cast<const Feature::FieldValueUnion&>(v));
    }

    //! Translate legacy filter property operands to equivalent expression nodes.
    json filterExpression(json e)
    {
        if (!e.is_array() || e.empty() || !e[0].is_string())
            return e;
        const auto op = e[0].get<std::string>();
        if (op == "all" || op == "any" || op == "none")
        {
            for (unsigned i = 1; i < e.size(); ++i)
                e[i] = filterExpression(e[i]);
            if (op == "none")
            {
                e[0] = "any";
                return json::array({"!", e});
            }
        }
        else if (op == "!has")
            return json::array({"!", json::array({"has", e.at(1)})});
        else if (e.size() >= 3 && e[1].is_string() &&
                 std::none_of(e.begin() + 2, e.end(),
                              [](const json& value) { return value.is_array() || value.is_object(); }) &&
                 (op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=" || op == "in" || op == "!in"))
        {
            const auto name = e[1].get<std::string>();
            e[1] = name == "$type" ? json::array({"geometry-type"})
                   : name == "$id" ? json::array({"id"})
                                   : json::array({"get", name});
            if (op == "in" || op == "!in")
            {
                json values = json::array();
                for (unsigned i = 2; i < e.size(); ++i)
                    values.push_back(e[i]);
                auto result = json::array({"in", e[1], json::array({"literal", values})});
                return op == "!in" ? json::array({"!", result}) : result;
            }
        }
        return e;
    }

    //! Validate operator arity and literal positions once, before accepting a style layer.
    void validate(const json& e, unsigned depth = 0)
    {
        if (depth > 32)
            throw std::runtime_error("expression nesting exceeds 32");
        if (e.is_object())
            throw std::runtime_error("legacy stop functions are not supported; use expressions");
        if (!e.is_array())
            return;
        if (e.empty() || !e[0].is_string())
            throw std::runtime_error("expected expression operator");
        const auto op = e[0].get<std::string>();
        const auto n = e.size();
        if (op == "literal")
        {
            if (n != 2)
                throw std::runtime_error("literal arity");
            return;
        }
        if (op == "match")
        {
            if (n < 5 || n % 2 != 1)
                throw std::runtime_error("match arity");
            validate(e[1], depth + 1);
            for (std::size_t i = 3; i < n; i += 2)
                validate(e[i], depth + 1);
            validate(e.back(), depth + 1);
            return;
        }
        if (op == "interpolate")
        {
            if (n < 7 || n % 2 != 1 || !e[1].is_array() || e[1].empty())
                throw std::runtime_error("interpolate arity");
            if (e[1] != json::array({"linear"}) &&
                !(e[1].size() == 2 && e[1][0] == "exponential" && e[1][1].is_number() && e[1][1].get<double>() > 0))
                throw std::runtime_error("only linear/exponential interpolation is supported");
            validate(e[2], depth + 1);
            double previous = -std::numeric_limits<double>::infinity();
            for (std::size_t i = 3; i < n; i += 2)
            {
                if (!e[i].is_number() || e[i].get<double>() <= previous)
                    throw std::runtime_error("unordered stops");
                previous = e[i].get<double>();
                validate(e[i + 1], depth + 1);
            }
            return;
        }
        const bool validArity =
            ((op == "zoom" || op == "geometry-type" || op == "id") && n == 1) ||
            ((op == "get" || op == "has") && n == 2 && e[1].is_string()) || (op == "!" && n == 2) ||
            ((op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=" || op == "in") && n == 3) ||
            ((op == "all" || op == "any") && n >= 1) ||
            ((op == "coalesce" || op == "to-number" || op == "number" || op == "boolean" || op == "string") && n >= 2) ||
            ((op == "+" || op == "*" || op == "min" || op == "max") && n >= 3) || ((op == "-" || op == "/") && n == 3) ||
            (op == "case" && n >= 4 && n % 2 == 0) || (op == "step" && n >= 3 && n % 2 == 1);
        if (!validArity)
            throw std::runtime_error("unsupported operator or arity: " + op);
        if (op == "step")
        {
            double previous = -std::numeric_limits<double>::infinity();
            for (std::size_t i = 3; i < n; i += 2)
            {
                if (!e[i].is_number() || e[i].get<double>() <= previous)
                    throw std::runtime_error("unordered stops");
                previous = e[i].get<double>();
            }
        }
        for (std::size_t i = 1; i < n; ++i)
            validate(e[i], depth + 1);
    }

    //! Evaluate a validated tree; branch and fallback operators are deliberately lazy.
    json eval(const json& e, const Feature& f, double zoom)
    {
        if (!e.is_array())
            return e;
        const auto op = e[0].get<std::string>();
        const auto n = e.size();
        auto at = [&](std::size_t i) { return eval(e.at(i), f, zoom); };
        if (op == "literal")
            return e[1];
        if (op == "zoom")
            return zoom;
        if (op == "get")
            return field(f, e[1]);
        if (op == "has")
            return f.hasField(e[1]);
        if (op == "id")
            return f.id < 0 ? json(nullptr) : json(f.id);
        if (op == "geometry-type")
            return f.geometry.type == Geometry::Type::Polygon || f.geometry.type == Geometry::Type::MultiPolygon ? "Polygon"
                   : f.geometry.type == Geometry::Type::LineString || f.geometry.type == Geometry::Type::MultiLineString
                       ? "LineString"
                       : "Point";
        if (op == "!")
            return !at(1).get<bool>();
        if (op == "all" || op == "any")
        {
            for (std::size_t i = 1; i < n; ++i)
                if (at(i).get<bool>() == (op == "any"))
                    return op == "any";
            return op == "all";
        }
        if (op == "case")
        {
            for (std::size_t i = 1; i + 1 < n; i += 2)
                if (at(i).get<bool>())
                    return at(i + 1);
            return at(n - 1);
        }
        if (op == "match")
        {
            auto value = at(1);
            for (std::size_t i = 2; i + 1 < n; i += 2)
                if (e[i] == value || (e[i].is_array() && std::find(e[i].begin(), e[i].end(), value) != e[i].end()))
                    return at(i + 1);
            return at(n - 1);
        }
        if (op == "coalesce" || op == "to-number" || op == "number" || op == "boolean" || op == "string")
        {
            for (std::size_t i = 1; i < n; ++i)
            {
                auto value = at(i);
                if (op == "coalesce" && !value.is_null())
                    return value;
                if ((op == "number" || op == "to-number") && value.is_number())
                    return value;
                if (op == "boolean" && value.is_boolean())
                    return value;
                if (op == "string" && value.is_string())
                    return value;
                if (op == "to-number")
                {
                    if (value.is_null())
                        return 0;
                    if (value.is_boolean())
                        return value.get<bool>() ? 1 : 0;
                    if (value.is_string())
                        try
                        {
                            std::size_t used = 0;
                            auto s = value.get<std::string>();
                            double d = std::stod(s, &used);
                            if (used == s.size() && std::isfinite(d))
                                return d;
                        }
                        catch (...)
                        {
                        }
                }
            }
            if (op == "coalesce")
                return nullptr;
            throw std::runtime_error("expression type assertion failed: " + op);
        }
        if (op == "step" || op == "interpolate")
        {
            const bool interpolate = op == "interpolate";
            const double value = at(interpolate ? 2 : 1).get<double>();
            std::size_t first = 3;
            if (!interpolate && (n == 3 || value < e[3].get<double>()))
                return at(2);
            if (interpolate && value <= e[first].get<double>())
                return at(first + 1);
            for (std::size_t i = first; i + 2 < n; i += 2)
            {
                const double lo = e[i].get<double>(), hi = e[i + 2].get<double>();
                if (value < hi)
                {
                    if (!interpolate)
                        return at(i + 1);
                    double t = (value - lo) / (hi - lo);
                    if (e[1][0] == "exponential")
                    {
                        const double base = e[1][1].get<double>();
                        if (base != 1)
                            t = (std::pow(base, value - lo) - 1) / (std::pow(base, hi - lo) - 1);
                    }
                    const auto a = at(i + 1), b = at(i + 3);
                    if (a.is_number() && b.is_number())
                        return a.get<double>() * (1 - t) + b.get<double>() * t;
                    auto c = glm::mix(cssColor(a), cssColor(b), t);
                    return json::array({c.r, c.g, c.b, c.a});
                }
            }
            return at(n - 1);
        }
        auto a = at(1), b = at(2);
        if (op == "==")
            return a == b;
        if (op == "!=")
            return a != b;
        if (op == "in")
        {
            if (b.is_array())
                return std::find(b.begin(), b.end(), a) != b.end();
            if (a.is_string() && b.is_string())
                return b.get<std::string>().find(a.get<std::string>()) != std::string::npos;
            return false;
        }
        if (op == "<" || op == ">" || op == "<=" || op == ">=")
        {
            if (!(a.is_number() && b.is_number()) && !(a.is_string() && b.is_string()))
                return false;
            if (op == "<")
                return a < b;
            if (op == ">")
                return a > b;
            if (op == "<=")
                return a <= b;
            return a >= b;
        }
        double result = a.get<double>();
        for (std::size_t i = 2; i < n; ++i)
        {
            double v = at(i).get<double>();
            if (op == "+")
                result += v;
            else if (op == "-")
                result -= v;
            else if (op == "*")
                result *= v;
            else if (op == "/")
            {
                if (v == 0)
                    throw std::runtime_error("division by zero");
                result /= v;
            }
            else if (op == "min")
                result = std::min(result, v);
            else if (op == "max")
                result = std::max(result, v);
        }
        return result;
    }
} // namespace

struct MapboxStyle::Impl
{
    std::vector<Source> sources;
    std::vector<Layer> layers;
    std::vector<json> plans;
    std::vector<std::string> diagnostics;
};

Result<> MapboxStyle::load(std::string_view text)
{
    auto impl = std::make_shared<Impl>();
    try
    {
        auto root = json::parse(text);
        if (root.at("version") != 8 || !root.at("sources").is_object() || !root.at("layers").is_array())
            return Failure("Expected a version 8 style with sources and layers");
        for (const auto& entry : root["sources"].items())
        {
            const auto& s = entry.value();
            if (s.value("type", "") != "vector")
                continue;
            if (s.value("scheme", "xyz") != "xyz")
                return Failure("Only XYZ MVT sources are supported");
            Source source;
            source.id = entry.key();
            source.url = s.value("url", "");
            source.tiles = s.value("tiles", std::vector<std::string>{});
            source.minZoom = s.value("minzoom", 0u);
            source.maxZoom = s.value("maxzoom", 14u);
            source.attribution = s.value("attribution", "");
            impl->sources.push_back(std::move(source));
        }
        std::set<std::string> ids;
        for (auto plan : root["layers"])
        {
            const auto id = plan.at("id").get<std::string>();
            if (!ids.insert(id).second)
                return Failure("Duplicate style layer: " + id);
            const auto type = plan.value("type", "");
            if (type != "background" && type != "fill" && type != "line")
            {
                impl->diagnostics.push_back(id + ": omitted unsupported layer type " + type);
                continue;
            }
            try
            {
                Layer layer;
                layer.id = id;
                layer.source = plan.value("source", "");
                layer.sourceLayer = plan.value("source-layer", "");
                layer.type = type == "line" ? Type::Line : type == "fill" ? Type::Fill : Type::Background;
                layer.minZoom = plan.value("minzoom", 0.0);
                layer.maxZoom = plan.value("maxzoom", 24.0);
                if (type != "background" &&
                    (layer.sourceLayer.empty() || std::none_of(impl->sources.begin(), impl->sources.end(),
                                                               [&](const auto& s) { return s.id == layer.source; })))
                    throw std::runtime_error("missing vector source or source-layer");
                plan["filter"] = filterExpression(plan.value("filter", json(true)));
                validate(plan["filter"]);
                if (!plan.contains("paint"))
                    plan["paint"] = json::object();
                if (!plan.contains("layout"))
                    plan["layout"] = json::object();
                for (const auto& item : plan["paint"].items())
                {
                    const auto& key = item.key();
                    if (key == type + "-color" || key == type + "-opacity" ||
                        (type == "line" && (key == "line-width" || key == "line-gap-width")))
                    {
                        validate(item.value());
                        if (key == type + "-color" && item.value().is_string())
                            cssColor(item.value());
                    }
                    else if (key == "fill-antialias" || key == "fill-outline-color")
                        impl->diagnostics.push_back(id + ": " + key + " ignored (analytic fill coverage only)");
                    else
                        throw std::runtime_error("unsupported paint property " + key);
                }
                for (const auto& item : plan["layout"].items())
                {
                    if (item.key() == "visibility")
                        validate(item.value());
                    else if (item.key() == "line-cap" || item.key() == "line-join")
                    {
                        if (item.value() != "round")
                            impl->diagnostics.push_back(id + ": " + item.key() + " approximated as round");
                    }
                    else
                        throw std::runtime_error("unsupported layout property " + item.key());
                }
                impl->layers.push_back(std::move(layer));
                impl->plans.push_back(std::move(plan));
            }
            catch (const std::exception& e)
            {
                impl->diagnostics.push_back(id + ": omitted: " + e.what());
            }
        }
        if (impl->layers.empty())
            return Failure("Style contains no supported paint layers");
        _impl = impl;
        return ResultVoidOK;
    }
    catch (const std::exception& e)
    {
        return Failure(std::string("Invalid style: ") + e.what());
    }
}

const std::vector<MapboxStyle::Source>& MapboxStyle::sources() const
{
    static const std::vector<Source> empty;
    return _impl ? _impl->sources : empty;
}
const std::vector<MapboxStyle::Layer>& MapboxStyle::layers() const
{
    static const std::vector<Layer> empty;
    return _impl ? _impl->layers : empty;
}
const std::vector<std::string>& MapboxStyle::diagnostics() const
{
    static const std::vector<std::string> empty;
    return _impl ? _impl->diagnostics : empty;
}

Result<std::optional<MapboxStyle::Paint>> MapboxStyle::evaluate(std::size_t index, const Feature& f, double zoom) const
{
    try
    {
        if (!_impl || index >= _impl->layers.size() || !std::isfinite(zoom))
            return Failure("Invalid style evaluation");
        const auto& layer = _impl->layers[index];
        const auto& plan = _impl->plans[index];
        if (zoom < layer.minZoom || zoom >= layer.maxZoom ||
            eval(plan["layout"].value("visibility", json("visible")), f, zoom) == "none" ||
            !eval(plan["filter"], f, zoom).get<bool>())
            return std::optional<Paint>{};
        const auto type = layer.type == Type::Background ? "background" : layer.type == Type::Fill ? "fill" : "line";
        const auto& paint = plan["paint"];
        auto c = cssColor(eval(paint.value(std::string(type) + "-color", json("#000000")), f, zoom));
        double opacity = eval(paint.value(std::string(type) + "-opacity", json(1.0)), f, zoom).get<double>();
        Paint out;
        for (unsigned i = 0; i < 3; ++i)
            out.color[i] = static_cast<float>(c[i] <= 0.04045 ? c[i] / 12.92 : std::pow((c[i] + 0.055) / 1.055, 2.4));
        out.color.a = static_cast<float>(c.a * std::clamp(opacity, 0.0, 1.0));
        out.widthPixels = eval(paint.value("line-width", json(1.0)), f, zoom).get<double>();
        out.gapPixels = eval(paint.value("line-gap-width", json(0.0)), f, zoom).get<double>();
        if (!std::isfinite(opacity) || !std::isfinite(out.widthPixels) || !std::isfinite(out.gapPixels) ||
            out.widthPixels < 0 || out.gapPixels < 0)
            return Failure("Invalid numeric paint value in " + layer.id);
        if (out.color.a == 0 || (layer.type == Type::Line && out.widthPixels == 0))
            return std::optional<Paint>{};
        return std::optional<Paint>(out);
    }
    catch (const std::exception& e)
    {
        return Failure(std::string("Style evaluation: ") + e.what());
    }
}
