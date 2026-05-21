/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include "helpers.h"
#include <array>

using namespace ROCKY_NAMESPACE;

constexpr std::string_view DEMO_AUDIO_FILE = "data/audio/dragon-studio-helicopter-sound-8d-372463.mp3";

auto Demo_Audio = [](Application& app)
{
    struct SourceSpec
    {
        const char* label;
        double lon;
        double lat;
        double alt;
        Color color;
    };

    static std::vector<entt::entity> entities;
    static float gain = 0.35f;

    if (entities.empty())
    {
        static const std::array<SourceSpec, 3> specs = {
            SourceSpec{ "Helicopter Los Angeles", -118.2437, 34.0522, 1500.0, StockColor::Orange },
            SourceSpec{ "Helicopter London", -0.1278, 51.5074, 1500.0, StockColor::Cyan },
            SourceSpec{ "Helicopter Tokyo", 139.6917, 35.6895, 1500.0, StockColor::Lime }
        };

        app.registry.write([&](entt::registry& reg)
            {
                for (auto& spec : specs)
                {
                    auto entity = reg.create();
                    entities.emplace_back(entity);

                    auto& style = reg.emplace<LabelStyle>(entity);
                    style.textSize = 18.0f;
                    style.textOutlineSize = 1.0f;
                    style.textPivot = { 0.5f, 1.0f };
                    style.textOffset = { 0, -18 };
                    style.textColor = spec.color;
                    style.textOutlineColor = StockColor::Black;
                    style.borderColor = spec.color;
                    style.backgroundColor = Color("#101010cc");
                    style.padding = { 5.0f, 3.0f };
                    style.fontName = std::filesystem::path(ROCKY_DEMO_DEFAULT_FONT).lexically_normal().string();

                    auto& label = reg.emplace<Label>(entity, spec.label);
                    label.style = entity;

                    auto& transform = reg.emplace<Transform>(entity);
                    transform.position = GeoPoint(SRS::WGS84, spec.lon, spec.lat, spec.alt);
                    transform.radius = 50000.0;

                    auto& audio = reg.emplace<AudioSource>(entity, DEMO_AUDIO_FILE, true);
                    audio.gain = gain;
                    audio.minDistance = 500.0f;
                    audio.maxDistance = 8046.72f; // 5 miles in meters.
                    audio.rolloff = 1.0f;
                    audio.play(reg);
                }
            });

        app.vsgcontext->requestFrame();
    }

    if (ImGuiLTable::Begin("audio demo"))
    {
        if (ImGuiLTable::SliderFloat("Gain", &gain, 0.0f, 1.0f, "%.2f"))
        {
            app.registry.read([&](entt::registry& reg)
                {
                    for (auto entity : entities)
                    {
                        if (auto* audio = reg.try_get<AudioSource>(entity))
                        {
                            audio->gain = gain;
                            audio->dirty(reg);
                        }
                    }
                });
        }

        if (ImGuiLTable::Button("Play all"))
        {
            app.registry.read([&](entt::registry& reg)
                {
                    for (auto entity : entities)
                    {
                        if (auto* audio = reg.try_get<AudioSource>(entity))
                            audio->play(reg);
                    }
                });
        }

        if (ImGuiLTable::Button("Stop all"))
        {
            app.registry.read([&](entt::registry& reg)
                {
                    for (auto entity : entities)
                    {
                        if (auto* audio = reg.try_get<AudioSource>(entity))
                            audio->stop(reg);
                    }
                });
        }

        ImGuiLTable::End();
    }
};
