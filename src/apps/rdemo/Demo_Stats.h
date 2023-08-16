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

    const int over = 60;

    ImGui::SeparatorText("Timings");
    ImGuiLTable::Begin("Timings");
    {
        sprintf(buf, "%.2f ms", 0.001f * (float)app.stats.frame.count());
        ImGuiLTable::PlotLines("Frame", get_timings, &frames, frame_count, f, buf, 0.0f, 17.0f);

        sprintf(buf, u8"%lld \x00B5s", average(&events, over, f));
        ImGuiLTable::PlotLines("Event", get_timings, &events, frame_count, f, buf, 0.0f, 10.0f);

        sprintf(buf, u8"%lld \x00B5s", average(&update, over, f));
        ImGuiLTable::PlotLines("Update", get_timings, &update, frame_count, f, buf, 0.0f, 10.0f);

        sprintf(buf, u8"%lld \x00B5s", average(&record, over, f));
        ImGuiLTable::PlotLines("Record", get_timings, &record, frame_count, f, buf, 0.0f, 10.0f);
    }
    ImGuiLTable::End();

    ImGui::SeparatorText("Thread Pools");
    auto& metrics = util::job_metrics::get();
    ImGuiLTable::Begin("Thread Pools");
    for (auto& m : metrics)
    {
        if (m) {
            std::string name = m->name.empty() ? "default" : m->name;
            sprintf(buf, "(%d) %d / %d", (int)m->concurrency, (int)m->running, (int)m->pending);
            ImGuiLTable::Text(name.c_str(), buf);
        }
    }
    ImGuiLTable::End();
        
    frame_num++;
};