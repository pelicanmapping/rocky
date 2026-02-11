/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#include "SkyNode.h"
#include "VSGUtils.h"
#include "PipelineState.h"
#include <rocky/Ellipsoid.h>
#include <rocky/Ephemeris.h>

using namespace ROCKY_NAMESPACE;

#define LC "[SkyNode] "

namespace
{
    vsg::ref_ptr<vsg::Command> makeEllipsoid(
        const SRS& worldSRS,
        float thickness,
        bool with_tex_coords,
        bool with_normals)
    {
        auto geodeticSRS = worldSRS.geodeticSRS(); // get a long/lat SRS
        SRSOperation geodeticToGeocentric = geodeticSRS.to(worldSRS); // and a xform

        int latSegments = 100;
        int lonSegments = 2 * latSegments;
        int num_verts = (latSegments + 1) * (lonSegments);
        int num_indices = (latSegments * lonSegments * 6);

        vsg::DataList arrays;
        vsg::ref_ptr<vsg::vec3Array> verts, normals;
        vsg::ref_ptr<vsg::vec2Array> uvs;

        verts = vsg::vec3Array::create(num_verts);
        arrays.push_back(verts);

        if (with_tex_coords) {
            uvs = vsg::vec2Array::create(num_verts);
            arrays.push_back(uvs);
        }

        if (with_normals) {
            normals = vsg::vec3Array::create(num_verts);
            arrays.push_back(normals);
        }

        auto indices = vsg::ushortArray::create(num_indices);


        double segmentSize = 180.0 / (double)latSegments; // degrees

        int iptr = 0;
        for (int y = 0; y <= latSegments; ++y)
        {
            double lat = -90.0 + segmentSize * (double)y;

            for (int x = 0; x < lonSegments; ++x)
            {
                int vptr = (y * lonSegments) + x;
                double lon = -180.0 + segmentSize * (double)x;

                vsg::dvec3 g;
                geodeticToGeocentric.transform(glm::dvec3(lon, lat, thickness), g);
                verts->set(vptr, vsg::vec3(g.x, g.y, g.z));

                if (with_tex_coords)
                {
                    double s = (lon + 180) / 360.0;
                    double t = (lat + 90.0) / 180.0;
                    uvs->set(vptr, vsg::vec2(s, t));
                }

                if (with_normals)
                {
                    auto normal = vsg::normalize(vsg::vec3(g.x, g.y, g.z));
                    normals->set(vptr, normal);
                }

                if (y < latSegments)
                {
                    int x_plus_1 = x < lonSegments - 1 ? x + 1 : 0;
                    int y_plus_1 = y + 1;
                    indices->set(iptr++, y * lonSegments + x);
                    indices->set(iptr++, y * lonSegments + x_plus_1);
                    indices->set(iptr++, y_plus_1 * lonSegments + x);

                    indices->set(iptr++, y * lonSegments + x_plus_1);
                    indices->set(iptr++, y_plus_1 * lonSegments + x_plus_1);
                    indices->set(iptr++, y_plus_1 * lonSegments + x);
                }
            }
        }

        auto command = vsg::VertexIndexDraw::create();
        command->assignArrays(arrays);
        command->assignIndices(indices);
        command->indexCount = static_cast<uint32_t>(indices->size());
        command->instanceCount = 1;
        return command;
    }

#define ATMOSPHERE_VERT_SHADER "shaders/rocky.atmo.sky.vert"
#define ATMOSPHERE_FRAG_SHADER "shaders/rocky.atmo.sky.frag"

    vsg::ref_ptr<vsg::ShaderSet> makeAtmoShaderSet(VSGContext& context)
    {
        auto file = vsg::findFile(ATMOSPHERE_VERT_SHADER, context->searchPaths);
        Log()->info("Loading atmosphere vertex shader from: {}", file.string());

        // load shaders
        auto vertexShader = vsg::ShaderStage::read(
            VK_SHADER_STAGE_VERTEX_BIT,
            "main",
            vsg::findFile(ATMOSPHERE_VERT_SHADER, context->searchPaths),
            context->readerWriterOptions);

        auto fragmentShader = vsg::ShaderStage::read(
            VK_SHADER_STAGE_FRAGMENT_BIT,
            "main",
            vsg::findFile(ATMOSPHERE_FRAG_SHADER, context->searchPaths),
            context->readerWriterOptions);

        if (!vertexShader || !fragmentShader)
        {
            return { };
        }

        auto shaderSet = vsg::ShaderSet::create(vsg::ShaderStages{ vertexShader, fragmentShader });

        shaderSet->addAttributeBinding("in_vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));

        // need VSG view-dependent data (lights)
        PipelineUtils::addViewDependentData(shaderSet, VK_SHADER_STAGE_VERTEX_BIT);

        // vsg modelview and projection matrix
        shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT, 0, 128);

        return shaderSet;
    }


