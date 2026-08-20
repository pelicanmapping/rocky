/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "SlugSystem.h"
#include "SlugResource.h"

#include <rocky/ecs/Line.h>
#include <rocky/ecs/Mesh.h>
#include <rocky/ecs/Overlay.h>
#include <rocky/ecs/Point.h>
#include <rocky/ecs/Transform.h>
#include <rocky/vsg/SharedRenderData.h>

#include <SlugAdapter.h>

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

using namespace ROCKY_NAMESPACE;

namespace
{
    using rocky::detail::SlugAtlasInput;
    using rocky::detail::SlugAtlasOutput;
    using rocky::detail::SlugCircleInput;
    using rocky::detail::SlugContourInput;
    using rocky::detail::SlugPointInput;
    using rocky::detail::SlugShapeInput;
    using rocky::detail::SlugShapeKind;
    using rocky::detail::SlugTextureFormat;

    struct OverlayBuild
    {
        entt::entity entity = entt::null;
        std::vector<SlugShapeInput> shapes;
        std::vector<SlugLayerResource> layers;
        vsg::ref_ptr<vsg::ImageInfo> curveTexture;
        vsg::ref_ptr<vsg::ImageInfo> bandTexture;
        std::size_t sourceSignature = 0u;
        std::uint32_t textureWidthLog2 = 0u;
        std::uint32_t indirectionSize = 0u;
        bool exportOnly = false;
        std::string exportPath;
        bool exportAttempted = false;
        bool exportSucceeded = false;
        std::string exportMessage;
        std::string error;
    };

    struct SlugPointMapper
    {
        bool useSRSOperation = false;
        bool transformInput = false;
        SRSOperation toWorld;
        glm::dmat4 inputToCoordinates = glm::dmat4(1.0);
        std::array<float, 4> uvToEmX = { 1.0f, 0.0f, 0.0f, 0.0f };
        std::array<float, 4> uvToEmY = { 0.0f, 1.0f, 0.0f, 0.0f };

        bool map(const glm::dvec3& point, SlugPointInput& output) const
        {
            glm::dvec3 input = point;
            if (useSRSOperation)
            {
                if (!toWorld.transform(point, input))
                    return false;
            }

            glm::dvec3 coordinates = input;
            if (transformInput)
            {
                const auto homogeneous = inputToCoordinates * glm::dvec4(
                    input.x,
                    input.y,
                    input.z,
                    1.0);
                if (!std::isfinite(homogeneous.w) ||
                    std::abs(homogeneous.w) <= std::numeric_limits<double>::epsilon())
                    return false;

                coordinates = glm::dvec3(homogeneous) / homogeneous.w;
            }

            output = {
                static_cast<float>(coordinates.x + 0.5),
                static_cast<float>(coordinates.y + 0.5)
            };
            return std::isfinite(output.x) && std::isfinite(output.y);
        }
    };

    struct SlugMetricSpace
    {
        double referenceMeters = 1.0;
        std::array<float, 4> uvToEmX = { 1.0f, 0.0f, 0.0f, 0.0f };
        std::array<float, 4> uvToEmY = { 0.0f, 1.0f, 0.0f, 0.0f };

        bool map(const SlugPointInput& input, SlugPointInput& output) const
        {
            output = {
                input.x * uvToEmX[0] + input.y * uvToEmX[1] + uvToEmX[2],
                input.x * uvToEmY[0] + input.y * uvToEmY[1] + uvToEmY[2]
            };
            return std::isfinite(output.x) && std::isfinite(output.y);
        }
    };

    void hashCombine(std::size_t& seed, std::size_t value)
    {
        seed ^= value + static_cast<std::size_t>(0x9e3779b9u) + (seed << 6u) + (seed >> 2u);
    }

    template<typename T>
    void hashValue(std::size_t& seed, const T& value)
    {
        hashCombine(seed, std::hash<T>{}(value));
    }

    template<typename T>
    const T* resolveComponent(
        entt::registry& registry,
        entt::entity reference,
        entt::entity owner)
    {
        const auto entity = reference != entt::null ? reference : owner;
        return registry.valid(entity) ? registry.try_get<T>(entity) : nullptr;
    }

    template<typename T>
    void hashComponent(std::size_t& signature, const T* component)
    {
        hashValue(signature, component != nullptr);
        if (component)
            hashValue(signature, component->componentRevision());
    }

