/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "OverlayBakeSystem.h"
#include "OverlayRenderContext.h"
#include "../RTT.h"
#include <rocky/vsg/VSGUtils.h>
#include <rocky/ecs/Decal.h>
#include <rocky/ecs/Mesh.h>
#include <rocky/ecs/Line.h>
#include <rocky/ecs/Point.h>
#include <rocky/ecs/Model.h>
#include "ModelSystem.h"
#include "ECSTypes.h"
#include <algorithm>
#include <cfloat>
#include <cmath>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::detail;

namespace
{
    inline glm::uvec2 resolveTextureSize(const Overlay& overlay, unsigned fallback)
    {
        glm::uvec2 size = overlay.textureSize;
        if (size.x == 0u) size.x = fallback;
        if (size.y == 0u) size.y = fallback;
        return size;
    }

    struct OverlayBakeViewNode : public vsg::Inherit<vsg::Node, OverlayBakeViewNode>
    {
        vsg::ref_ptr<vsg::View> view;
        vsg::ref_ptr<vsg::Camera> camera;

        SRS worldSRS;
        entt::entity target = entt::null;

        vsg::dvec3 eye = { 0.0, 0.0, 1.0 };
        vsg::dvec3 center = { 0.0, 0.0, 0.0 };
        vsg::dvec3 up = { 0.0, 1.0, 0.0 };
        double left = -1.0;
        double right = 1.0;
        double bottom = -1.0;
        double top = 1.0;
        double znear = 0.01;
        double zfar = 1000.0;
        std::uint32_t viewportWidth = 512u;
        std::uint32_t viewportHeight = 512u;

        void traverse(vsg::RecordTraversal& record) const override
        {
            RenderDomain prevDomain = RenderDomain::Main;
            bool hadPrevDomain = record.getValue(RENDER_DOMAIN_KEY, prevDomain);

            entt::entity prevTarget = entt::null;
            bool hadPrevTarget = record.getValue(OVERLAY_BAKE_TARGET_KEY, prevTarget);

            SRS prevWorldSRS;
            bool hadPrevWorldSRS = record.getValue("rocky.worldsrs", prevWorldSRS);

            RenderDomain domain = RenderDomain::OverlayBake;
            record.setValue(RENDER_DOMAIN_KEY, domain);
            record.setValue(OVERLAY_BAKE_TARGET_KEY, target);
            record.setValue("rocky.worldsrs", worldSRS);

            if (camera)
            {
                camera->viewMatrix = vsg::LookAt::create(eye, center, up);
                camera->projectionMatrix = vsg::Orthographic::create(left, right, bottom, top, znear, zfar);
                camera->viewportState = vsg::ViewportState::create(VkExtent2D{ viewportWidth, viewportHeight });
            }

            if (view)
                view->accept(record);

            record.setValue(RENDER_DOMAIN_KEY, hadPrevDomain ? prevDomain : RenderDomain::Main);
            record.setValue(OVERLAY_BAKE_TARGET_KEY, hadPrevTarget ? prevTarget : entt::null);
            if (hadPrevWorldSRS)
                record.setValue("rocky.worldsrs", prevWorldSRS);
        }

        void traverse(vsg::Visitor& visitor) override
        {
            visitor.setValue("rocky.worldsrs", worldSRS);
            if (view)
                view->accept(visitor);
        }

        void traverse(vsg::ConstVisitor& visitor) const override
        {
            visitor.setValue("rocky.worldsrs", worldSRS);
            if (view)
                view->accept(visitor);
        }
    };

    glm::dmat4 makeWorldMatrix(const Transform& xform, const SRS& worldSRS)
    {
        auto pos = xform.position.transform(worldSRS);
        if (!pos.valid())
            return glm::dmat4(1.0);

        if (xform.topocentric)
        {
            return worldSRS.topocentricToWorldMatrix(glm::dvec3(pos.x, pos.y, pos.z)) * xform.localMatrix;
        }
        else
        {
            return glm::translate(glm::dmat4(1.0), glm::dvec3(pos.x, pos.y, pos.z)) * xform.localMatrix;
        }
    }

    struct OverlayBounds
    {
        bool hasBounds = false;
        SRS srs;
        double referenceLongitude = 0.0;
        double minx = DBL_MAX, miny = DBL_MAX, minz = DBL_MAX;
        double maxx = -DBL_MAX, maxy = -DBL_MAX, maxz = -DBL_MAX;

