/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "SkyNode.h"
#include "engine/Utils.h"
#include "engine/Runtime.h"

#include <rocky/Ellipsoid.h>
#include <rocky/Ephemeris.h>

#include <vsg/nodes/StateGroup.h>
#include <vsg/nodes/Geometry.h>
#include <vsg/nodes/VertexIndexDraw.h>
#include <vsg/utils/ShaderSet.h>
#include <vsg/utils/GraphicsPipelineConfigurator.h>
#include <vsg/state/BindDescriptorSet.h>
#include <vsg/state/ViewDependentState.h>
#include <vsg/io/Options.h>

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
        auto geodeticSRS = worldSRS.geoSRS(); // get a long/lat SRS
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
                geodeticToGeocentric.transform(dvec3(lon, lat, thickness), g);
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

    vsg::ref_ptr<vsg::ShaderSet> makeShaderSet()
    {
        // set up search paths to SPIRV shaders and textures
        vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");
        vsg::Paths morePaths = vsg::getEnvPaths("ROCKY_FILE_PATH");
        searchPaths.insert(searchPaths.end(), morePaths.begin(), morePaths.end());
        auto options = vsg::Options::create();

        // load shaders
        auto vertexShader = vsg::ShaderStage::read(
            VK_SHADER_STAGE_VERTEX_BIT,
            "main",
            vsg::findFile(ATMOSPHERE_VERT_SHADER, searchPaths),
            options);

        auto fragmentShader = vsg::ShaderStage::read(
            VK_SHADER_STAGE_FRAGMENT_BIT,
            "main",
            vsg::findFile(ATMOSPHERE_FRAG_SHADER, searchPaths),
            options);

        if (!vertexShader || !fragmentShader)
        {
            return { };
        }

        auto shaderSet = vsg::ShaderSet::create(vsg::ShaderStages{ vertexShader, fragmentShader });

        shaderSet->addAttributeBinding("in_vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
        shaderSet->addAttributeBinding("in_normal", "HAS_IN_NORMAL", 1, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
        shaderSet->addAttributeBinding("in_uv", "HAS_IN_UV", 2, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));

        // vsg packed lights data
        shaderSet->addUniformBinding("vsg_lights", "", 1, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array::create(64));

        // vsg modelview and projection matrix
        shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT, 0, 128);

        return shaderSet;
    }


    vsg::ref_ptr<vsg::StateGroup> makeStateGroup(bool with_normals, bool with_tex_coords, Runtime& runtime)
    {
        auto sharedObjects = runtime.sharedObjects;

        auto shaderSet = makeShaderSet();
        if (!shaderSet)
        {
            Log::warn() << LC << "Failed to create shader set!" << std::endl;
            return { };
        }

        // Make the pipeline configurator:
        auto pipelineConfig = vsg::GraphicsPipelineConfig::create(shaderSet);
        auto& defines = pipelineConfig->shaderHints->defines;

        // enable the arrays we think we need
        pipelineConfig->enableArray("in_vertex", VK_VERTEX_INPUT_RATE_VERTEX, 12);
        if (with_normals)
        {
            pipelineConfig->enableArray("in_normal", VK_VERTEX_INPUT_RATE_VERTEX, 12);
            defines.insert("HAS_IN_NORMAL");
        }
        if (with_tex_coords)
        {
            pipelineConfig->enableArray("in_uv", VK_VERTEX_INPUT_RATE_VERTEX, 8);
            defines.insert("HAS_IN_UV");
        }

        // activate the packed lights uniform
        vsg::Descriptors descriptors;
        if (auto& lightDataBinding = shaderSet->getUniformBinding("vsg_lights"))
        {
            auto data = lightDataBinding.data;
            if (!data) data = vsg::vec4Array::create(64);
            pipelineConfig->assignUniform(descriptors, "vsg_lights", data);
        }

        // packed lights are in a secondary DS so we must add that..
        vsg::ref_ptr<vsg::ViewDescriptorSetLayout> vdsl;
        if (sharedObjects)
            vdsl = sharedObjects->shared_default<vsg::ViewDescriptorSetLayout>();
        else
            vdsl = vsg::ViewDescriptorSetLayout::create();
        pipelineConfig->additionalDescriptorSetLayout = vdsl;

        // only render back faces
        pipelineConfig->rasterizationState->cullMode = VK_CULL_MODE_FRONT_BIT;

        // no depth testing
        pipelineConfig->depthStencilState->depthCompareOp = VK_COMPARE_OP_ALWAYS;

        // no depth writing
        pipelineConfig->depthStencilState->depthWriteEnable = VK_FALSE;

        // 1/1 blending:
        pipelineConfig->colorBlendState->attachments = vsg::ColorBlendState::ColorBlendAttachments{ {
            true,
            VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE, VK_BLEND_OP_ADD,
            VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE, VK_BLEND_OP_ADD,
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
        } };

        //pipelineConfig->rasterizationState->polygonMode = VK_POLYGON_MODE_LINE;

        // initialize
        if (sharedObjects)
            sharedObjects->share(pipelineConfig, [](auto gpc) { gpc->init(); });
        else
            pipelineConfig->init();

        // --- end pipelineconfig setup ---


        // set up the state group that will select the new pipeline:
        auto stategroup = vsg::StateGroup::create();

        // attach the pipeline:
        stategroup->add(pipelineConfig->bindGraphicsPipeline);

        // assign any custom ArrayState that may be required
        stategroup->prototypeArrayState = shaderSet->getSuitableArrayState(defines);

        // activate the descriptor set:
        auto bindViewDescriptorSets = vsg::BindViewDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineConfig->layout, 1);
        stategroup->add(bindViewDescriptorSets);

        return stategroup;
    }


    vsg::ref_ptr<vsg::Node> makeAtmosphere(const SRS& srs, float thickness, Runtime& runtime)
    {
        auto stategroup = makeStateGroup(false, false, runtime);
        if (!stategroup)
        {
            Log::warn() << LC << "Failed to make state group!" << std::endl;
            return { };
        }

        // attach the actual atmospheric geometry
        const bool with_texcoords = false;
        const bool with_normals = false;
        auto geometry = makeEllipsoid(srs, thickness, with_texcoords, with_normals);

        stategroup->addChild(geometry);

        return stategroup;
    }
}