    std::array<float, 4> toArray(const Color& color)
    {
        return { color[0], color[1], color[2], color[3] };
    }

    bool isFinite(const Color& color)
    {
        return
            std::isfinite(color[0]) &&
            std::isfinite(color[1]) &&
            std::isfinite(color[2]) &&
            std::isfinite(color[3]);
    }

    bool isFiniteXY(const glm::dvec3& point)
    {
        return std::isfinite(point.x) && std::isfinite(point.y);
    }

    bool makeSlugPointMapper(
        const SRS& inputSRS,
        const Transform* projector,
        const SRS& worldSRS,
        SlugPointMapper& output,
        std::string& error)
    {
        output = {};
        if (!inputSRS.valid())
            return true;

        if (!worldSRS.valid())
        {
            error = "Slug georeferenced geometry requires a valid world SRS";
            return false;
        }
        if (!projector || !projector->position.valid())
        {
            error = "Slug georeferenced geometry requires a fitted projector Transform";
            return false;
        }

        output.toWorld = inputSRS.to(worldSRS);
        if (!output.toWorld.valid())
        {
            error = "Slug geometry SRS cannot transform to the world SRS";
            return false;
        }
        output.useSRSOperation = true;

        const auto position = projector->position.transform(worldSRS);
        if (!position.valid())
        {
            error = "Slug projector position cannot transform to the world SRS";
            return false;
        }

        auto frameToWorld = projector->topocentric ?
            worldSRS.topocentricToWorldMatrix(glm::dvec3(position.x, position.y, position.z)) :
            glm::translate(glm::dmat4(1.0), glm::dvec3(position.x, position.y, position.z));

        const auto inputToCoordinates = frameToWorld * projector->localMatrix;

        const auto determinant = glm::determinant(inputToCoordinates);
        if (!std::isfinite(determinant) ||
            std::abs(determinant) <= std::numeric_limits<double>::epsilon())
        {
            error = "Slug projector Transform is singular";
            return false;
        }

        output.transformInput = true;
        output.inputToCoordinates = glm::inverse(inputToCoordinates);
        return true;
    }

    float normalizedPixelWidth(const Overlay& overlay, float pixels, std::uint32_t fallback)
    {
        const auto width = overlay.textureSize.x != 0u ? overlay.textureSize.x : fallback;
        const auto height = overlay.textureSize.y != 0u ? overlay.textureSize.y : fallback;
        const auto reference = static_cast<float>(std::max(1u, std::min(width, height)));
        return pixels / reference;
    }

    bool makeSlugMetricSpace(
        const Transform* projector,
        const SRS& worldSRS,
        SlugMetricSpace& output,
        std::string& error)
    {
        output = {};
        if (!projector || !projector->position.valid())
        {
            error = "Slug metric line width requires a projector Transform";
            return false;
        }

        if (!worldSRS.valid())
        {
            error = "Slug metric line width requires distance-based world SRS units";
            return false;
        }
        const auto worldUnits = worldSRS.units();
        if (!worldUnits.isDistance())
        {
            error = "Slug metric line width requires distance-based world SRS units";
            return false;
        }

        const double unitsToMeters = worldUnits.convertTo(Units::METERS, 1.0);
        const glm::dvec3 xAxis =
            glm::dvec3(projector->localMatrix[0]) * unitsToMeters;
        const glm::dvec3 yAxis =
            glm::dvec3(projector->localMatrix[1]) * unitsToMeters;
        const double xLength = glm::length(xAxis);
        if (!std::isfinite(xLength) || xLength <= std::numeric_limits<double>::epsilon())
        {
            error = "Slug projector Transform has no finite horizontal extent";
            return false;
        }

        // Express the projector plane in an orthonormal metric basis. This
        // preserves distances for anisotropic and sheared projectors while
        // keeping every unit-square corner inside Slughorn's stable [0,1]
        // canvas. referenceMeters is the common scale factored out below.
        const glm::dvec3 xDirection = xAxis / xLength;
        const double yAlongX = glm::dot(yAxis, xDirection);
        const glm::dvec3 yPerpendicularVector = yAxis - yAlongX * xDirection;
        const double yPerpendicular = glm::length(yPerpendicularVector);
        const double referenceMeters = std::max(
            xLength + std::abs(yAlongX),
            yPerpendicular);
        if (!std::isfinite(yAlongX) ||
            !std::isfinite(yPerpendicular) ||
            yPerpendicular <= std::numeric_limits<double>::epsilon() ||
            !std::isfinite(referenceMeters) ||
            referenceMeters <= std::numeric_limits<double>::epsilon())
        {
            error = "Slug projector Transform has a singular horizontal plane";
            return false;
        }

        const double xx = xLength / referenceMeters;
        const double xy = yAlongX / referenceMeters;
        const double yy = yPerpendicular / referenceMeters;
        output.referenceMeters = referenceMeters;
        output.uvToEmX = {
            static_cast<float>(xx),
            static_cast<float>(xy),
            static_cast<float>(0.5 - 0.5 * (xx + xy)),
            0.0f };
        output.uvToEmY = {
            0.0f,
            static_cast<float>(yy),
            static_cast<float>(0.5 - 0.5 * yy),
            0.0f };
        return true;
    }