        void expand(const SRS& inSRS, const glm::dvec3& input)
        {
            if (!inSRS.valid())
                return;

            if (!srs.valid())
                srs = inSRS;

            auto point = GeoPoint(inSRS, input.x, input.y, input.z).transform(srs);
            if (!point.valid())
                return;

            double x = point.x;
            if (srs.isGeodetic())
            {
                if (!hasBounds)
                    referenceLongitude = x;

                // Keep longitudes contiguous around the first sample so an
                // overlay crossing the antimeridian does not center at Greenwich.
                while (x - referenceLongitude > 180.0) x -= 360.0;
                while (x - referenceLongitude < -180.0) x += 360.0;
            }

            minx = std::min(minx, x); miny = std::min(miny, point.y); minz = std::min(minz, point.z);
            maxx = std::max(maxx, x); maxy = std::max(maxy, point.y); maxz = std::max(maxz, point.z);
            hasBounds = true;
        }
    };

    Transform* ensureOverlayTransform(
        entt::registry& reg,
        entt::entity e_overlay,
        bool forceRecompute,
        float depthSafetyFactor,
        const SRS& worldSRS,
        const glm::uvec2& textureSize)
    {
        auto* existing = reg.try_get<Transform>(e_overlay);
        bool autoManaged = reg.any_of<AutoOverlayTransform>(e_overlay);
        if (existing && !autoManaged)
            return existing;
        if (existing && autoManaged && !forceRecompute)
            return existing;

        OverlayBounds bounds;
        double paddingPixels = 2.0; // filtering/antialiasing guard band

        if (reg.all_of<Mesh>(e_overlay))
        {
            auto& mesh = reg.get<Mesh>(e_overlay);
            if (auto* geom = reg.try_get<MeshGeometry>(mesh.geometry))
            {
                for (auto& v : geom->vertices)
                    bounds.expand(geom->srs, v);
            }
        }

        if (reg.all_of<Line>(e_overlay))
        {
            auto& line = reg.get<Line>(e_overlay);
            if (auto* geom = reg.try_get<LineGeometry>(line.geometry))
            {
                for (auto& v : geom->points)
                    bounds.expand(geom->srs, v);
            }

            double width = 2.0;
            if (auto* style = reg.try_get<LineStyle>(line.style))
                width = std::abs((double)style->width);
            paddingPixels = std::max(paddingPixels, 0.5 * width + 2.0);
        }

        if (reg.all_of<Point>(e_overlay))
        {
            auto& point = reg.get<Point>(e_overlay);
            if (auto* geom = reg.try_get<PointGeometry>(point.geometry))
            {
                for (auto& v : geom->points)
                    bounds.expand(geom->srs, v);

                if (auto* style = reg.try_get<PointStyle>(point.style); style && style->useGeometryWidths)
                    for (auto width : geom->widths)
                        paddingPixels = std::max(paddingPixels, 0.5 * std::abs((double)width) + 2.0);
            }

            double width = 3.0;
            if (auto* style = reg.try_get<PointStyle>(point.style))
                width = std::abs((double)style->width);
            paddingPixels = std::max(paddingPixels, 0.5 * width + 2.0);
        }

        // A model without a Transform is already interpreted in world coordinates
        // by ModelSystem, so use its node bounds in that same SRS. Models with an
        // explicit Transform use that user-supplied projection volume above.
        if (!bounds.hasBounds && reg.all_of<Model, detail::ModelDetail>(e_overlay) && worldSRS.valid())
        {
            auto& modelDetail = reg.get<detail::ModelDetail>(e_overlay);
            if (modelDetail.node)
            {
                vsg::ComputeBounds cb;
                modelDetail.node->accept(cb);
                if (cb.bounds)
                {
                    const auto& b = cb.bounds;
                    for (int ix = 0; ix < 2; ++ix)
                        for (int iy = 0; iy < 2; ++iy)
                            for (int iz = 0; iz < 2; ++iz)
                                bounds.expand(worldSRS, glm::dvec3(
                                    ix ? b.max.x : b.min.x,
                                    iy ? b.max.y : b.min.y,
                                    iz ? b.max.z : b.min.z));
                }
            }
        }

        if (!bounds.hasBounds || !bounds.srs.valid())
            return existing;

        Transform* xform = existing;
        if (!xform)
        {
            xform = &reg.emplace<Transform>(e_overlay);
            (void)reg.get_or_emplace<AutoOverlayTransform>(e_overlay);
        }
        double centerX = (bounds.minx + bounds.maxx) * 0.5;
        if (bounds.srs.isGeodetic())
            centerX = std::remainder(centerX, 360.0);
        xform->position = GeoPoint(bounds.srs, centerX,
            (bounds.miny + bounds.maxy) * 0.5,
            (bounds.minz + bounds.maxz) * 0.5);

        if (bounds.srs.isGeodetic())
        {
            xform->topocentric = true;

            // Use the largest span across multiple sample lat/lon slices.
            // A single centroid-latitude sample can underestimate width for
            // tall geodetic extents, clipping one side (commonly east).
            auto eastMetersMid = GeoPoint(bounds.srs, bounds.minx, xform->position.y, 0.0)
                .geodesicDistanceTo(GeoPoint(bounds.srs, bounds.maxx, xform->position.y, 0.0))
                .as(Units::METERS);
            auto eastMetersSouth = GeoPoint(bounds.srs, bounds.minx, bounds.miny, 0.0)
                .geodesicDistanceTo(GeoPoint(bounds.srs, bounds.maxx, bounds.miny, 0.0))
                .as(Units::METERS);
            auto eastMetersNorth = GeoPoint(bounds.srs, bounds.minx, bounds.maxy, 0.0)
                .geodesicDistanceTo(GeoPoint(bounds.srs, bounds.maxx, bounds.maxy, 0.0))
                .as(Units::METERS);
            auto eastMeters = std::max(eastMetersMid, std::max(eastMetersSouth, eastMetersNorth));

            auto northMetersMid = GeoPoint(bounds.srs, xform->position.x, bounds.miny, 0.0)
                .geodesicDistanceTo(GeoPoint(bounds.srs, xform->position.x, bounds.maxy, 0.0))
                .as(Units::METERS);
            auto northMetersWest = GeoPoint(bounds.srs, bounds.minx, bounds.miny, 0.0)
                .geodesicDistanceTo(GeoPoint(bounds.srs, bounds.minx, bounds.maxy, 0.0))
                .as(Units::METERS);
            auto northMetersEast = GeoPoint(bounds.srs, bounds.maxx, bounds.miny, 0.0)
                .geodesicDistanceTo(GeoPoint(bounds.srs, bounds.maxx, bounds.maxy, 0.0))
                .as(Units::METERS);
            auto northMeters = std::max(northMetersMid, std::max(northMetersWest, northMetersEast));

            eastMeters = std::max(1.0, eastMeters);
            northMeters = std::max(1.0, northMeters);
            eastMeters += 2.0 * eastMeters * paddingPixels / std::max(1u, textureSize.x);
            northMeters += 2.0 * northMeters * paddingPixels / std::max(1u, textureSize.y);

            // Choose the minimum practical thickness for the orthographic projector volume.
            // It must cover earth-curvature drop from the tangent frame center to the overlay
            // edges, plus any source Z range. Keeping this tight reduces decal culling load.
            const double verticalRange = std::max(1.0, bounds.maxz - bounds.minz);
            const double verticalHalf = 0.5 * verticalRange;

            const double halfDiagonalMeters = 0.5 * std::sqrt(eastMeters * eastMeters + northMeters * northMeters);
            const double R = bounds.srs.ellipsoid().semiMajorAxis();

            double curvatureDrop = 0.0;
            if (R > 0.0 && halfDiagonalMeters > 0.0)
            {
                double hh = std::min(halfDiagonalMeters, R - 1.0);
                curvatureDrop = R - std::sqrt(std::max(0.0, R * R - hh * hh));
            }

            // Adaptive terrain-relief floor based on footprint size.
            // Keeps small overlays tight and large overlays robust.
            const double diagonalMeters = std::sqrt(eastMeters * eastMeters + northMeters * northMeters);
            const double terrainReliefFloor =
                std::clamp(0.03 * diagonalMeters, 500.0, 4000.0);
            const double margin = 25.0; // meters
            const double requiredHalfDepth =
                curvatureDrop +
                verticalHalf +
                (0.5 * terrainReliefFloor) +
                margin;
            const double minThickness = terrainReliefFloor + (2.0 * margin);
            const double autoThickness = std::max(minThickness, std::max(verticalRange + 1.0, requiredHalfDepth * 2.0));
            const double safety = std::max(0.1, (double)depthSafetyFactor);
            const double thickness = std::max(verticalRange + 1.0, autoThickness * safety);

            xform->localMatrix = glm::scale(
                glm::dmat4(1.0),
                glm::dvec3(
                    eastMeters,
                    northMeters,
                    thickness));
        }
        else
        {
            xform->topocentric = false;
            double width = std::max(1.0, bounds.maxx - bounds.minx);
            double height = std::max(1.0, bounds.maxy - bounds.miny);
            width += 2.0 * width * paddingPixels / std::max(1u, textureSize.x);
            height += 2.0 * height * paddingPixels / std::max(1u, textureSize.y);
            const double verticalRange = std::max(1.0, bounds.maxz - bounds.minz);
            const double diagonalMeters = std::sqrt(width * width + height * height);
            const double terrainReliefFloor =
                std::clamp(0.03 * diagonalMeters, 500.0, 4000.0);
            const double safety = std::max(0.1, (double)depthSafetyFactor);
            xform->localMatrix = glm::scale(
                glm::dmat4(1.0),
                glm::dvec3(
                    width,
                    height,
                    std::max(verticalRange + 1.0, terrainReliefFloor * safety)));
        }

        xform->dirty(reg);
        return xform;
    }

