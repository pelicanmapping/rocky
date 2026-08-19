/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include "helpers.h"

using namespace ROCKY_NAMESPACE;


auto Demo_Clamping = [](Application& app)
{
    struct Clamping : public Component<Clamping>
    {
        bool clampToTerrain = true;
    };


    class ClampingSystem : public vsg::Inherit<detail::SimpleSystemNodeBase, ClampingSystem>
    {
    public:
        ClampingSystem(Registry& registry) : Inherit(registry) {}

        static std::shared_ptr<ClampingSystem> create(Registry& r) {
            return std::make_shared<ClampingSystem>(r);
        }

        ElevationSampler sampler;


        void initialize(VSGContext vsgcontext) override
        {
        }

        void update(VSGContext vsgcontext) override
        {
            if (_dirty)
            {
                _dirty = false;
                auto&& [_, reg] = _registry.write();

                auto clamp = [&](entt::entity e, LineGeometry& geom, Clamping& clamping)
                {
                    if (clamping.clampToTerrain)
                    {
                        auto r = sampler.clampRange(geom.srs, geom.points.begin(), geom.points.end(), vsgcontext->io);
                        geom.dirty(reg);
                    }
                };

                reg.view<LineGeometry, Clamping>().each(clamp);
            }
        }

        void dirty(bool value=true)
        {
            _dirty = value;
        }

    private:
        CallbackSubs _subs;
        bool _dirty = false;
    };


    static entt::entity e = entt::null;
    static std::shared_ptr<ClampingSystem> sys;
    static CallbackSubs _subs;

    if (e == entt::null)
    {
        // Create the clamping system and add it to the scene graph
        auto sys = ClampingSystem::create(app.registry);
        app.systemsNode->add(sys);

        sys->sampler.layer = app.mapNode->map->layer<ElevationLayer>();

        auto rebuildGeom = [](LineGeometry& geom, const LineStyle& style)
        {
            Feature feature;
            feature.geometry.type = Geometry::Type::LineString;
            feature.geometry.points = {
                { 7.49, 46.12, 4000.0 },
                { 8.11, 46.17, 4000.0 }
            };
            FeatureBuilder fb;
            fb.buildLineGeometry({ feature }, style, geom);
        };

        app.registry.write([&](entt::registry& reg)
            {
                e = reg.create();

                auto& geom = reg.emplace<LineGeometry>(e);

                auto& style = reg.emplace<LineStyle>(e);
                style.color = StockColor::Yellow;
                style.width = 5.0f; // pixels
                style.depthOffset = 100.0f; // meters
                style.resolution = 1000.0f; // meters

                reg.emplace<Line>(e, geom, style);

                auto& clamping = reg.emplace<Clamping>(e);
                clamping.clampToTerrain = true;

                rebuildGeom(geom, style);
            });

        _subs += app.mapNode->terrainNode->onTileLoaded([sys](const TileKey& key)
            {
                sys->dirty();
            });
    }


    if (ImGuiLTable::Begin("clamping demo"))
    {
        auto [_, reg] = app.registry.read();

        ImGuiLTable::End();
    }
};
