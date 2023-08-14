/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <vsg/io/read.h>
#include <vsg/nodes/MatrixTransform.h>

#include "helpers.h"
using namespace ROCKY_NAMESPACE;

class Model : public rocky::Inherit<Attachment, Model>
{
public:
    Model()
    {
        underGeoTransform = true;
        horizonCulling = true;
    }

    URI uri;

    void createNode(Runtime& runtime) override
    {
        auto result = uri.read(IOOptions());
        if (result.status.ok())
        {
            // this is a bit awkward but it works when the URI has an extension
            auto options = vsg::Options::create(*runtime.readerWriterOptions);
            auto extension = std::filesystem::path(uri.full()).extension();
            options->extensionHint = extension.empty() ? std::filesystem::path(result.value.contentType) : extension;
            std::stringstream in(result.value.data);
            auto model = vsg::read_cast<vsg::Node>(in, options);

            if (model)
            {
                // make it bigger so we can see it
                auto xform = vsg::MatrixTransform::create();
                xform->matrix = vsg::scale(150000.0);

                node = vsg::Switch::create();
                node->addChild(true, xform);
                xform->addChild(model);
            }
        }
    }

    JSON to_json() const override {
        return "";
    }
};


auto Demo_Model = [](Application& app)
{
    //ImGui::Text("Not yet implemented");

    static shared_ptr<MapObject> object;
    static shared_ptr<Model> model;
    static bool visible = true;
    static Status status;

    if (model && model->node == nullptr)
    {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Model load failed!");
        return;
    }

    if (!model)
    {
        ImGui::Text("Wait...");
    
        model = Model::create();
        model->uri = URI("https://raw.githubusercontent.com/vsg-dev/vsgExamples/master/data/models/teapot.vsgt");

        object = MapObject::create(model);
        object->xform->setPosition(GeoPoint(SRS::WGS84, 0, 0, 50000));

        app.add(object);

        // by the next frame, the object will be alive in the scene
        return;
    }

    if (ImGuiLTable::Begin("model"))
    {
        if (ImGuiLTable::Checkbox("Visible", &visible))
        {
            if (visible)
                app.add(object);
            else
                app.remove(object);
        }

        GeoPoint pos = object->xform->position();
        glm::fvec3 v{ pos.x, pos.y, pos.z };

        if (ImGuiLTable::SliderFloat("Latitude", &v.y, -85.0, 85.0, "%.1f"))
        {
            object->xform->setPosition(GeoPoint(pos.srs, v.x, v.y, v.z));
        }

        if (ImGuiLTable::SliderFloat("Longitude", &v.x, -180.0, 180.0, "%.1f"))
        {
            object->xform->setPosition(GeoPoint(pos.srs, v.x, v.y, v.z));
        }

        if (ImGuiLTable::SliderFloat("Altitude", &v.z, 0.0, 2500000.0, "%.1f"))
        {
            object->xform->setPosition(GeoPoint(pos.srs, v.x, v.y, v.z));
        }

        ImGuiLTable::End();
    }
};
