
/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#include "MeshSystem.h"
#include "../PipelineState.h"

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::detail;

#define MESH_VERT_SHADER "shaders/rocky.mesh.vert"
#define MESH_FRAG_SHADER "shaders/rocky.mesh.frag"

#define MESH_SET 0
#define MESH_BINDING_STYLE_LUT 0 // layout(set=0, binding=0) in the shader
#define MESH_BINDING_UNIFORMS  1 // layout(set=0, binding=1) in the shader
#define MESH_BINDING_TEXTURES  2 // layout(set=0, binding=2) in the shader

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
        //shaderSet->addAttributeBinding("in_depthoffset", "", 4, VK_FORMAT_R32_SFLOAT, {});

        shaderSet->addDescriptorBinding("styles", "", MESH_SET, MESH_BINDING_STYLE_LUT,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, {});

        shaderSet->addDescriptorBinding("mesh", "", MESH_SET, MESH_BINDING_UNIFORMS,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, {});

        shaderSet->addDescriptorBinding("meshTexture", "", MESH_SET, MESH_BINDING_TEXTURES,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_MESH_TEXTURES, VK_SHADER_STAGE_FRAGMENT_BIT, {});

        // Note: 128 is the maximum size required by the Vulkan spec so don't increase it
        shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT, 0, 128);

        return shaderSet;
    }
}

namespace
{
    void on_construct_Mesh(entt::registry& r, entt::entity e)
    {
        r.emplace<MeshDetail>(e);

        // TODO: put this in a utility function somewhere
        // common components that may already exist on this entity:
        (void)r.get_or_emplace<ActiveState>(e);
        (void)r.get_or_emplace<Visibility>(e);

        r.get<Mesh>(e).owner = e;
        r.get<Mesh>(e).dirty(r);
    }

    void on_construct_MeshStyle(entt::registry& r, entt::entity e)
    {
        r.emplace<MeshStyleDetail>(e);
        r.get<MeshStyle>(e).owner = e;
        r.get<MeshStyle>(e).dirty(r);
    }

    void on_construct_MeshGeometry(entt::registry& r, entt::entity e)
    {
        r.emplace<MeshGeometryDetail>(e);
        r.get<MeshGeometry>(e).owner = e;
        r.get<MeshGeometry>(e).dirty(r);
    }

    void on_construct_MeshTexture(entt::registry& r, entt::entity e)
    {
        auto& tex = r.get<MeshTexture>(e);
        tex.owner = e;
        tex.dirty(r);
    }


    void on_destroy_Mesh(entt::registry& r, entt::entity e)
    {
        r.remove<MeshDetail>(e);
    }

    void on_destroy_MeshStyle(entt::registry& r, entt::entity e)
    {
        r.remove<MeshStyleDetail>(e);
    }

    void on_destroy_MeshGeometry(entt::registry& r, entt::entity e)
    {
        r.remove<MeshGeometryDetail>(e);
    }

    void on_destroy_MeshTexture(entt::registry& r, entt::entity e)
    {
        // nop - todo: remove it from the texture atlas?
    }
}