    vsg::ref_ptr<vsg::StateGroup> makeAtmoStateGroup(VSGContext& vsgcontext)
    {
        auto shaderSet = makeAtmoShaderSet(vsgcontext);
        if (!shaderSet)
        {
            Log()->warn(LC "Failed to create shader set!");
            return { };
        }

        // Make the pipeline configurator:
        auto pipelineConfig = vsg::GraphicsPipelineConfig::create(shaderSet);
        auto& defines = pipelineConfig->shaderHints->defines;

        // enable the arrays we think we need
        pipelineConfig->enableArray("in_vertex", VK_VERTEX_INPUT_RATE_VERTEX, 12);

        // activate the packed lights uniform
        PipelineUtils::enableViewDependentData(pipelineConfig);


        struct SetPipelineStates : public vsg::Visitor
        {
            void apply(vsg::Object& object) override {
                object.traverse(*this);
            }
            void apply(vsg::RasterizationState& state) override {
                state.cullMode = VK_CULL_MODE_FRONT_BIT;
            }
            void apply(vsg::DepthStencilState& state) override {
                state.depthCompareOp = VK_COMPARE_OP_ALWAYS;
                state.depthWriteEnable = VK_FALSE;
            }
            void apply(vsg::ColorBlendState& state) override {
                state.attachments = vsg::ColorBlendState::ColorBlendAttachments{
                    { true,
                      VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE, VK_BLEND_OP_ADD,
                      VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE, VK_BLEND_OP_ADD,
                      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT }
                };
            }
        };
        vsg::visit<SetPipelineStates>(pipelineConfig);

        if (vsgcontext->sharedObjects)
            vsgcontext->sharedObjects->share(pipelineConfig, [](auto gpc) { gpc->init(); });
        else
            pipelineConfig->init();

        // --- end pipelineconfig setup ---


        // set up the state group that will select the new pipeline:
        auto stategroup = vsg::StateGroup::create();
        stategroup->add(pipelineConfig->bindGraphicsPipeline);
        //stategroup->add(PipelineUtils::createViewDependentBindCommand(pipelineConfig, VK_PIPELINE_BIND_POINT_GRAPHICS));
        stategroup->add(vsg::BindViewDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineConfig->layout, VSG_VIEW_DEPENDENT_DESCRIPTOR_SET_INDEX));

        return stategroup;
    }


    vsg::ref_ptr<vsg::Node> makeAtmosphere(const SRS& srs, float thickness, VSGContext& vsgcontext)
    {
        // attach the actual atmospheric geometry
        const bool with_texcoords = false;
        const bool with_normals = false;

        auto stategroup = makeAtmoStateGroup(vsgcontext);
        if (!stategroup)
        {
            Log()->warn(LC "Failed to make state group!");
            return { };
        }

        auto geometry = makeEllipsoid(srs, thickness, with_texcoords, with_normals);

        stategroup->addChild(geometry);

        return stategroup;
    }
}

SkyNode::SkyNode(const VSGContext& c) :
    _context(c)
{
    setWorldSRS(SRS::ECEF);
}

void
SkyNode::setWorldSRS(const SRS& srs)
{
    if (srs.valid())
    {
        children.clear();

        // some ambient light:
        ambient = vsg::AmbientLight::create();
        ambient->name = "Sky Ambient";
        ambient->color = { 0.03f, 0.03f, 0.03f };
        addChild(ambient);

        // the sun:
        auto sun_data = rocky::Ephemeris().sunPosition(rocky::DateTime());
        sun = vsg::PointLight::create();
        sun->name = "Sol";
        sun->position = { sun_data.geocentric.x, sun_data.geocentric.y, sun_data.geocentric.z };
        sun->color = { 1, 1, 1 };
        sun->intensity = 1.0;
        addChild(sun);

        // Tell the shaders that lighting is a go
        //_context->shaderCompileSettings->defines.insert("ROCKY_LIGHTING");
        // TODO: dirty shaders

        // the atmopshere:
        const float earth_atmos_thickness = 50000.0; // 96560.0;
        _atmosphere = makeAtmosphere(srs, earth_atmos_thickness, _context);
        setShowAtmosphere(true);
    }
}

void
SkyNode::setShowAtmosphere(bool show)
{
    if (_atmosphere)
    {
        auto iter = std::find(children.begin(), children.end(), _atmosphere);

        if (iter == children.end() && show == true)
        {
            addChild(_atmosphere);

            // activate in shaders
            _context->shaderCompileSettings->defines.insert("ROCKY_ATMOSPHERE");
            // TODO: dirty shaders
        }
        else if (iter != children.end() && show == false)
        {
            children.erase(iter);

            // activate in shaders
            _context->shaderCompileSettings->defines.erase("ROCKY_ATMOSPHERE");
            // TODO: dirty shaders
        }
    }
}

void
SkyNode::setDateTime(const DateTime& value)
{
    auto sun_data = rocky::Ephemeris().sunPosition(value);
    sun->position = { sun_data.geocentric.x, sun_data.geocentric.y, sun_data.geocentric.z };
}
