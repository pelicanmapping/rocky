/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Viewpoint.h"

using namespace ROCKY_NAMESPACE;


#include "json.h"
namespace ROCKY_NAMESPACE
{
    void to_json(json& j, const Viewpoint& obj) {
        j = json::object();
        set(j, "name", obj.name);
        set(j, "heading", obj.heading);
        set(j, "pitch", obj.pitch);
        set(j, "range", obj.range);
        if (obj.positionOffset.has_value()) {
            set(j, "x_offset", obj.positionOffset->x);
            set(j, "y_offset", obj.positionOffset->y);
            set(j, "z_offset", obj.positionOffset->z);
        }
        //TODO
        //if (obj.target && obj.target->position().valid())
        //    j.at("position") = parse_json(obj.target->to_json());
    }

    void from_json(const json& j, Viewpoint& obj) {
        get_to(j, "name", obj.name);
        get_to(j, "heading", obj.heading);
        get_to(j, "pitch", obj.pitch);
        get_to(j, "range", obj.range);
        get_to(j, "x_offset", obj.positionOffset->x);
        get_to(j, "y_offset", obj.positionOffset->y);
        get_to(j, "z_offset", obj.positionOffset->z);
        //TODO target
    }
}