SkyNode::SkyNode(const InstanceVSG& inst) :
    _instance(inst)
{
    setWorldSRS(SRS::ECEF);
}

void
SkyNode::setWorldSRS(const SRS& srs)
{
    if (srs.valid())
    {
        children.clear();

        // the sun:
        auto sun_data = rocky::Ephemeris().sunPosition(rocky::DateTime());
        _sun = new vsg::PointLight();
        _sun->name = "Sol";
        _sun->position = { sun_data.geocentric.x, sun_data.geocentric.y, sun_data.geocentric.z };
        _sun->color = { 1, 1, 1 };
        _sun->intensity = 1.0;
        addChild(_sun);

        // Tell the shaders that lighting is a go
        _instance.runtime().shaderCompileSettings->defines.insert("RK_LIGHTING");
        _instance.runtime().dirtyShaders();

        // the atmopshere:
        const float earth_atmos_thickness = 96560.0;
        _atmosphere = makeAtmosphere(srs, earth_atmos_thickness, _instance.runtime());
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
            _instance.runtime().shaderCompileSettings->defines.insert("RK_ATMOSPHERE");
            _instance.runtime().dirtyShaders();
        }
        else if (iter != children.end() && show == false)
        {
            children.erase(iter);

            // activate in shaders
            _instance.runtime().shaderCompileSettings->defines.erase("RK_ATMOSPHERE");
            _instance.runtime().dirtyShaders();
        }
    }
}
