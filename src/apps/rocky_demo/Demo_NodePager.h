/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/NodePager.h>
#include <rocky/ElevationSampler.h>
#include "helpers.h"

using namespace ROCKY_NAMESPACE;

auto Demo_NodePager = [](Application& app)
{
    static const Color colors[4] = {
        Color{1, 0, 0, 1}, // red
        Color{0, 1, 0, 1}, // green
        Color{0, 0, 1, 1}, // blue
        Color{1, 1, 0, 1}  // yellow
    };

    static vsg::ref_ptr<NodePager> pager;
    static Profile profile("global-geodetic");
    static ElevationSampler clamper;

    if (!pager)
    {
        // set up the elevation clamper:
        clamper.layer = app.mapNode->map->layer<ElevationLayer>();

        // set up the pager, which needs to know both the tiling profile it will use
        // and the profile of the map.
        pager = NodePager::create(profile, app.mapNode->srs());

        // tiles will start to appear at this level of detail:
        pager->minLevel = 1;

        // tiles will max out at the level of detail:
        pager->maxLevel = 16;

        // whether to replace each LOD with the higher one as you zoom in (versus accumulating them)
        pager->refinePolicy = NodePager::RefinePolicy::Replace;

        // a function that will calculate the bounding sphere for each tile.
        auto calculateTileBound = [&](const TileKey& key, const IOOptions& io)
            {
                auto ex = key.extent().transform(app.mapNode->srs());
                auto bs = ex.createWorldBoundingSphere(0, 0);

                if (clamper.ok() && key.level > 1)
                {
                    auto resolutionX = clamper.layer->resolution(key.level).first;
                    if (auto p = clamper.clamp(ex.centroid(), resolutionX, io))
                    {
                        auto n = glm::normalize(bs.center);
                        bs.center += n * p.value().transform(ex.srs().geodeticSRS()).z;
                        return vsg::dsphere(to_vsg(bs.center), bs.radius * 1.01);
                    }
                }

                return to_vsg(bs);
            };

        // we'll use it to control tile paging:
        pager->calculateBound = calculateTileBound;

        // The function that will create the payload for each TileKey:
        pager->createPayload = [&app, calculateTileBound](const TileKey& key, const IOOptions& io)
            {
                // Make a simple topocentric-aligned bounding box representing our clamped tile.
                // Let's use VSG's builder utility to make it easy.
                vsg::Builder builder;

                auto bs = calculateTileBound(key, io);  
                auto r = bs.radius / sqrt(2);
                vsg::box box(vsg::vec3(-r, -r, -r), vsg::vec3(r, r, r));

                vsg::GeometryInfo gi(box);
                gi.color = to_vsg(Color::Cyan);
                gi.transform = to_vsg(key.profile.srs().ellipsoid().topocentricToGeocentricMatrix(to_glm(bs.center)));

                vsg::StateInfo si;
                si.lighting = false;
                si.wireframe = true;

                return builder.createBox(gi, si);
            };

        // Always initialize a NodePager before using it:
        pager->initialize(app.vsgcontext);

        app.mainScene->addChild(pager);

        app.vsgcontext->requestFrame();
    }

    if (ImGuiLTable::Begin("NodePager"))
    {
        static std::vector<std::string> pn = { "global-geodetic", "spherical-mercator" }; // , "qsc+x", "qsc+z", "qsc-z"
        int profileIndex = 0;
        for (int i = 0; i < pn.size(); i++)
            if (pn[i] == profile.wellKnownName())
                profileIndex = i;

        if (ImGuiLTable::BeginCombo("Profile", pn[profileIndex].c_str()))
        {
            for (int i = 0; i < pn.size(); ++i)
            {
                if (ImGui::RadioButton(pn[i].c_str(), profileIndex == i))
                {
                    profileIndex = i;
                    profile = Profile(pn[i]);                    
                    app.vsgcontext->onNextUpdate([&]()
                        {
                            util::remove(pager, app.mainScene->children);
                            pager = nullptr;
                        });

                    app.vsgcontext->requestFrame();
                }
            }
            ImGuiLTable::EndCombo();
        }

        ImGuiLTable::Text("Tiles", "%u", pager->tiles());

        bool accumulate = pager->refinePolicy == NodePager::RefinePolicy::Accumulate;
        if (ImGuiLTable::Checkbox("Accumulate", &accumulate))
        {
            pager->refinePolicy = accumulate ? NodePager::RefinePolicy::Accumulate : NodePager::RefinePolicy::Replace;
            app.vsgcontext->requestFrame();
        }

        if (ImGuiLTable::SliderFloat("Pixel Error", &pager->pixelError, 64.0f, 1024.0f, "%.0f px"))
        {
            app.vsgcontext->requestFrame();
        }

        if (ImGuiLTable::Button("Reload"))
        {
            pager->initialize(app.vsgcontext);
            app.vsgcontext->requestFrame();
        }

        ImGuiLTable::End();

        auto keys = pager->tileKeys();
        if (!keys.empty())
        {
            std::sort(keys.begin(), keys.end());
            int count = 0;
            for (auto& key : keys)
            {   
                ImGui::Text("%s", key.str().c_str());
                if (++count % 6 > 0) ImGui::SameLine();
            }
            ImGui::Text("%s", " ");
        }
    }
};