    bool addMesh(
        entt::registry& registry,
        entt::entity owner,
        const SRS& worldSRS,
        const Mesh& mesh,
        OverlayBuild& build)
    {
        const auto* geometry = resolveComponent<MeshGeometry>(registry, mesh.geometry, owner);
        if (!geometry)
        {
            build.error = "Slug Mesh has no MeshGeometry";
            return false;
        }

        SlugPointMapper mapper;
        if (!makeSlugPointMapper(
            geometry->srs,
            registry.try_get<Transform>(owner),
            worldSRS,
            mapper,
            build.error))
            return false;

        const auto* style = resolveComponent<MeshStyle>(registry, mesh.style, owner);
        const Color color = style ? style->color : StockColor::White;
        if (!isFinite(color))
        {
            build.error = "Slug MeshStyle color must be finite";
            return false;
        }

        SlugShapeInput shape;
        shape.owner = static_cast<std::uint32_t>(entt::to_integral(owner));
        shape.kind = SlugShapeKind::Fill;
        shape.color = toArray(color);

        auto addTriangle = [&](std::uint32_t i0, std::uint32_t i1, std::uint32_t i2)
        {
            if (i0 >= geometry->vertices.size() ||
                i1 >= geometry->vertices.size() ||
                i2 >= geometry->vertices.size())
            {
                build.error = "Slug MeshGeometry contains an out-of-range index";
                return false;
            }

            if (!isFiniteXY(geometry->vertices[i0]) ||
                !isFiniteXY(geometry->vertices[i1]) ||
                !isFiniteXY(geometry->vertices[i2]))
            {
                build.error = "Slug MeshGeometry contains a non-finite XY coordinate";
                return false;
            }

            SlugPointInput p0, p1, p2;
            if (!mapper.map(geometry->vertices[i0], p0) ||
                !mapper.map(geometry->vertices[i1], p1) ||
                !mapper.map(geometry->vertices[i2], p2))
            {
                build.error = "Slug MeshGeometry coordinate transformation failed";
                return false;
            }

            SlugContourInput contour;
            contour.closed = true;
            contour.points = { p0, p1, p2 };
            shape.contours.emplace_back(std::move(contour));
            return true;
        };

        if (!geometry->indices.empty())
        {
            if ((geometry->indices.size() % 3u) != 0u)
            {
                build.error = "Slug MeshGeometry indices must be a triangle list";
                return false;
            }

            for (std::size_t i = 0u; i < geometry->indices.size(); i += 3u)
            {
                if (!addTriangle(
                    geometry->indices[i],
                    geometry->indices[i + 1u],
                    geometry->indices[i + 2u]))
                {
                    return false;
                }
            }
        }
        else
        {
            if ((geometry->vertices.size() % 3u) != 0u)
            {
                build.error = "Unindexed Slug MeshGeometry must contain triangle-list vertices";
                return false;
            }

            for (std::uint32_t i = 0u; i < geometry->vertices.size(); i += 3u)
            {
                if (!addTriangle(i, i + 1u, i + 2u))
                    return false;
            }
        }

        if (!shape.contours.empty())
            build.shapes.emplace_back(std::move(shape));
        return true;
    }

