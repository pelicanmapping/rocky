/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky_vsg/Icon.h>


#include "helpers.h"
using namespace ROCKY_NAMESPACE;

auto Demo_Icon = [](Application& app)
{
    static shared_ptr<MapObject> object;
    static shared_ptr<Icon> icon;
    static bool visible = true;
    static Status status;

    if (status.failed())
    {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Image load failed");
        ImGui::TextColored(ImVec4(1, 0, 0, 1), status.message.c_str());
        return;
    }

    if (!icon)
    {
        ImGui::Text("Wait...");

        auto io = app.instance.ioOptions();
        auto image = io.services().readImageFromURI("https://user-images.githubusercontent.com/326618/236923465-c85eb0c2-4d31-41a7-8ef1-29d34696e3cb.png", io);
        if (image.status.failed())
        {
            status = image.status;
            return;
        }

        icon = Icon::create();

        icon->setImage(image.value);
        icon->setStyle(IconStyle{ 128.0f });

        object = MapObject::create(icon);
        object->xform->setPosition(GeoPoint(SRS::WGS84, 0, 0, 50000));

        app.add(object);

        // by the next frame, the object will be alive in the scene
        return;
    }

    if (ImGuiLTable::Begin("icon"))
    {
        if (ImGuiLTable::Checkbox("Visible", &visible))
        {
            if (visible)
                app.add(object);
            else
                app.remove(object);
        }

        auto style = icon->style();

        if (ImGuiLTable::SliderFloat("Pixel size", &style.size_pixels, 1.0f, 1024.0f))
        {
            icon->setStyle(style);
        }

        ImGuiLTable::End();
    }
};