    inline std::size_t hashValue(std::size_t seed, std::size_t value)
    {
        seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        return seed;
    }

    template<typename VEC>
    inline std::size_t hashVec3(std::size_t seed, const VEC& v)
    {
        auto hx = std::hash<double>{}(v.x);
        auto hy = std::hash<double>{}(v.y);
        auto hz = std::hash<double>{}(v.z);
        seed = hashValue(seed, hx);
        seed = hashValue(seed, hy);
        seed = hashValue(seed, hz);
        return seed;
    }

    template<typename VEC>
    inline std::size_t hashVec2(std::size_t seed, const VEC& v)
    {
        seed = hashValue(seed, std::hash<double>{}((double)v.x));
        seed = hashValue(seed, std::hash<double>{}((double)v.y));
        return seed;
    }

    template<typename VEC>
    inline std::size_t hashVec4(std::size_t seed, const VEC& v)
    {
        seed = hashValue(seed, std::hash<double>{}((double)v.x));
        seed = hashValue(seed, std::hash<double>{}((double)v.y));
        seed = hashValue(seed, std::hash<double>{}((double)v.z));
        seed = hashValue(seed, std::hash<double>{}((double)v.w));
        return seed;
    }

    inline std::size_t hashEntity(std::size_t seed, entt::entity value)
    {
        return hashValue(seed, std::hash<std::underlying_type_t<entt::entity>>{}(entt::to_integral(value)));
    }

