/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */

 /**
 * THE DEMO APPLICATION is an ImGui-based app that shows off all the features
 * of the Rocky Application API. We intend each "Demo_*" include file to be
 * both a unit test for that feature, and a reference or writing your own code.
 */
#include <rocky_vsg/Application.h>
#include <rocky_vsg/engine/TerrainEngine.h>
#include <rocky/Memory.h>
#include <vsg/core/Allocator.h>
#include "helpers.h"

using namespace ROCKY_NAMESPACE;

namespace
{
    vsg::ref_ptr<vsg::DeviceMemory> memory;
    const int frame_count = 300;
    using Timings = std::vector<std::chrono::microseconds>;
    Timings frames(frame_count);
    Timings events(frame_count);
    Timings update(frame_count);
    Timings record(frame_count);
    Timings present(frame_count);
    int frame_num = 0;
    char buf[256];
    float get_timings(void* data, int index) {
        return 0.001 * (float)(*(Timings*)data)[index].count();
    };
    unsigned long long average(void* data, int count, int start) {
        Timings& t = *(Timings*)(data);
        unsigned long long total = 0;
        int s = start - count; if (s < 0) s += frame_count;
        for (int i = s; i <= s + count; i++)
            total += t[i % frame_count].count();
        return total / count;
    }
}
auto Demo_Stats = [](Application& app)
{
    int f = frame_num % frame_count;

    frames[f] = app.stats.frame;
    events[f] = app.stats.events;
    update[f] = app.stats.update;
    record[f] = app.stats.record;
    present[f] = app.stats.present;

    const int over = 60;

    ImGui::SeparatorText("Timings");

    if (app.debugLayerOn())
    {
        ImGui::TextColored(ImVec4(1, .3, .3, 1), "Warning: debug validation is ON");
    }
    if (app.instance.runtime().asyncCompile == false)
    {
        ImGui::TextColored(ImVec4(1, .3, .3, 1), "Warning: async compilation is OFF");
    }

    if (ImGuiLTable::Begin("Timings"))
    {
        sprintf(buf, "%.2f ms", 0.001f * (float)app.stats.frame.count());
        ImGuiLTable::PlotLines("Frame", get_timings, &frames, frame_count, f, buf, 0.0f, 17.0f);

        sprintf(buf, u8"%lld \x00B5s", average(&events, over, f));
        ImGuiLTable::PlotLines("Event", get_timings, &events, frame_count, f, buf, 0.0f, 10.0f);

        sprintf(buf, u8"%lld \x00B5s", average(&update, over, f));
        ImGuiLTable::PlotLines("Update", get_timings, &update, frame_count, f, buf, 0.0f, 10.0f);

        sprintf(buf, u8"%lld \x00B5s", average(&record, over, f));
        ImGuiLTable::PlotLines("Record", get_timings, &record, frame_count, f, buf, 0.0f, 10.0f);

        sprintf(buf, u8"%lld \x00B5s", average(&present, over, f));
        ImGuiLTable::PlotLines("Present", get_timings, &present, frame_count, f, buf, 0.0f, 10.0f);
        ImGuiLTable::End();
    }

    ImGui::SeparatorText("Memory");
    if (ImGuiLTable::Begin("Memory"))
    {
        auto& alloc = vsg::Allocator::instance();
        ImGuiLTable::Text("Process private", "%.1lf MB", (double)Memory::getProcessPhysicalUsage() / 1048576.0);
        if (alloc->allocatorType == vsg::ALLOCATOR_TYPE_VSG_ALLOCATOR)
        {
            ImGuiLTable::Text("VSG alloc total", "%.1lf MB", (double)alloc->totalMemorySize() / 1048576.0);
            ImGuiLTable::Text("VSG alloc available", "%.1lf MB", (double)alloc->totalAvailableSize() / 1048576.0);
            ImGuiLTable::Text("VSG alloc reserved", "%.1lf MB", (double)alloc->totalReservedSize() / 1048576.0);
        }
        {
            std::stringstream buf;
            std::unique_lock lock(app.instance.runtime()._deferred_unref_mutex);
            for (auto& v : app.instance.runtime()._deferred_unref_queue)
                buf << std::to_string(v.size()) << ' ';
            ImGuiLTable::Text("Deferred deletes", buf.str().c_str());
        }
        ImGuiLTable::End();
    }

    ImGui::SeparatorText("Thread Pools");
    auto& metrics = util::job_metrics::get();
    if (ImGuiLTable::Begin("Thread Pools"))
    {
        for (auto& m : metrics)
        {
            if (m) {
                std::string name = m->name.empty() ? "default" : m->name;
                sprintf(buf, "(%d) %d / %d", (int)m->concurrency, (int)m->running, (int)m->pending);
                ImGuiLTable::Text(name.c_str(), buf);
            }
        }
        ImGuiLTable::End();
    }

    ImGui::SeparatorText("Terrain Engine");
    if (ImGuiLTable::Begin("Terrain Engine"))
    {
        auto& engine = app.mapNode->terrain->_engine;
        ImGuiLTable::Text("Active tiles", std::to_string(engine->tiles.size()).c_str());
        ImGuiLTable::Text("Geometry pool cache", std::to_string(engine->geometryPool._sharedGeometries.size()).c_str());
        ImGuiLTable::End();
    }

    frame_num++;
};