MeshSystemNode::MeshSystemNode(Registry& registry) :
    Inherit(registry)
{
    _styleInUse.fill(false);
    _textureInUse.fill(false);
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

    _pipelines.resize(NUM_PIPELINES);

    // create all pipeline permutations.
    for (int feature_mask = 0; feature_mask < NUM_PIPELINES; ++feature_mask)
    {
        auto& c = _pipelines[feature_mask];

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

        //if (feature_mask & DYNAMIC_STYLE)
        //{
        //    c.config->enableDescriptor("mesh");
        //    c.config->shaderHints->defines.insert("USE_MESH_STYLE");
        //}

        //if (feature_mask & TEXTURE)
        //{
        //    c.config->enableTexture("mesh_texture");
        //    c.config->shaderHints->defines.insert("USE_MESH_TEXTURE");
        //}
        
        struct SetPipelineStates : public vsg::Visitor
        {
            int feature_mask;
            SetPipelineStates(int feature_mask_) : feature_mask(feature_mask_) { }
            void apply(vsg::Object& object) override {
                object.traverse(*this);
            }
            //void apply(vsg::RasterizationState& state) override {
            //    state.cullMode =
            //        (feature_mask & CULL_BACKFACES) ? VK_CULL_MODE_BACK_BIT :
            //        VK_CULL_MODE_NONE;
            //}
            //void apply(vsg::DepthStencilState& state) override {
            //    if ((feature_mask & WRITE_DEPTH) == 0) {
            //        state.depthWriteEnable = VK_FALSE;
            //    }
            //}
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

    // Style look up table.
    _styleLUT_data = vsg::ubyteArray::create(sizeof(MeshStyleLUT));
    _styleLUT_buffer = vsg::DescriptorBuffer::create(_styleLUT_data, MESH_BINDING_STYLE_LUT, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

    // add a default style in slot 0.
    auto& styleLUT = *static_cast<MeshStyleLUT*>(_styleLUT_data->dataPointer());
    styleLUT.lut[0].populate(MeshStyle());
    _styleInUse[0] = true;
    _styleLUTSize = 1;

    // Texture arena
    _textureBuffer = vsg::DescriptorImage::create(
        vsg::ImageInfoList{},
        MESH_BINDING_TEXTURES, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    _registry.write([&](entt::registry& r)
        {
            // install the ecs callbacks
            r.on_construct<Mesh>().connect<&on_construct_Mesh>();
            r.on_construct<MeshStyle>().connect<&on_construct_MeshStyle>();
            r.on_construct<MeshGeometry>().connect<&on_construct_MeshGeometry>();
            r.on_construct<MeshTexture>().connect<&on_construct_MeshTexture>();

            r.on_destroy<Mesh>().connect<&on_destroy_Mesh>();
            r.on_destroy<MeshStyle>().connect<&on_destroy_MeshStyle>();
            r.on_destroy<MeshGeometry>().connect<&on_destroy_MeshGeometry>();
            r.on_destroy<MeshTexture>().connect<&on_destroy_MeshTexture>();

            // Set up the dirty tracking.
            auto e = r.create();
            r.emplace<Mesh::Dirty>(e);
            r.emplace<MeshStyle::Dirty>(e);
            r.emplace<MeshGeometry::Dirty>(e);
            r.emplace<MeshTexture::Dirty>(e);
        });
}

void
MeshSystemNode::createOrUpdateComponent(const Mesh& mesh, MeshDetail& meshDetail,
    MeshStyleDetail* styleDetail, MeshGeometryDetail* geomDetail, VSGContext& vsgcontext)
{
    auto layout = getPipelineLayout(mesh);

    bool isNew = !meshDetail.node;

    if (isNew)
    {
        auto bind = BindMeshDescriptors::create();

        bind->_uniformsData = vsg::ubyteArray::create(sizeof(MeshUniforms));
        bind->_uniformsBuffer = vsg::DescriptorBuffer::create(bind->_uniformsData, MESH_BINDING_UNIFORMS, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

        bind->pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        bind->firstSet = 0;
        bind->layout = layout;
        bind->descriptorSet = vsg::DescriptorSet::create(layout->setLayouts.front(),
            vsg::Descriptors{ _styleLUT_buffer, _textureBuffer, bind->_uniformsBuffer });

        meshDetail.node = vsg::StateGroup::create();
        meshDetail.node->stateCommands.emplace_back(bind);
        meshDetail.bind = bind;

        compile(meshDetail.node);
    }

    ROCKY_SOFT_ASSERT_AND_RETURN(meshDetail.node, void());

    // remove the children so we can rebuild the graph.
    meshDetail.node->children.clear();

    vsg::Group* parent = meshDetail.node;

    auto& uniforms = *static_cast<MeshUniforms*>(meshDetail.bind->_uniformsData->dataPointer());
    auto styleIndex = styleDetail ? styleDetail->index : -1;
    if (isNew || styleIndex != uniforms.styleIndex)
    {
        uniforms.styleIndex = styleIndex;
        upload(meshDetail.bind->_uniformsBuffer->bufferInfoList);
    }

    if (geomDetail)
    {
        ROCKY_SOFT_ASSERT_AND_RETURN(geomDetail->node, void(), "node is missing");
        parent->addChild(geomDetail->node);
    }
}

void
MeshSystemNode::createOrUpdateGeometry(const MeshGeometry& geom, MeshGeometryDetail& geomDetail, VSGContext& vsgcontext)
{
    // NB: registry is read-locked

    if (geomDetail.geomNode)
    {
        vsgcontext->dispose(geomDetail.geomNode);
    }

    geomDetail.geomNode = MeshGeometryNode::create();

    vsg::ref_ptr<vsg::Node> root;
    vsg::dmat4 localizer_matrix;

    if (geom.srs.valid())
    {
        if (geom.verts.size() > 0)
        {
            GeoPoint anchor(geom.srs, geom.verts.front());
            auto [xform, offset] = anchor.parseAsReferencePoint();

            // transform and localize:
            std::vector<glm::dvec3> verts(geom.verts); // copy
            xform.transformRange(verts.begin(), verts.end());
            for (auto& v : verts) v -= offset;

            auto& gn = geomDetail.geomNode;

            gn->_verts.resize(verts.size());
            std::transform(verts.begin(), verts.end(), gn->_verts.begin(),
                [](const glm::dvec3& v) { return to_vsg(v); });

            gn->_colors.resize(geom.colors.size());
            std::transform(geom.colors.begin(), geom.colors.end(), gn->_colors.begin(),
                [](const glm::fvec4& c) { return to_vsg(c); });

            gn->_uvs.resize(geom.uvs.size());
            std::transform(geom.uvs.begin(), geom.uvs.end(), gn->_uvs.begin(),
                [](const glm::fvec2& uv) { return to_vsg(uv); });

            gn->_indices = geom.indices;

            auto localizer = vsg::MatrixTransform::create(vsg::translate(to_vsg(offset)));
            localizer->addChild(geomDetail.geomNode);
            root = localizer;
        }

        else if (geom.triangles.size() > 0)
        {
            GeoPoint anchor(geom.srs, geom.triangles.front().verts[0]);
            auto [xform, offset] = anchor.parseAsReferencePoint();

            geomDetail.geomNode->reserve(geom.triangles.size() * 3);

            vsg::dvec3 v0, v1, v2;
            vsg::vec3 v32[3];
            for (auto& tri : geom.triangles)
            {
                xform(tri.verts[0], v0); v32[0] = v0 - to_vsg(offset);
                xform(tri.verts[1], v1); v32[1] = v1 - to_vsg(offset);
                xform(tri.verts[2], v2); v32[2] = v2 - to_vsg(offset);

                geomDetail.geomNode->addTriangle(
                    v32,
                    reinterpret_cast<const vsg::vec2*>(tri.uvs),
                    reinterpret_cast<const vsg::vec4*>(tri.colors));
            }

            auto localizer = vsg::MatrixTransform::create(vsg::translate(to_vsg(offset)));
            localizer->addChild(geomDetail.geomNode);
            root = localizer;
        }
    }
    else
    {
        if (geom.verts.size() > 0)
        {
            auto& gn = geomDetail.geomNode;

            gn->_verts.resize(geom.verts.size());
            std::transform(geom.verts.begin(), geom.verts.end(), gn->_verts.begin(),
                [](const glm::dvec3& v) { return to_vsg(v); });

            gn->_colors.resize(geom.colors.size());
            std::transform(geom.colors.begin(), geom.colors.end(), gn->_colors.begin(),
                [](const glm::fvec4& c) { return to_vsg(c); });

            gn->_uvs.resize(geom.uvs.size());
            std::transform(geom.uvs.begin(), geom.uvs.end(), gn->_uvs.begin(),
                [](const glm::fvec2& uv) { return to_vsg(uv); });

            gn->_indices = geom.indices;
        }

        else if (geom.triangles.size() > 0)
        {
            for (auto& tri : geom.triangles)
            {
                geomDetail.geomNode->addTriangle(
                    reinterpret_cast<const vsg::dvec3*>(tri.verts),
                    reinterpret_cast<const vsg::vec2*>(tri.uvs),
                    reinterpret_cast<const vsg::vec4*>(tri.colors));
            }
        }

        root = geomDetail.geomNode;
    }

    if (!geomDetail.cullNode)
    {
        geomDetail.cullNode = vsg::CullNode::create();
    }

    geomDetail.cullNode->child = root;

    vsg::ComputeBounds cb;
    geomDetail.cullNode->child->accept(cb);
    geomDetail.cullNode->bound.set((cb.bounds.min + cb.bounds.max) * 0.5, vsg::length(cb.bounds.min - cb.bounds.max) * 0.5);

    geomDetail.node = geomDetail.cullNode;

    compile(geomDetail.node);
}


void
MeshSystemNode::createOrUpdateStyle(const MeshStyle& style, MeshStyleDetail& styleDetail, entt::registry& reg)
{
    auto& styleLUT = *static_cast<MeshStyleLUT*>(_styleLUT_data->dataPointer());
    bool reassignTextures = false;

    if (styleDetail.index >= 0)
    {
        auto& entry = styleLUT.lut[styleDetail.index];
        entry.populate(style);
    }
    else
    {
        ROCKY_SOFT_ASSERT_AND_RETURN(_styleLUTSize <= MAX_MESH_STYLES, void(),
            "Line style LUT overflow - call support");

        for (unsigned i = 0; i < MAX_MESH_STYLES; ++i)
        {
            if (!_styleInUse[i])
            {
                _styleInUse[i] = true;
                styleLUT.lut[i].populate(style);
                styleDetail.index = i;
                _styleLUTSize = std::max(_styleLUTSize, i + 1);
                break;
            }
        }
    }

    auto& entry = styleLUT.lut[styleDetail.index];

    if (style.texture == entt::null)
    {
        entry.textureIndex = -1;
    }

    else if (style.texture != styleDetail.texture || entry.textureIndex < 0)
    {
        entry.textureIndex = -1;
        auto& tex = reg.get<MeshTexture>(style.texture);
        for (unsigned i = 0; i < _textureBuffer->imageInfoList.size(); ++i)
        {
            if (tex.imageInfo == _textureBuffer->imageInfoList[i])
            {
                entry.textureIndex = i;
                break;
            }
        }
    }

    styleDetail.texture = style.texture;

    upload(_styleLUT_buffer->bufferInfoList);
}

void
MeshSystemNode::addOrUpdateTexture(const MeshTexture& tex)
{
    if (tex._index < 0 && tex.imageInfo)
    {
        auto i = _textureArenaSize++;
        _textureBuffer->imageInfoList.resize(_textureArenaSize);
        _textureBuffer->imageInfoList[i] = tex.imageInfo;
        // don't compile yet... there may be others
    }
}


int
MeshSystemNode::featureMask(const Mesh& mesh) const
{
    int feature_set = 0;

    //if (mesh.texture != entt::null) feature_set |= TEXTURE;
    //if (mesh.style.has_value()) feature_set |= DYNAMIC_STYLE;
    //if (mesh.writeDepth) feature_set |= WRITE_DEPTH;
    //if (mesh.cullBackfaces) feature_set |= CULL_BACKFACES;
    return feature_set;
}

void
MeshSystemNode::traverse(vsg::RecordTraversal& record) const
{
    const vsg::dmat4 identity_matrix = vsg::dmat4(1.0);

    detail::RenderingState rs{
        record.getCommandBuffer()->viewID,
        record.getFrameStamp()->frameCount
    };

    // Collect render leaves while locking the registry
    _registry.read([&](entt::registry& reg)
        {
            auto view = reg.view<Mesh, MeshDetail, ActiveState, Visibility>();
            for (auto&& [entity, comp, compDetail, active, visibility] : view.each())
            {
                if (compDetail.node && visible(visibility, rs))
                {
                    auto* transformDetail = reg.try_get<TransformDetail>(entity);
                    if (transformDetail)
                    {
                        if (transformDetail->passingCull(rs))
                        {
                            _renderLeaves.emplace_back(RenderLeaf{ compDetail.node, transformDetail });
                        }
                    }
                    else
                    {
                        _renderLeaves.emplace_back(RenderLeaf{ compDetail.node, nullptr });
                    }
                }
            }
        });

    // Render collected data
    if (!_renderLeaves.empty())
    {
        _pipelines[0].commands->accept(record);

        for (auto& leaf : _renderLeaves)
        {
            if (leaf.xformDetail)
            {
                leaf.xformDetail->push(record);
            }

            leaf.node->accept(record);

            if (leaf.xformDetail)
            {
                leaf.xformDetail->pop(record);
            }
        }
    }

    _renderLeaves.clear();
}

void
MeshSystemNode::update(VSGContext& vsgcontext)
{
    bool uploadStyles = false;

    auto numTextures = _textureArenaSize;

    _registry.read([&](entt::registry& reg)
        {
            MeshTexture::eachDirty(reg, [&](entt::entity e)
                {
                    auto& tex = reg.get<MeshTexture>(e);
                    addOrUpdateTexture(tex);
                });

            MeshStyle::eachDirty(reg, [&](entt::entity e)
                {
                    auto& [style, styleDetail] = reg.get<MeshStyle, MeshStyleDetail>(e);
                    createOrUpdateStyle(style, styleDetail, reg);
                    uploadStyles = true;
                });

            MeshGeometry::eachDirty(reg, [&](entt::entity e)
                {
                    auto& [geom, geomDetail] = reg.get<MeshGeometry, MeshGeometryDetail>(e);
                    createOrUpdateGeometry(geom, geomDetail, vsgcontext);
                });

            Mesh::eachDirty(reg, [&](entt::entity e)
                {
                    auto& [comp, compDetail] = reg.get<Mesh, MeshDetail>(e);
                    auto* styleDetail = comp.style != entt::null ? reg.try_get<MeshStyleDetail>(comp.style) : nullptr;
                    auto* geomDetail = comp.geometry != entt::null ? reg.try_get<MeshGeometryDetail>(comp.geometry) : nullptr;
                    createOrUpdateComponent(comp, compDetail, styleDetail, geomDetail, vsgcontext);
                });
        });

    if (uploadStyles)
    {
        upload(_styleLUT_buffer->bufferInfoList);
    }

    if (numTextures < _textureArenaSize)
    {
        compile(_textureBuffer);
    }

    Inherit::update(vsgcontext);
}


MeshGeometryNode::MeshGeometryNode()
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
MeshGeometryNode::reserve(size_t num_verts)
{
    _verts.reserve(num_verts);
    _normals.reserve(num_verts);
    _colors.reserve(num_verts);
    _uvs.reserve(num_verts);
    _indices.reserve(num_verts);
}

void
MeshGeometryNode::addTriangle(const vsg::vec3* verts, const vsg::vec2* uvs, const vsg::vec4* colors)
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
            _lut[key] = (index_type)i;
        }

        _indices.push_back((index_type)i);
    }
}

void
MeshGeometryNode::compile(vsg::Context& context)
{
    if (commands.empty())
    {
        if (_verts.size() == 0)
            return;

        if (_normals.empty())
            _normals.assign(_verts.size(), vsg::vec3(0, 0, 1));

        if (_colors.empty())
            _colors.assign(_verts.size(), _defaultColor);

        if (_uvs.empty())
            _uvs.assign(_verts.size(), vsg::vec2(0, 0));

        auto vert_array = vsg::vec3Array::create(_verts.size(), _verts.data());
        auto normal_array = vsg::vec3Array::create(_normals.size(), _normals.data());
        auto color_array = vsg::vec4Array::create(_colors.size(), _colors.data());
        auto uv_array = vsg::vec2Array::create(_uvs.size(), _uvs.data());
        auto index_array = vsg::uintArray::create(_indices.size(), _indices.data());

        assignArrays({ vert_array, normal_array, color_array, uv_array });
        assignIndices(index_array);

        _drawCommand->indexCount = (uint32_t)index_array->size();

        commands.push_back(_drawCommand);
    }

    vsg::Geometry::compile(context);
}
