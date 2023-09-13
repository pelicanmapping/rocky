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

auto Demo_Serialization = [](Application& app)
{
    if (ImGui::Button("Serialize Map to stdout (JSON)"))
    {
        JSON map_json = app.mapNode->map->to_json();
        std::cout << rocky::json_pretty(map_json) << std::endl;
    }
};
