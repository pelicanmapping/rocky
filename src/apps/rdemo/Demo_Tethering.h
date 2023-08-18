/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Viewpoint.h>
#include <rocky/SRS.h>
#include <rocky_vsg/MapManipulator.h>

#include <vsg/utils/Builder.h>

#include "helpers.h"
using namespace ROCKY_NAMESPACE;

using namespace std::chrono_literals;

auto Demo_Tethering = [](Application& app)
{
    auto view = app.displayConfiguration.windows.begin()->second.front();
    auto manip = view ? view->getObject<MapManipulator>("rocky.manip") : nullptr;
    if (!manip) return;

    // Make an entity for us to tether to and set it in motion
    static std::shared_ptr<MapObject> entity;
    const float s = 20.0;
    if (!entity)
    {
        vsg::GeometryInfo gi;
        gi.color = { 1.0, 0.1, 0.1, 1.0 };
        gi.dx = { s, 0, 0 };
        gi.dy = { 0, s, 0 };
        gi.dz = { 0, 0, s };

        auto attachment = Attachment::create();
        auto sw = vsg::Switch::create();
        sw->addChild(true, vsg::Builder().createSphere(gi));
        attachment->node = sw;
        attachment->underGeoTransform = true;

        entity = MapObject::create(attachment);
        entity->xform->setPosition(GeoPoint(SRS::WGS84, -121, 38, 25000.0));

        app.add(entity);
    }

    // move the entity:
    auto pos = entity->xform->position();
    pos.x += 0.0005;
    entity->xform->setPosition(pos);

    if (ImGuiLTable::Begin("tethering"))
    {
        bool tethering = manip->isTethering();
        if (ImGuiLTable::Checkbox("Tether active:", &tethering))
        {
            if (tethering)
            {
                auto vp = manip->getViewpoint();
                vp.target = entity;
                vp.range = s * 12.0;
                manip->setViewpoint(vp, 2.0s);
            }
            else
            {
                manip->home();
            }
        }
        ImGuiLTable::End();
    }
};