    inline std::size_t hashMatrix(std::size_t seed, const glm::dmat4& matrix)
    {
        for (int column = 0; column < 4; ++column)
            for (int row = 0; row < 4; ++row)
                seed = hashValue(seed, std::hash<double>{}(matrix[column][row]));
        return seed;
    }

    std::size_t computeOverlayGeometryStamp(entt::registry& reg, entt::entity e_overlay)
    {
        std::size_t seed = 0u;

        if (reg.all_of<Mesh>(e_overlay))
        {
            auto& mesh = reg.get<Mesh>(e_overlay);
            seed = hashEntity(seed, mesh.geometry);
            seed = hashEntity(seed, mesh.style);
            if (auto* geom = reg.try_get<MeshGeometry>(mesh.geometry))
            {
                seed = hashValue(seed, std::hash<std::string>{}(geom->srs.definition()));
                seed = hashValue(seed, geom->vertices.size());
                for (auto& v : geom->vertices) seed = hashVec3(seed, v);
                seed = hashValue(seed, geom->colors.size());
                for (auto& v : geom->colors) seed = hashVec4(seed, v);
                seed = hashValue(seed, geom->normals.size());
                for (auto& v : geom->normals) seed = hashVec3(seed, v);
                seed = hashValue(seed, geom->uvs.size());
                for (auto& v : geom->uvs) seed = hashVec2(seed, v);
                seed = hashValue(seed, geom->indices.size());
                for (auto v : geom->indices) seed = hashValue(seed, std::hash<std::uint32_t>{}(v));
            }
        }

        if (reg.all_of<Line>(e_overlay))
        {
            auto& line = reg.get<Line>(e_overlay);
            seed = hashEntity(seed, line.geometry);
            seed = hashEntity(seed, line.style);
            if (auto* geom = reg.try_get<LineGeometry>(line.geometry))
            {
                seed = hashValue(seed, std::hash<std::string>{}(geom->srs.definition()));
                seed = hashValue(seed, std::hash<int>{}((int)geom->topology));
                seed = hashValue(seed, geom->points.size());
                for (auto& v : geom->points) seed = hashVec3(seed, v);
                seed = hashValue(seed, geom->colors.size());
                for (auto& v : geom->colors) seed = hashVec4(seed, v);
            }
            if (auto* style = reg.try_get<LineStyle>(line.style))
                seed = hashValue(seed, std::hash<float>{}(style->width));
        }

        if (reg.all_of<Point>(e_overlay))
        {
            auto& point = reg.get<Point>(e_overlay);
            seed = hashEntity(seed, point.geometry);
            seed = hashEntity(seed, point.style);
            if (auto* geom = reg.try_get<PointGeometry>(point.geometry))
            {
                seed = hashValue(seed, std::hash<std::string>{}(geom->srs.definition()));
                seed = hashValue(seed, geom->points.size());
                for (auto& v : geom->points) seed = hashVec3(seed, v);
                seed = hashValue(seed, geom->colors.size());
                for (auto& v : geom->colors) seed = hashVec4(seed, v);
                seed = hashValue(seed, geom->widths.size());
                for (auto v : geom->widths) seed = hashValue(seed, std::hash<float>{}(v));
            }
            if (auto* style = reg.try_get<PointStyle>(point.style))
            {
                seed = hashValue(seed, std::hash<float>{}(style->width));
                seed = hashValue(seed, std::hash<bool>{}(style->useGeometryWidths));
            }
        }

        if (auto* model = reg.try_get<Model>(e_overlay))
        {
            seed = hashValue(seed, std::hash<std::string>{}(model->uri.full()));
            seed = hashValue(seed, std::hash<bool>{}(model->localMatrix.has_value()));
            if (model->localMatrix)
                seed = hashMatrix(seed, *model->localMatrix);
            if (auto* detail = reg.try_get<detail::ModelDetail>(e_overlay))
                seed = hashValue(seed, std::hash<const void*>{}(detail->node.get()));
        }

        return seed;
    }