    bool addLine(
        entt::registry& registry,
        entt::entity owner,
        const Overlay& overlay,
        const SRS& worldSRS,
        const Line& line,
        std::uint32_t fallbackSize,
        OverlayBuild& build)
    {
        const auto* geometry = resolveComponent<LineGeometry>(registry, line.geometry, owner);
        if (!geometry)
        {
            build.error = "Slug Line has no LineGeometry";
            return false;
        }

        const auto* style = resolveComponent<LineStyle>(registry, line.style, owner);
        const LineStyle defaultStyle;
        const auto& resolvedStyle = style ? *style : defaultStyle;
        const bool metricWidth = resolvedStyle.widthUnits.isDistance();
        if (!metricWidth && !resolvedStyle.widthUnits.isScreenSize())
        {
            build.error = "Slug LineStyle width units must be screen or distance units";
            return false;
        }

        SlugPointMapper mapper;
        if (!makeSlugPointMapper(
            geometry->srs,
            registry.try_get<Transform>(owner),
            worldSRS,
            mapper,
            build.error))
            return false;

        if (!isFinite(resolvedStyle.color))
        {
            build.error = "Slug LineStyle color must be finite";
            return false;
        }
        if (!std::isfinite(resolvedStyle.outlineWidth))
        {
            build.error = "Slug LineStyle outline width must be finite";
            return false;
        }
        if (resolvedStyle.outlineWidth > 0.0f &&
            !isFinite(resolvedStyle.outlineColor))
        {
            build.error = "Slug LineStyle outline color must be finite";
            return false;
        }

        SlugShapeInput shape;
        shape.owner = static_cast<std::uint32_t>(entt::to_integral(owner));
        shape.kind = SlugShapeKind::Stroke;
        shape.color = toArray(resolvedStyle.color);
        SlugMetricSpace metricSpace;
        if (metricWidth)
        {
            if (!makeSlugMetricSpace(
                registry.try_get<Transform>(owner),
                worldSRS,
                metricSpace,
                build.error))
                return false;

            shape.uvToEmX = metricSpace.uvToEmX;
            shape.uvToEmY = metricSpace.uvToEmY;
            shape.strokeWidth = static_cast<float>(
                Distance(resolvedStyle.width, resolvedStyle.widthUnits).as(Units::METERS) /
                metricSpace.referenceMeters);
            shape.outlineWidth = static_cast<float>(
                Distance(
                    std::max(0.0f, resolvedStyle.outlineWidth),
                    resolvedStyle.widthUnits).as(Units::METERS) /
                metricSpace.referenceMeters);
        }
        else
        {
            shape.uvToEmX = mapper.uvToEmX;
            shape.uvToEmY = mapper.uvToEmY;
            shape.strokeWidth = normalizedPixelWidth(
                overlay, resolvedStyle.width, fallbackSize);
            shape.outlineWidth = normalizedPixelWidth(
                overlay, std::max(0.0f, resolvedStyle.outlineWidth), fallbackSize);
        }
        shape.outlineColor = toArray(resolvedStyle.outlineColor);

        if (shape.strokeWidth <= 0.0f || !std::isfinite(shape.strokeWidth))
        {
            build.error = "Slug LineStyle width must be positive and finite";
            return false;
        }

        if (geometry->topology == LineTopology::Strip)
        {
            if (geometry->points.size() >= 2u)
            {
                SlugContourInput contour;
                contour.points.reserve(geometry->points.size());
                for (const auto& point : geometry->points)
                {
                    if (!isFiniteXY(point))
                    {
                        build.error = "Slug LineGeometry contains a non-finite XY coordinate";
                        return false;
                    }
                    SlugPointInput mapped;
                    if (!mapper.map(point, mapped))
                    {
                        build.error = "Slug LineGeometry coordinate transformation failed";
                        return false;
                    }
                    if (metricWidth)
                    {
                        SlugPointInput metric;
                        if (!metricSpace.map(mapped, metric))
                        {
                            build.error = "Slug LineGeometry metric mapping failed";
                            return false;
                        }
                        mapped = metric;
                    }
                    contour.points.emplace_back(mapped);
                }
                shape.contours.emplace_back(std::move(contour));
            }
        }
        else
        {
            for (std::size_t i = 0u; i + 1u < geometry->points.size(); i += 2u)
            {
                if (!isFiniteXY(geometry->points[i]) ||
                    !isFiniteXY(geometry->points[i + 1u]))
                {
                    build.error = "Slug LineGeometry contains a non-finite XY coordinate";
                    return false;
                }

                SlugPointInput p0, p1;
                if (!mapper.map(geometry->points[i], p0) ||
                    !mapper.map(geometry->points[i + 1u], p1))
                {
                    build.error = "Slug LineGeometry coordinate transformation failed";
                    return false;
                }
                if (metricWidth)
                {
                    SlugPointInput metric0, metric1;
                    if (!metricSpace.map(p0, metric0) ||
                        !metricSpace.map(p1, metric1))
                    {
                        build.error = "Slug LineGeometry metric mapping failed";
                        return false;
                    }
                    p0 = metric0;
                    p1 = metric1;
                }

                SlugContourInput contour;
                contour.points = { p0, p1 };
                shape.contours.emplace_back(std::move(contour));
            }
        }

        if (!shape.contours.empty())
            build.shapes.emplace_back(std::move(shape));
        return true;
    }

