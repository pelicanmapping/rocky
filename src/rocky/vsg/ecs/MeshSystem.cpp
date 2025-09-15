
/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "MeshSystem.h"
#include "../PipelineState.h"

#include <vsg/state/BindDescriptorSet.h>
#include <vsg/state/ViewDependentState.h>
#include <vsg/commands/DrawIndexed.h>

#include <vsg/nodes/CullNode.h>
#include <vsg/utils/ComputeBounds.h>

using namespace ROCKY_NAMESPACE;

#define MESH_VERT_SHADER "shaders/rocky.mesh.vert"
#define MESH_FRAG_SHADER "shaders/rocky.mesh.frag"

#define MESH_UNIFORM_SET 0 // must match layout(set=X) in the shader UBO
#define MESH_STYLE_BUFFER_BINDING 1 // must match the layout(binding=X) in the shader UBO (set=0)
#define MESH_TEXTURE_BINDING 6

namespace
{
    vsg::ref_ptr<vsg::ShaderSet> createShaderSet(VSGContext& context)
    {
        vsg::ref_ptr<vsg::ShaderSet> shaderSet;

        // load shaders
        auto vertexShader = vsg::ShaderStage::read(
            VK_SHADER_STAGE_VERTEX_BIT,
            "main",
            vsg::findFile(MESH_VERT_SHADER, context->searchPaths),
            context->readerWriterOptions);

        auto fragmentShader = vsg::ShaderStage::read(
            VK_SHADER_STAGE_FRAGMENT_BIT,
            "main",
            vsg::findFile(MESH_FRAG_SHADER, context->searchPaths),
            context->readerWriterOptions);

        if (!vertexShader || !fragmentShader)
        {
            return { };
        }

        vsg::ShaderStages shaderStages{ vertexShader, fragmentShader };

        shaderSet = vsg::ShaderSet::create(shaderStages);

        // "binding" (3rd param) must match "layout(location=X) in" in the vertex shader
        shaderSet->addAttributeBinding("in_vertex",      "", 0, VK_FORMAT_R32G32B32_SFLOAT, { });
        shaderSet->addAttributeBinding("in_normal",      "", 1, VK_FORMAT_R32G32B32_SFLOAT, {});
        shaderSet->addAttributeBinding("in_color",       "", 2, VK_FORMAT_R32G32B32A32_SFLOAT, {});
        shaderSet->addAttributeBinding("in_uv",          "", 3, VK_FORMAT_R32G32_SFLOAT, {});
        shaderSet->addAttributeBinding("in_depthoffset", "", 4, VK_FORMAT_R32_SFLOAT, {});

        // line data uniform buffer (width, stipple, etc.)
        shaderSet->addDescriptorBinding("mesh", "USE_MESH_STYLE",
            MESH_UNIFORM_SET, MESH_STYLE_BUFFER_BINDING,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, {});

        // Optional texture
        shaderSet->addDescriptorBinding("mesh_texture", "USE_MESH_TEXTURE",
            MESH_UNIFORM_SET, MESH_TEXTURE_BINDING,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, {});

        // Note: 128 is the maximum size required by the Vulkan spec so don't increase it
        shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT, 0, 128);

        return shaderSet;
    }
}


MeshSystemNode::MeshSystemNode(Registry& registry) :
    Inherit(registry)
{
    //nop
}