    std::size_t computeOverlayContentStamp(entt::registry& reg, entt::entity e_overlay, std::size_t geometryStamp)
    {
        std::size_t seed = hashValue(0u, geometryStamp);

        if (auto* mesh = reg.try_get<Mesh>(e_overlay))
        {
            if (auto* style = reg.try_get<MeshStyle>(mesh->style))
            {
                seed = hashVec4(seed, style->color);
                seed = hashValue(seed, std::hash<bool>{}(style->useGeometryColors));
                seed = hashEntity(seed, style->texture);
                seed = hashValue(seed, std::hash<bool>{}(style->wireframe));
                seed = hashValue(seed, std::hash<bool>{}(style->lighting));
                seed = hashValue(seed, std::hash<std::uint32_t>{}(style->stipplePattern));
                seed = hashValue(seed, std::hash<bool>{}(style->writeDepth));
                seed = hashValue(seed, std::hash<bool>{}(style->drawBackfaces));
                seed = hashValue(seed, std::hash<bool>{}(style->twoPassAlpha));
                seed = hashValue(seed, std::hash<bool>{}(style->transparencyBin));
                if (auto* texture = reg.try_get<MeshTexture>(style->texture))
                    seed = hashValue(seed, std::hash<const void*>{}(texture->imageInfo.get()));
            }
        }

        if (auto* line = reg.try_get<Line>(e_overlay))
        {
            if (auto* style = reg.try_get<LineStyle>(line->style))
            {
                seed = hashVec4(seed, style->color);
                seed = hashValue(seed, std::hash<float>{}(style->width));
                seed = hashValue(seed, std::hash<std::uint16_t>{}(style->stipplePattern));
                seed = hashValue(seed, std::hash<int>{}(style->stippleFactor));
                seed = hashValue(seed, std::hash<float>{}(style->resolution));
                seed = hashValue(seed, std::hash<bool>{}(style->useGeometryColors));
                seed = hashValue(seed, std::hash<bool>{}(style->transparencyBin));
            }
        }

        if (auto* point = reg.try_get<Point>(e_overlay))
        {
            if (auto* style = reg.try_get<PointStyle>(point->style))
            {
                seed = hashVec4(seed, style->color);
                seed = hashValue(seed, std::hash<float>{}(style->width));
                seed = hashValue(seed, std::hash<float>{}(style->antialias));
                seed = hashValue(seed, std::hash<bool>{}(style->useGeometryColors));
                seed = hashValue(seed, std::hash<bool>{}(style->useGeometryWidths));
                seed = hashValue(seed, std::hash<bool>{}(style->transparencyBin));
            }
        }

        return seed;
    }
}

OverlayBakeSystemNode::OverlayBakeSystemNode(Registry& registry) :
    Inherit(registry)
{
    _registry.write([&](entt::registry& r)
        {
            r.on_construct<Overlay>().connect<&OverlayBakeSystemNode::on_construct_Overlay>(*this);
            r.on_update<Overlay>().connect<&OverlayBakeSystemNode::on_update_Overlay>(*this);
            r.on_destroy<Overlay>().connect<&OverlayBakeSystemNode::on_destroy_Overlay>(*this);
            r.on_destroy<OverlayBakeDetail>().connect<&OverlayBakeSystemNode::on_destroy_OverlayBakeDetail>(*this);
        });
}

