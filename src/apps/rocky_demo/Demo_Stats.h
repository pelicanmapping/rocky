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
#include <rocky/vsg/Application.h>
#include <rocky/vsg/terrain/TerrainEngine.h>
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
    int frame_num = 0;
    float get_timings(void* data, int index) {
        return 0.001f * (float)(*(Timings*)data)[index].count();
    };
    unsigned long long average(void* data, int count, int start) {
        Timings& t = *(Timings*)(data);
        unsigned long long total = 0;
        int s = start - count; if (s < 0) s += frame_count;
        for (int i = s; i <= s + count; i++)
            total += t[i % frame_count].count();
        return total / count;
    }
    unsigned long long lowest(void* data, int count, int start) {
        Timings& t = *(Timings*)(data);
        long long result = INT_MAX;
        int s = start - count; if (s < 0) s += frame_count;
        for (int i = s; i <= s + count; i++)
            result = std::min(result, static_cast<long long>(t[i % frame_count].count()));
        return result;
    }
}
auto Demo_Stats = [](Application& app)
{
    int f = (++frame_num) % frame_count;
    frames[f] = app.stats.frame;
    events[f] = app.stats.events;
    update[f] = app.stats.update;
    record[f] = app.stats.record;

    static int over = 60;

    ImGui::SeparatorText("Timings");

    if (app.debugLayerOn())
    {
        ImGui::TextColored(ImVec4(1, .3f, .3f, 1), "Warning: debug validation is ON");
    }

    if (ImGuiLTable::Begin("Timings"))
    {
        std::string buf;

        if (app.context->renderContinuously)
        {
            //float fps = 1.0f / (1e-6f * (float)average(&frames, over, f));
            float fps = 1.0f / (1e-6f * (float)lowest(&frames, over, f));
            buf = util::format("%.2f ms (%.1f fps)", 0.001f * (float)app.stats.frame.count(), fps);
        }
        else
        {
            buf = util::format("%.2f ms", 0.001f * (float)app.stats.frame.count());
        }

        ImGuiLTable::PlotLines("Frame", get_timings, &frames, frame_count, f, buf.c_str(), 0.0f, 17.0f);

        buf = util::format(u8"%lld us", average(&events, over, f));
        ImGuiLTable::PlotLines("Event", get_timings, &events, frame_count, f, buf.c_str(), 0.0f, 10.0f);

        buf = util::format(u8"%lld us", average(&update, over, f));
        ImGuiLTable::PlotLines("Update", get_timings, &update, frame_count, f, buf.c_str(), 0.0f, 10.0f);

        buf = util::format(u8"%lld us", average(&record, over, f));
        ImGuiLTable::PlotLines("Record", get_timings, &record, frame_count, f, buf.c_str(), 0.0f, 10.0f);

        ImGuiLTable::End();
    }

    ImGui::SeparatorText("Memory");
    if (ImGuiLTable::Begin("Memory"))
    {
        auto& alloc = vsg::Allocator::instance();
        ImGuiLTable::Text("Working set", "%.1lf MB", (double)Memory::getProcessPhysicalUsage() / 1048576.0);
        ImGuiLTable::Text("Private bytes", "%.1lf MB", (double)Memory::getProcessPrivateUsage() / 1048576.0);

        // VSG allocator. Commented out for now b/c this API may not be threadsafe (occaissonal crashes)
        //if (alloc->allocatorType == vsg::ALLOCATOR_TYPE_VSG_ALLOCATOR)
        //{
        //    ImGuiLTable::Text("VSG alloc total", "%.1lf MB", (double)alloc->totalMemorySize() / 1048576.0);
        //    ImGuiLTable::Text("VSG alloc available", "%.1lf MB", (double)alloc->totalAvailableSize() / 1048576.0);
        //    ImGuiLTable::Text("VSG alloc reserved", "%.1lf MB", (double)alloc->totalReservedSize() / 1048576.0);
        //}
        ImGuiLTable::End();
    }

    ImGui::SeparatorText("Thread Pools");
    auto* metrics = jobs::get_metrics();
    if (ImGuiLTable::Begin("Thread Pools"))
    {
        for (auto m : metrics->all())
        {
            if (m)
            {
                std::string name = m->name.empty() ? "default" : m->name;
                auto buf = util::format("(%d) %d / %d", (int)m->concurrency, (int)m->running, (int)m->pending);
                ImGuiLTable::Text(name.c_str(), buf.c_str());
            }
        }
        ImGuiLTable::End();
    }

    auto& engine = app.mapNode->terrainNode->engine;
    auto& cache = app.context->io.services.contentCache;
    float ratio = cache->gets > 0 ? float(cache->hits) / float(cache->gets) : 0.0f;

    ImGui::SeparatorText("System");
    if (ImGuiLTable::Begin("System"))
    {
        ImGuiLTable::Text("Loader concurrency", std::to_string(engine->settings.concurrency).c_str());
        ImGuiLTable::Text("Resident tiles", std::to_string(engine->tiles.size()).c_str());
        ImGuiLTable::Text("Geometry pool cache", std::to_string(engine->geometryPool.size()).c_str());
        ImGuiLTable::Text("Content cache hits", "%d%%", int(ratio * 100.0f));
        ImGui::SameLine();
        if (ImGui::Button("Clear"))
            cache->clear();

        ImGuiLTable::End();
    }
};