void
MeshSystemNode::initialize(VSGContext& context)
{
    auto shaderSet = createShaderSet(context);

    if (!shaderSet)
    {
        status = Failure(Failure::ResourceUnavailable,
            "Mesh shaders are missing or corrupt. "
            "Did you set ROCKY_FILE_PATH to point at the rocky share folder?");
        return;
    }

    pipelines.resize(NUM_PIPELINES);

    // create all pipeline permutations.
    for (int feature_mask = 0; feature_mask < NUM_PIPELINES; ++feature_mask)
    {
        auto& c = pipelines[feature_mask];

        // Create the pipeline configurator for terrain; this is a helper object
        // that acts as a "template" for terrain tile rendering state.
        c.config = vsg::GraphicsPipelineConfig::create(shaderSet);

        // Compile settings / defines. We need to clone this since it may be
        // different defines for each configuration permutation.
        c.config->shaderHints = context->shaderCompileSettings ?
            vsg::ShaderCompileSettings::create(*context->shaderCompileSettings) :
            vsg::ShaderCompileSettings::create();

        // activate the arrays we intend to use
        c.config->enableArray("in_vertex", VK_VERTEX_INPUT_RATE_VERTEX, 12);
        c.config->enableArray("in_normal", VK_VERTEX_INPUT_RATE_VERTEX, 12);
        c.config->enableArray("in_color", VK_VERTEX_INPUT_RATE_VERTEX, 16);
        c.config->enableArray("in_uv", VK_VERTEX_INPUT_RATE_VERTEX, 8);
        c.config->enableArray("in_depthoffset", VK_VERTEX_INPUT_RATE_VERTEX, 4);

        if (feature_mask & DYNAMIC_STYLE)
        {
            c.config->enableDescriptor("mesh");
            c.config->shaderHints->defines.insert("USE_MESH_STYLE");
        }

        if (feature_mask & TEXTURE)
        {
            c.config->enableTexture("mesh_texture");
            c.config->shaderHints->defines.insert("USE_MESH_TEXTURE");
        }
        
        struct SetPipelineStates : public vsg::Visitor
        {
            int feature_mask;
            SetPipelineStates(int feature_mask_) : feature_mask(feature_mask_) { }
            void apply(vsg::Object& object) override {
                object.traverse(*this);
            }
            void apply(vsg::RasterizationState& state) override {
                state.cullMode =
                    (feature_mask & CULL_BACKFACES) ? VK_CULL_MODE_BACK_BIT :
                    VK_CULL_MODE_NONE;
            }
            void apply(vsg::DepthStencilState& state) override {
                if ((feature_mask & WRITE_DEPTH) == 0) {
                    state.depthWriteEnable = VK_FALSE;
                }
            }
            void apply(vsg::ColorBlendState& state) override {
                state.attachments = vsg::ColorBlendState::ColorBlendAttachments{
                    { true,
                      VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD,
                      VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD,
                      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT }
                };
            }
        };
        SetPipelineStates visitor(feature_mask);
        c.config->accept(visitor);

        // Initialize GraphicsPipeline from the data in the configuration.
        c.config->init();

        c.commands = vsg::Commands::create();
        c.commands->addChild(c.config->bindGraphicsPipeline);
    }
}

void
MeshSystemNode::createOrUpdateNode(const Mesh& mesh, detail::BuildInfo& data, VSGContext& vsgcontext) const
{
    vsg::ref_ptr<vsg::StateGroup> stategroup;
    vsg::ref_ptr<BindMeshDescriptors> bindCommand;

    vsg::ref_ptr<vsg::ImageInfo> texImageInfo;
    if (mesh.texture != entt::null)
    {
        _registry.read([&](entt::registry& r)
            {
                auto* tex = r.try_get<Texture>(mesh.texture);
                if (tex)
                    texImageInfo = tex->imageInfo;
            });
    }

    if (mesh.style.has_value() || texImageInfo)
    {
        bindCommand = BindMeshDescriptors::create();
        if (texImageInfo)
            bindCommand->_imageInfo = texImageInfo;

        bindCommand->updateStyle(mesh.style.value());
        bindCommand->init(getPipelineLayout(mesh));

        stategroup = vsg::StateGroup::create();
        stategroup->stateCommands.push_back(bindCommand);
    }

    vsg::ref_ptr<vsg::Node> geometry_root;

    auto geometry = MeshGeometry::create();

    if (!mesh.triangles.empty())
    {
        geometry->reserve(mesh.triangles.size() * 3);
    }

    if (mesh.srs.valid() && mesh.triangles.size() > 0)
    {
        GeoPoint anchor(mesh.srs, mesh.triangles.front().verts[0]);
        SRSOperation xform;
        vsg::dvec3 offset;
        parseReferencePoint(anchor, xform, offset);

        vsg::dvec3 v0, v1, v2;
        vsg::vec3 v32[3];
        for (auto& tri : mesh.triangles)
        {
            xform(tri.verts[0], v0); v32[0] = v0 - offset;
            xform(tri.verts[1], v1); v32[1] = v1 - offset;
            xform(tri.verts[2], v2); v32[2] = v2 - offset;

            geometry->add(
                v32,
                reinterpret_cast<const vsg::vec2*>(tri.uvs),
                reinterpret_cast<const vsg::vec4*>(tri.colors),
                tri.depthoffsets);
        }

        auto localizer = vsg::MatrixTransform::create(vsg::translate(offset));
        localizer->addChild(geometry);
        geometry_root = localizer;
    }
    else
    {
        for (auto& tri : mesh.triangles)
        {
            geometry->add(
                reinterpret_cast<const vsg::dvec3*>(tri.verts),
                reinterpret_cast<const vsg::vec2*>(tri.uvs),
                reinterpret_cast<const vsg::vec4*>(tri.colors),
                tri.depthoffsets);
        }
        geometry_root = geometry;
    }

    // parent this mesh with a culling node
    auto cull = vsg::CullNode::create();    
    if (stategroup)
    {
        stategroup->addChild(geometry_root);
        cull->child = stategroup;
    }
    else
    {
        cull->child = geometry_root;
    }

    vsg::ComputeBounds cb;
    cull->child->accept(cb);
    cull->bound.set((cb.bounds.min + cb.bounds.max) * 0.5, vsg::length(cb.bounds.min - cb.bounds.max) * 0.5);

    data.new_node = cull;


    // check for updates to the style, and upload the new data if needed:
    if (!stategroup)
    {
        stategroup = util::find<vsg::StateGroup>(data.new_node ? data.new_node : data.existing_node);
        if (stategroup)
        {
            vsg::ModifiedCount mc;
            auto bindCommand = stategroup->stateCommands[0]->cast<BindMeshDescriptors>();
            if (bindCommand->_styleData->getModifiedCount(mc) && mc.count > 0)
            {
                vsgcontext->upload(bindCommand->_ubo->bufferInfoList);
            }
        }
    }
}