void OverlayBakeSystemNode::initialize(VSGContext vsgcontext)
{
    for (std::size_t i = 0; i < _sharedCameras.size(); ++i)
    {
        if (!_sharedCameras[i])
        {
            auto extent = VkExtent2D{ textureSize, textureSize };
            _sharedCameras[i] = vsg::Camera::create(
                vsg::Orthographic::create(-1.0, 1.0, -1.0, 1.0, 0.01, 1000.0),
                vsg::LookAt::create(vsg::dvec3(0.0, 0.0, 1.0), vsg::dvec3(0.0, 0.0, 0.0), vsg::dvec3(0.0, 1.0, 0.0)),
                vsg::ViewportState::create(extent));
        }

        if (!_sharedViews[i] && bakeScene)
        {
            _sharedViews[i] = vsg::View::create(_sharedCameras[i], bakeScene);

            if (i == 0u)
            {
                auto noDepth = vsg::DepthStencilState::create();
                noDepth->depthTestEnable = VK_FALSE;
                noDepth->depthWriteEnable = VK_FALSE;
                _sharedViews[i]->overridePipelineStates.push_back(noDepth);
            }
        }
    }
}

void OverlayBakeSystemNode::on_construct_Overlay(entt::registry& r, entt::entity e)
{
    (void)r.get_or_emplace<ActiveState>(e);
    (void)r.get_or_emplace<Visibility>(e);
}

void OverlayBakeSystemNode::on_update_Overlay(entt::registry& r, entt::entity e)
{
    // nop
}

void OverlayBakeSystemNode::on_destroy_Overlay(entt::registry& r, entt::entity e)
{
    if (r.any_of<AutoOverlayTransform>(e))
    {
        r.remove<AutoOverlayTransform>(e);
        if (r.any_of<Transform>(e))
            r.remove<Transform>(e);
    }

    if (r.any_of<OverlayBakeTexture>(e))
        r.remove<OverlayBakeTexture>(e);

    r.remove<OverlayBakeDetail>(e);
}

void OverlayBakeSystemNode::on_destroy_OverlayBakeDetail(entt::registry& r, entt::entity e)
{
    auto& detail = r.get<OverlayBakeDetail>(e);

    if (detail.hostCommandGraph && detail.renderGraph)
    {
        auto& children = detail.hostCommandGraph->children;
        children.erase(std::remove(children.begin(), children.end(), detail.renderGraph), children.end());
    }

    dispose(detail.renderGraph);
    dispose(detail.texture);
}

bool OverlayBakeSystemNode::createBakeResources(
    VSGContext vsgcontext,
    const glm::uvec2& textureSize,
    bool useDepthBuffer,
    vsg::ref_ptr<vsg::ImageInfo>& outTexture,
    vsg::ref_ptr<vsg::RenderGraph>& outRenderGraph,
    vsg::ref_ptr<vsg::Node>& outViewNode,
    vsg::ref_ptr<vsg::CommandGraph>& outHostCommandGraph)
{
    if (!bakeScene)
        return false;

    const std::size_t mode = useDepthBuffer ? 1u : 0u;
    if (!_sharedCameras[mode] || !_sharedViews[mode])
        initialize(vsgcontext);

    if (!_sharedCameras[mode] || !_sharedViews[mode])
        return false;

    auto bakeNode = OverlayBakeViewNode::create();
    bakeNode->view = _sharedViews[mode];
    bakeNode->camera = _sharedCameras[mode];
    bakeNode->worldSRS = worldSRS;
    bakeNode->viewportWidth = textureSize.x;
    bakeNode->viewportHeight = textureSize.y;

    auto extent = VkExtent2D{ textureSize.x, textureSize.y };

    auto context = vsg::Context::create(vsgcontext->device());
    auto color = vsg::ImageInfo::create();
    vsg::ref_ptr<vsg::ImageInfo> depth;
    if (useDepthBuffer)
        depth = vsg::ImageInfo::create();
    auto rg = RTT::createOffScreenRenderGraph(*context, extent, color, depth, vsg::vec4(0.0f, 0.0f, 0.0f, 0.0f));
    rg->addChild(bakeNode);

    auto viewer = vsgcontext->viewer();
    if (!viewer)
        return false;

    bool installed = false;
    for (auto& task : viewer->recordAndSubmitTasks)
    {
        for (auto& cg : task->commandGraphs)
        {
            if (!cg || !cg->window)
                continue;

            outHostCommandGraph = cg;
            installed = true;
            break;
        }
        if (installed)
            break;
    }

    if (!installed)
        return false;

    // Compile now, but do not attach until the registry still owns the overlay
    // and its bake camera is valid. This prevents orphan command-graph children.
    vsgcontext->compileRenderGraph(rg, outHostCommandGraph->window);

    outTexture = color;
    outRenderGraph = rg;
    outViewNode = bakeNode;

    vsgcontext->requestFrame();
    return true;
}