    bool addPoints(
        entt::registry& registry,
        entt::entity owner,
        const Overlay& overlay,
        const SRS& worldSRS,
        const Point& point,
        std::uint32_t fallbackSize,
        OverlayBuild& build)
    {
        const auto* geometry = resolveComponent<PointGeometry>(registry, point.geometry, owner);
        if (!geometry)
        {
            build.error = "Slug Point has no PointGeometry";
            return false;
        }

        SlugPointMapper mapper;
        if (!makeSlugPointMapper(
            geometry->srs,
            registry.try_get<Transform>(owner),
            worldSRS,
            mapper,
            build.error))
            return false;

        const auto* style = resolveComponent<PointStyle>(registry, point.style, owner);
        const PointStyle defaultStyle;
        const auto& resolvedStyle = style ? *style : defaultStyle;
        if (!isFinite(resolvedStyle.color))
        {
            build.error = "Slug PointStyle color must be finite";
            return false;
        }
        const float radius = 0.5f * normalizedPixelWidth(
            overlay, resolvedStyle.width, fallbackSize);

        if (radius <= 0.0f || !std::isfinite(radius))
        {
            build.error = "Slug PointStyle width must be positive and finite";
            return false;
        }

        SlugShapeInput shape;
        shape.owner = static_cast<std::uint32_t>(entt::to_integral(owner));
        shape.kind = SlugShapeKind::Circles;
        shape.color = toArray(resolvedStyle.color);
        shape.circles.reserve(geometry->points.size());
        for (const auto& p : geometry->points)
        {
            if (!isFiniteXY(p))
            {
                build.error = "Slug PointGeometry contains a non-finite XY coordinate";
                return false;
            }
            SlugPointInput center;
            if (!mapper.map(p, center))
            {
                build.error = "Slug PointGeometry coordinate transformation failed";
                return false;
            }
            shape.circles.emplace_back(SlugCircleInput{ center.x, center.y, radius });
        }

        if (!shape.circles.empty())
            build.shapes.emplace_back(std::move(shape));
        return true;
    }

    std::size_t computeSourceSignature(
        entt::registry& registry,
        entt::entity entity,
        const SRS& worldSRS)
    {
        std::size_t signature = 0u;
        hashValue(signature, worldSRS.definition());

        const auto& overlay = registry.get<Overlay>(entity);
        hashValue(signature, overlay.textureSize.x);
        hashValue(signature, overlay.textureSize.y);

        const auto* mesh = registry.try_get<Mesh>(entity);
        hashComponent(signature, mesh);
        if (mesh)
        {
            hashValue(signature, entt::to_integral(mesh->geometry));
            hashValue(signature, entt::to_integral(mesh->style));
            hashComponent(signature, resolveComponent<MeshGeometry>(registry, mesh->geometry, entity));
            hashComponent(signature, resolveComponent<MeshStyle>(registry, mesh->style, entity));
        }

        const auto* line = registry.try_get<Line>(entity);
        hashComponent(signature, line);
        if (line)
        {
            hashValue(signature, entt::to_integral(line->geometry));
            hashValue(signature, entt::to_integral(line->style));
            hashComponent(signature, resolveComponent<LineGeometry>(registry, line->geometry, entity));
            hashComponent(signature, resolveComponent<LineStyle>(registry, line->style, entity));
        }

        const auto* point = registry.try_get<Point>(entity);
        hashComponent(signature, point);
        if (point)
        {
            hashValue(signature, entt::to_integral(point->geometry));
            hashValue(signature, entt::to_integral(point->style));
            hashComponent(signature, resolveComponent<PointGeometry>(registry, point->geometry, entity));
            hashComponent(signature, resolveComponent<PointStyle>(registry, point->style, entity));
        }

        const bool georeferenced =
            (mesh && resolveComponent<MeshGeometry>(registry, mesh->geometry, entity) &&
                resolveComponent<MeshGeometry>(registry, mesh->geometry, entity)->srs.valid()) ||
            (line && resolveComponent<LineGeometry>(registry, line->geometry, entity) &&
                resolveComponent<LineGeometry>(registry, line->geometry, entity)->srs.valid()) ||
            (point && resolveComponent<PointGeometry>(registry, point->geometry, entity) &&
                resolveComponent<PointGeometry>(registry, point->geometry, entity)->srs.valid());

        const auto* lineStyle = line ?
            resolveComponent<LineStyle>(registry, line->style, entity) : nullptr;
        const bool metricLine = lineStyle && lineStyle->widthUnits.isDistance();

        // Georeferenced mapping depends on the fitted projector. Metric line
        // conversion does too, since localMatrix defines the decal's extent.
        if (georeferenced || metricLine)
        {
            const auto* transform = registry.try_get<Transform>(entity);
            hashValue(signature, transform != nullptr);
            if (transform)
                hashValue(signature, transform->revision);
        }
        return signature;
    }