int
MeshSystemNode::featureMask(const Mesh& mesh) const
{
    int feature_set = 0;

    if (mesh.texture != entt::null) feature_set |= TEXTURE;
    if (mesh.style.has_value()) feature_set |= DYNAMIC_STYLE;
    if (mesh.writeDepth) feature_set |= WRITE_DEPTH;
    if (mesh.cullBackfaces) feature_set |= CULL_BACKFACES;
    return feature_set;
}


BindMeshDescriptors::BindMeshDescriptors()
{
    //nop
}

void
BindMeshDescriptors::updateStyle(const MeshStyle& value)
{
    if (!_styleData)
    {
        _styleData = vsg::ubyteArray::create(sizeof(MeshStyle));
        // do NOT mark as DYNAMIC_DATA, since we only update it when the style changes.
    }

    MeshStyle& my_style = *static_cast<MeshStyle*>(_styleData->dataPointer());
    my_style = value;
    _styleData->dirty();
}

void
BindMeshDescriptors::init(vsg::ref_ptr<vsg::PipelineLayout> layout)
{
    vsg::Descriptors descriptors;

    // the dynamic style buffer, if present:
    if (_styleData)
    {
        // the style buffer:
        _ubo = vsg::DescriptorBuffer::create(_styleData, MESH_STYLE_BUFFER_BINDING, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        descriptors.push_back(_ubo);
    }

    // the texture, if present:
    if (_imageInfo)
    {
        auto texture = vsg::DescriptorImage::create(_imageInfo, MESH_TEXTURE_BINDING, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        descriptors.push_back(texture);
    }

    if (!descriptors.empty())
    {
        this->pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        this->firstSet = 0;
        this->layout = layout;
        this->descriptorSet = vsg::DescriptorSet::create(
            layout->setLayouts.front(),
            descriptors);
    }
}


MeshGeometry::MeshGeometry()
{
    _drawCommand = vsg::DrawIndexed::create(
        0, // index count
        1, // instance count
        0, // first index
        0, // vertex offset
        0  // first instance
    );
}

void
MeshGeometry::reserve(size_t num_verts)
{
    _verts.reserve(num_verts);
    _normals.reserve(num_verts);
    _colors.reserve(num_verts);
    _uvs.reserve(num_verts);
    _depthoffsets.reserve(num_verts);
    _indices.reserve(num_verts);
}

void
MeshGeometry::add(
    const vsg::vec3* verts,
    const vsg::vec2* uvs,
    const vsg::vec4* colors,
    const float* depthoffsets)
{
    for (int v = 0; v < 3; ++v)
    {
        auto i = _verts.size();
        auto key = std::make_tuple(verts[v], colors[v]);
        auto iter1 = _lut.find(key);
        i = iter1 != _lut.end() ? iter1->second : _verts.size();

        if (i == _verts.size())
        {
            _verts.push_back(verts[v]);
            _uvs.push_back(uvs[v]);
            _colors.push_back(colors[v]);
            _depthoffsets.push_back(depthoffsets[v]);
            _lut[key] = (index_type)i;
        }

        _indices.push_back((index_type)i);
    }
}

void
MeshGeometry::compile(vsg::Context& context)
{
    if (commands.empty())
    {
        if (_verts.size() == 0)
            return;

        if (_normals.empty())
            _normals.assign(_verts.size(), vsg::vec3(0, 0, 1));

        auto vert_array = vsg::vec3Array::create(_verts.size(), _verts.data());
        auto normal_array = vsg::vec3Array::create(_normals.size(), _normals.data());
        auto color_array = vsg::vec4Array::create(_colors.size(), _colors.data());
        auto uv_array = vsg::vec2Array::create(_uvs.size(), _uvs.data());
        auto depthoffset_array = vsg::floatArray::create(_depthoffsets.size(), _depthoffsets.data());
        auto index_array = vsg::uintArray::create(_indices.size(), _indices.data());

        assignArrays({ vert_array, normal_array, color_array, uv_array, depthoffset_array });
        assignIndices(index_array);

        _drawCommand->indexCount = (uint32_t)index_array->size();

        commands.push_back(_drawCommand);
    }

    vsg::Geometry::compile(context);
}