bool OverlayBakeSystemNode::updateBakeCamera(entt::registry& r, entt::entity e_overlay, OverlayBakeDetail& detail, bool recomputeAutoTransform) const
{
    auto* xform = ensureOverlayTransform(
        r, e_overlay, recomputeAutoTransform, depthSafetyFactor, worldSRS, detail.textureSize);
    auto* viewNode = dynamic_cast<OverlayBakeViewNode*>(detail.viewNode.get());
    if (!xform || !viewNode || !viewNode->camera || !viewNode->view)
        return false;

    SRS srs = worldSRS.valid() ? worldSRS : (xform->position.srs.isGeodetic() ? xform->position.srs.geocentricSRS() : xform->position.srs);
    if (!srs.valid())
        return false;

    auto world = makeWorldMatrix(*xform, srs);

    glm::dvec3 tx(world[3]);
    glm::dvec3 x(world[0]);
    glm::dvec3 y(world[1]);
    glm::dvec3 z(world[2]);

    double sx = glm::length(x);
    double sy = glm::length(y);
    double sz = glm::length(z);
    if (sx <= 0.0 || sy <= 0.0 || sz <= 0.0)
        return false;

    x /= sx;
    y /= sy;
    z /= sz;

    double depth = std::max(10.0, sz * 4.0);

    viewNode->target = e_overlay;
    viewNode->worldSRS = srs;
    viewNode->eye = to_vsg(tx + z * depth * 0.5);
    viewNode->center = to_vsg(tx);
    viewNode->up = to_vsg(y);
    viewNode->left = -sx * 0.5;
    viewNode->right = sx * 0.5;
    viewNode->bottom = -sy * 0.5;
    viewNode->top = sy * 0.5;
    viewNode->znear = 0.01;
    viewNode->zfar = depth;

    return true;
}