    vsg::ref_ptr<vsg::Sampler> makeAtlasSampler()
    {
        auto sampler = vsg::Sampler::create();
        sampler->minFilter = VK_FILTER_NEAREST;
        sampler->magFilter = VK_FILTER_NEAREST;
        sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler->minLod = 0.0f;
        sampler->maxLod = 0.0f;
        return sampler;
    }

    bool makeAtlasImages(
        const SlugAtlasOutput& atlas,
        vsg::ref_ptr<vsg::Sampler> sampler,
        vsg::ref_ptr<vsg::ImageInfo>& curveImage,
        vsg::ref_ptr<vsg::ImageInfo>& bandImage,
        std::string& error)
    {
        if (atlas.curveTexture.format != SlugTextureFormat::RGBA32F ||
            atlas.bandTexture.format != SlugTextureFormat::RGBA16UI)
        {
            error = "Slughorn returned unsupported texture formats";
            return false;
        }

        const auto curveBytes = static_cast<std::uint64_t>(atlas.curveTexture.width) *
            atlas.curveTexture.height * sizeof(vsg::vec4);
        const auto bandBytes = static_cast<std::uint64_t>(atlas.bandTexture.width) *
            atlas.bandTexture.height * sizeof(vsg::usvec4);

        if (atlas.curveTexture.width == 0u || atlas.curveTexture.height == 0u ||
            atlas.bandTexture.width == 0u || atlas.bandTexture.height == 0u ||
            curveBytes != atlas.curveTexture.bytes.size() ||
            bandBytes != atlas.bandTexture.bytes.size())
        {
            error = "Slughorn returned invalid atlas texture dimensions";
            return false;
        }

        auto curveData = vsg::vec4Array2D::create(
            atlas.curveTexture.width,
            atlas.curveTexture.height,
            vsg::Data::Properties{ VK_FORMAT_R32G32B32A32_SFLOAT });
        auto bandData = vsg::usvec4Array2D::create(
            atlas.bandTexture.width,
            atlas.bandTexture.height,
            vsg::Data::Properties{ VK_FORMAT_R16G16B16A16_UINT });

        std::memcpy(
            curveData->dataPointer(),
            atlas.curveTexture.bytes.data(),
            atlas.curveTexture.bytes.size());
        std::memcpy(
            bandData->dataPointer(),
            atlas.bandTexture.bytes.data(),
            atlas.bandTexture.bytes.size());

        curveImage = vsg::ImageInfo::create(
            sampler, curveData, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        bandImage = vsg::ImageInfo::create(
            sampler, bandData, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        return true;
    }
}

SlugSystemNode::SlugSystemNode(Registry& registry) :
    Inherit(registry)
{
    _registry.write([&](entt::registry& r)
    {
        r.on_destroy<SlugResource>().connect<&SlugSystemNode::on_destroy_SlugResource>(*this);
    });
}

void SlugSystemNode::on_destroy_SlugResource(entt::registry& registry, entt::entity entity)
{
    const auto& resource = registry.get<SlugResource>(entity);
    if (resource.curveTexture || resource.bandTexture)
    {
        std::scoped_lock lock(_pendingDisposalsMutex);
        if (resource.curveTexture)
            _pendingDisposals.emplace_back(resource.curveTexture);
        if (resource.bandTexture)
            _pendingDisposals.emplace_back(resource.bandTexture);
    }
}

void SlugSystemNode::update(VSGContext vsgcontext)
{
    if (!vsgcontext)
        return;

    // Component destruction callbacks cannot touch the VSG lifecycle. Drain
    // their retained references here, before building or publishing resources.
    {
        std::vector<vsg::ref_ptr<vsg::ImageInfo>> pending;
        {
            std::scoped_lock lock(_pendingDisposalsMutex);
            pending.swap(_pendingDisposals);
        }
        for (auto& image : pending)
            dispose(image);
    }

    if (status.failed())
    {
        Inherit::update(vsgcontext);
        return;
    }

    if (!_atlasSampler)
        _atlasSampler = makeAtlasSampler();

    std::vector<OverlayBuild> builds;
    _registry.read([&](entt::registry& registry)
    {
        std::vector<entt::entity> entities;
        registry.view<Overlay>().each([&](auto entity, const auto& overlay)
        {
            if (overlay.technique == OverlayTechnique::Slug)
                entities.emplace_back(entity);
        });
        std::sort(entities.begin(), entities.end(), [](auto lhs, auto rhs)
        {
            return entt::to_integral(lhs) < entt::to_integral(rhs);
        });

        builds.reserve(entities.size());
        for (auto entity : entities)
        {
            const auto& overlay = registry.get<Overlay>(entity);
            OverlayBuild build;
            build.entity = entity;
            build.sourceSignature = computeSourceSignature(registry, entity, worldSRS);

            if (const auto* resource = registry.try_get<SlugResource>(entity))
            {
                build.exportPath = resource->exportPath;
                if (resource->sourceSignatureValid &&
                    resource->sourceSignature == build.sourceSignature)
                {
                    if (build.exportPath.empty())
                        continue;
                    build.exportOnly = true;
                }
            }
            bool foundPrimitive = false;
            if (const auto* mesh = registry.try_get<Mesh>(entity))
            {
                foundPrimitive = true;
                addMesh(registry, entity, worldSRS, *mesh, build);
            }
            if (build.error.empty())
            {
                if (const auto* line = registry.try_get<Line>(entity))
                {
                    foundPrimitive = true;
                    addLine(registry, entity, overlay, worldSRS, *line, textureWidth, build);
                }
            }
            if (build.error.empty())
            {
                if (const auto* point = registry.try_get<Point>(entity))
                {
                    foundPrimitive = true;
                    addPoints(registry, entity, overlay, worldSRS, *point, textureWidth, build);
                }
            }

            if (build.error.empty() && build.shapes.empty())
            {
                build.error = foundPrimitive ?
                    "Slug overlay geometry contains no renderable primitives" :
                    "Slug overlay requires a same-entity Mesh, Line, or Point";
            }

            builds.emplace_back(std::move(build));
        }
    });

    // Build each changed payload independently. New paged overlays therefore do
    // not repack or invalidate any atlas that is already resident.
    for (auto& build : builds)
    {
        if (!build.error.empty())
        {
            if (!build.exportPath.empty())
            {
                build.exportAttempted = true;
                build.exportMessage = build.error;
            }
            continue;
        }

        SlugAtlasInput input;
        // Always begin at the baseline width. Slughorn grows transactionally
        // when necessary; restarting here lets a simplified overlay shrink
        // instead of retaining its historical maximum allocation.
        input.textureWidth = textureWidth;
        input.shapes = std::move(build.shapes);
        input.exportPath = build.exportPath;

        SlugAtlasOutput atlas;
        if (!rocky::detail::buildSlugAtlas(input, atlas, build.error))
        {
            if (!build.exportPath.empty())
            {
                build.exportAttempted = true;
                build.exportMessage = build.error;
            }
            continue;
        }

        build.exportAttempted = atlas.exportAttempted;
        build.exportSucceeded = atlas.exportSucceeded;
        build.exportMessage = std::move(atlas.exportMessage);

        // An export of an unchanged source rebuilt the CPU atlas solely so the
        // SDK could serialize it. Keep the currently published textures and
        // descriptor state intact.
        if (build.exportOnly)
            continue;

        if (!makeAtlasImages(
            atlas, _atlasSampler,
            build.curveTexture, build.bandTexture, build.error))
            continue;

        build.textureWidthLog2 = atlas.textureWidthLog2;
        build.indirectionSize = atlas.indirectionSize;
        for (const auto& source : atlas.layers)
        {
            SlugLayerResource layer;
            layer.isOutline = source.isOutline;
            layer.color = Color(
                source.color[0], source.color[1], source.color[2], source.color[3]);
            layer.uvToEmX = glm::fvec4(
                source.uvToEmX[0], source.uvToEmX[1],
                source.uvToEmX[2], source.uvToEmX[3]);
            layer.uvToEmY = glm::fvec4(
                source.uvToEmY[0], source.uvToEmY[1],
                source.uvToEmY[2], source.uvToEmY[3]);
            layer.bandTransform = glm::fvec4(
                source.bandTransform[0], source.bandTransform[1],
                source.bandTransform[2], source.bandTransform[3]);
            layer.shapeData = glm::uvec4(
                source.shapeData[0], source.shapeData[1],
                source.shapeData[2], source.shapeData[3]);
            build.layers.emplace_back(std::move(layer));
        }

        if (build.layers.empty())
        {
            build.error = "Slug atlas did not publish this overlay's layers";
            build.curveTexture = {};
            build.bandTexture = {};
            continue;
        }

        // Do not compile here. DecalSystem compiles the pair only after this
        // payload is visible and has acquired a bounded descriptor slot.
    }

    std::vector<vsg::ref_ptr<vsg::ImageInfo>> oldImages;
    bool resourcesChanged = false;
    _registry.write([&](entt::registry& registry)
    {
        std::vector<entt::entity> stale;
        registry.view<SlugResource>().each([&](auto entity, auto& resource)
        {
            const auto* overlay = registry.try_get<Overlay>(entity);
            if (!overlay || overlay->technique != OverlayTechnique::Slug)
            {
                stale.emplace_back(entity);
            }
        });
        for (auto entity : stale)
        {
            registry.remove<SlugResource>(entity);
            resourcesChanged = true;
        }

        for (auto& build : builds)
        {
            if (!registry.valid(build.entity))
            {
                if (build.curveTexture)
                    oldImages.emplace_back(build.curveTexture);
                if (build.bandTexture)
                    oldImages.emplace_back(build.bandTexture);
                continue;
            }

            auto& resource = registry.get_or_emplace<SlugResource>(build.entity);

            if (build.exportAttempted)
            {
                // Preserve a newer request in the unlikely event one arrived
                // after this build took its read snapshot.
                if (resource.exportPath == build.exportPath)
                    resource.exportPath.clear();
                resource.exportSucceeded = build.exportSucceeded;
                resource.exportMessage = build.exportMessage;
                resourcesChanged = true;
            }

            if (build.exportOnly)
                continue;

            if (resource.curveTexture)
                oldImages.emplace_back(resource.curveTexture);
            if (resource.bandTexture)
                oldImages.emplace_back(resource.bandTexture);

            resource.owner = build.entity;
            resource.curveTexture = build.curveTexture;
            resource.bandTexture = build.bandTexture;
            resource.layers = std::move(build.layers);
            resource.textureWidthLog2 = build.textureWidthLog2;
            resource.indirectionSize = build.indirectionSize;
            resource.sourceSignature = build.sourceSignature;
            resource.sourceSignatureValid = true;
            ++resource.atlasGeneration;
            ++resource.revision;
            resource.ready = build.error.empty() &&
                resource.curveTexture && resource.bandTexture && !resource.layers.empty();
            resource.message = resource.ready ? std::string() : build.error;
            resourcesChanged = true;
        }
    });

    for (auto& image : oldImages)
        dispose(image);

    for (const auto& build : builds)
    {
        if (!build.error.empty())
            Log()->warn("SlugSystemNode: {}", build.error);
        else if (build.exportAttempted)
        {
            if (build.exportSucceeded)
                Log()->info("SlugSystemNode: {}", build.exportMessage);
            else
                Log()->warn("SlugSystemNode: {}", build.exportMessage);
        }
    }

    if (resourcesChanged)
        vsgcontext->requestFrame();
    Inherit::update(vsgcontext);
}
