/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky_vsg/Label.h>

#include "helpers.h"
using namespace ROCKY_NAMESPACE;

auto Demo_Label = [](Application& app)
{
    static shared_ptr<MapObject> object;
    static shared_ptr<Label> label;
    static bool visible = true;

    auto font = app.instance.runtime().defaultFont;

    if (font.working())
    {
        ImGui::Text("Loading font, please wait...");
        return;
    }

    if (!font.available())
    {
        ImGui::TextWrapped("No font available - did you set the ROCKY_DEFAULT_FONT environment variable?");
        return;
    }

    if (!label)
    {
        label = Label::create();
        label->setText("Hello, world.");
        label->textNode->font = font.value();

        object = MapObject::create(label);
        object->xform->setPosition(GeoPoint(SRS::WGS84, -35.0, 15.0));
        app.add(object);

        return;
    }

    if (ImGuiLTable::Begin("text"))
    {
        if (ImGuiLTable::Checkbox("Visible", &visible))
        {
            if (visible)
                app.add(object);
            else
                app.remove(object);
        }

        char buf[256];
        strcpy(&buf[0], label->text().c_str());
        if (ImGuiLTable::InputText("Text", buf, 255))
        {
            label->setText(std::string(buf));
        }

        auto& pos = object->xform->position();
        glm::fvec3 vec{ pos.x, pos.y, pos.z };

        if (ImGuiLTable::SliderFloat("Latitude", &vec.y, -85.0, 85.0, "%.1f"))
        {
            object->xform->setPosition(GeoPoint(pos.srs(), vec.x, vec.y, vec.z));
        }

        if (ImGuiLTable::SliderFloat("Longitude", &vec.x, -180.0, 180.0, "%.1f"))
        {
            object->xform->setPosition(GeoPoint(pos.srs(), vec.x, vec.y, vec.z));
        }

        ImGuiLTable::End();
    }
};