void OverlayBakeSystemNode::update(VSGContext vsgcontext)
{
    if (status.failed())
        return;

    bool depthPolicyChanged = std::abs(depthSafetyFactor - _lastDepthSafetyFactor) > 1e-6f;
    _lastDepthSafetyFactor = depthSafetyFactor;

    std::vector<entt::entity> overlays;
    std::vector<entt::entity> needsSetup;

    _registry.read([&](entt::registry& r)
        {
            auto view = r.view<Overlay>();
            view.each([&](auto e_overlay, auto&)
                {
                    if (!r.any_of<Decal>(e_overlay))
                        overlays.push_back(e_overlay);
                });
        });

    _registry.write([&](entt::registry& r)
        {
            for (auto e_overlay : overlays)
            {
                auto& overlay = r.get<Overlay>(e_overlay);
                auto& detail = r.get_or_emplace<OverlayBakeDetail>(e_overlay);
                auto requestedSize = resolveTextureSize(overlay, textureSize);
                if (depthPolicyChanged)
                    detail.autoTransformDirty = true;
                if (!(detail.renderGraph && detail.texture && detail.viewNode && detail.hostCommandGraph) ||
                    detail.textureSize != requestedSize ||
                    detail.useDepthBuffer != overlay.useDepthBuffer)
                    needsSetup.push_back(e_overlay);
            }
        });

    struct PendingSetup
    {
        entt::entity e_overlay = entt::null;
        glm::uvec2 textureSize = { 0u, 0u };
        bool useDepthBuffer = false;
        vsg::ref_ptr<vsg::ImageInfo> texture;
        vsg::ref_ptr<vsg::RenderGraph> renderGraph;
        vsg::ref_ptr<vsg::Node> viewNode;
        vsg::ref_ptr<vsg::CommandGraph> hostCommandGraph;
    };

    std::vector<PendingSetup> pending;
    pending.reserve(needsSetup.size());

    for (auto e_overlay : needsSetup)
    {
        glm::uvec2 requestedSize = { textureSize, textureSize };
        bool useDepthBuffer = false;
        _registry.read([&](entt::registry& r)
            {
                if (r.valid(e_overlay) && r.any_of<Overlay>(e_overlay))
                {
                    const auto& overlay = r.get<Overlay>(e_overlay);
                    requestedSize = resolveTextureSize(overlay, textureSize);
                    useDepthBuffer = overlay.useDepthBuffer;
                }
            });

        PendingSetup p;
        p.e_overlay = e_overlay;
        p.textureSize = requestedSize;
        p.useDepthBuffer = useDepthBuffer;
        if (createBakeResources(vsgcontext, requestedSize, useDepthBuffer, p.texture, p.renderGraph, p.viewNode, p.hostCommandGraph))
            pending.emplace_back(std::move(p));
    }

    _registry.write([&](entt::registry& r)
        {
            for (auto& p : pending)
            {
                if (!r.valid(p.e_overlay) || !r.any_of<Overlay>(p.e_overlay) ||
                    resolveTextureSize(r.get<Overlay>(p.e_overlay), textureSize) != p.textureSize ||
                    r.get<Overlay>(p.e_overlay).useDepthBuffer != p.useDepthBuffer)
                {
                    dispose(p.renderGraph);
                    dispose(p.texture);
                    continue;
                }

                auto& detail = r.get_or_emplace<OverlayBakeDetail>(p.e_overlay);

                if (detail.hostCommandGraph && detail.renderGraph)
                {
                    auto& children = detail.hostCommandGraph->children;
                    children.erase(std::remove(children.begin(), children.end(), detail.renderGraph), children.end());
                }

                dispose(detail.renderGraph);
                dispose(detail.texture);

                detail.texture = p.texture;
                detail.renderGraph = p.renderGraph;
                detail.viewNode = p.viewNode;
                detail.hostCommandGraph = p.hostCommandGraph;
                detail.textureSize = p.textureSize;
                detail.useDepthBuffer = p.useDepthBuffer;
                detail.contentStampValid = false;
                detail.autoTransformDirty = true;
                detail.bakeFramesRemaining = INITIAL_BAKE_FRAMES;
            }

            bool requestAnotherFrame = false;

            for (auto e_overlay : overlays)
            {
                if (!r.valid(e_overlay) || !r.any_of<Overlay>(e_overlay))
                    continue;

                auto& detail = r.get_or_emplace<OverlayBakeDetail>(e_overlay);

                if (!(detail.renderGraph && detail.texture && detail.viewNode && detail.hostCommandGraph))
                    continue;

                auto geometryStamp = computeOverlayGeometryStamp(r, e_overlay);
                if (!detail.geometryStampValid || geometryStamp != detail.geometryStamp)
                {
                    detail.geometryStamp = geometryStamp;
                    detail.geometryStampValid = true;
                    if (r.any_of<AutoOverlayTransform>(e_overlay))
                        detail.autoTransformDirty = true;
                }

                auto contentStamp = computeOverlayContentStamp(r, e_overlay, geometryStamp);
                if (!detail.contentStampValid || contentStamp != detail.contentStamp)
                {
                    detail.contentStamp = contentStamp;
                    detail.contentStampValid = true;
                    detail.bakeFramesRemaining = std::max(detail.bakeFramesRemaining, INITIAL_BAKE_FRAMES);
                }

                if (depthPolicyChanged && r.any_of<AutoOverlayTransform>(e_overlay))
                    detail.bakeFramesRemaining = std::max(detail.bakeFramesRemaining, INITIAL_BAKE_FRAMES);

                if (!updateBakeCamera(r, e_overlay, detail, detail.autoTransformDirty))
                {
                    if (detail.hostCommandGraph && detail.renderGraph)
                    {
                        auto& children = detail.hostCommandGraph->children;
                        children.erase(std::remove(children.begin(), children.end(), detail.renderGraph), children.end());
                    }
                    continue;
                }

                detail.autoTransformDirty = false;

                const auto& overlay = r.get<Overlay>(e_overlay);
                bool shouldBake = overlay.continuousBake || detail.bakeFramesRemaining > 0u;
                if (detail.hostCommandGraph && detail.renderGraph)
                {
                    auto& children = detail.hostCommandGraph->children;
                    auto existing = std::find(children.begin(), children.end(), detail.renderGraph);
                    if (shouldBake && existing == children.end())
                        children.insert(children.begin(), detail.renderGraph);
                    else if (!shouldBake && existing != children.end())
                        children.erase(existing);
                }

                if (overlay.continuousBake)
                {
                    requestAnotherFrame = true;
                }
                else if (detail.bakeFramesRemaining > 0u)
                {
                    --detail.bakeFramesRemaining;
                    requestAnotherFrame = requestAnotherFrame || detail.bakeFramesRemaining > 0u;
                }

                auto& bakeTexture = r.get_or_emplace<OverlayBakeTexture>(e_overlay);
                if (bakeTexture.texture != detail.texture || detail.styleEntity != e_overlay)
                {
                    bakeTexture.texture = detail.texture;
                    Overlay::dirty(r, e_overlay);
                    detail.styleEntity = e_overlay;
                }
            }

            if (requestAnotherFrame)
                vsgcontext->requestFrame();
        });

    Inherit::update(vsgcontext);
}
