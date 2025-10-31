
/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#include "MeshSystem.h"
#include "../PipelineState.h"
#include "../Polyfill.h"

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::detail;

#define MESH_VERT_SHADER "shaders/rocky.mesh.vert"
#define MESH_FRAG_SHADER "shaders/rocky.mesh.frag"

#define MESH_SET 0
//#define MESH_BINDING_STYLE_LUT 0 // layout(set=0, binding=0) in the shader
#define MESH_BINDING_UNIFORM   1 // layout(set=0, binding=1) in the shader
#define MESH_BINDING_TEXTURE   2 // layout(set=0, binding=2) in the shader

#define USE_DYNAMIC_STATE

namespace
{
    inline vsg::ref_ptr<vsg::ImageInfo> createEmptyTexture()
    {
        auto sampler = vsg::Sampler::create();
        auto image = Image::create(Image::R8_UNORM, 1, 1);
        return vsg::ImageInfo::create(sampler, util::moveImageToVSG(image));
    }

    vsg::ref_ptr<vsg::ShaderSet> createShaderSet(VSGContext& vsgcontext)
    {
        vsg::ref_ptr<vsg::ShaderSet> shaderSet;

        // load shaders
        auto vertexShader = vsg::ShaderStage::read(
            VK_SHADER_STAGE_VERTEX_BIT,
            "main",
            vsg::findFile(MESH_VERT_SHADER, vsgcontext->searchPaths),
            vsgcontext->readerWriterOptions);

        auto fragmentShader = vsg::ShaderStage::read(
            VK_SHADER_STAGE_FRAGMENT_BIT,
            "main",
            vsg::findFile(MESH_FRAG_SHADER, vsgcontext->searchPaths),
            vsgcontext->readerWriterOptions);

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

        shaderSet->addDescriptorBinding("mesh", "", MESH_SET, MESH_BINDING_UNIFORM,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, {});

        shaderSet->addDescriptorBinding("meshTexture", "", MESH_SET, MESH_BINDING_TEXTURE,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, {});

        // We need VSG's view-dependent data for lighting support
        PipelineUtils::addViewDependentData(shaderSet, VK_SHADER_STAGE_FRAGMENT_BIT);

        // Note: 128 is the maximum size required by the Vulkan spec so don't increase it
        shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT, 0, 128);

        return shaderSet;
    }

    // creates an empty, default style detail bind command, ready to be populated.
    void initializeStyleDetail(vsg::PipelineLayout* layout, MeshStyleDetail& styleDetail)
    {
        // uniform: "mesh.styles" in the shader
        styleDetail.styleUBOData = vsg::ubyteArray::create(sizeof(MeshStyleUniform));
        styleDetail.styleUBO = vsg::DescriptorBuffer::create(styleDetail.styleUBOData, MESH_BINDING_UNIFORM, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

        // uniform: "meshTexture" in the fragment shader
        styleDetail.styleTexture = vsg::DescriptorImage::create(createEmptyTexture(), MESH_BINDING_TEXTURE, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

        // bind command:
        styleDetail.bind = vsg::BindDescriptorSet::create();
        styleDetail.bind->pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        styleDetail.bind->firstSet = 0;
        styleDetail.bind->layout = layout;
        styleDetail.bind->descriptorSet = vsg::DescriptorSet::create(
            styleDetail.bind->layout->setLayouts.front(),
            vsg::Descriptors{ styleDetail.styleUBO, styleDetail.styleTexture });

        MeshStyleUniform& uniforms = *static_cast<MeshStyleUniform*>(styleDetail.styleUBOData->dataPointer());
        uniforms.style = MeshStyleRecord(); // default style
    }
}

namespace
{
    // disposal vector processed by the system
    static std::mutex s_cleanupMutex;
    static vsg::ref_ptr<vsg::Objects> s_toDispose = vsg::Objects::create();
    static inline void dispose(vsg::Object* object) {
        if (object) {
            std::scoped_lock lock(s_cleanupMutex);
            s_toDispose->addChild(vsg::ref_ptr<vsg::Object>(object));
        }
    }

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
    void on_construct_Texture(entt::registry& r, entt::entity e)
    {
        (void)r.get_or_emplace<MeshTextureDetail>(e);
        auto& tex = r.get<MeshTexture>(e);
        tex.owner = e;
        tex.dirty(r);
    }


    void on_destroy_MeshDetail(entt::registry& r, entt::entity e)
    {
        auto& d = r.get<MeshDetail>(e);
        d = MeshDetail();
    }
    void on_destroy_MeshStyleDetail(entt::registry& r, entt::entity e)
    {
        auto& d = r.get<MeshStyleDetail>(e);
        dispose(d.bind);
        for(auto& pass : d.passes)
            dispose(pass);
        d = MeshStyleDetail();
    }
    void on_destroy_MeshGeometryDetail(entt::registry& r, entt::entity e)
    {
        auto& d = r.get<MeshGeometryDetail>(e);
        dispose(d.node);
        d = MeshGeometryDetail();
    }
    void on_destroy_MeshTextureDetail(entt::registry& r, entt::entity e)
    {
        auto& d = r.get<MeshTextureDetail>(e);     
    }


    void on_update_Mesh(entt::registry& r, entt::entity e)
    {
        on_destroy_MeshDetail(r, e);
        r.get<Mesh>(e).dirty(r);
    }
    void on_update_MeshStyle(entt::registry& r, entt::entity e)
    {
        on_destroy_MeshStyleDetail(r, e);
        r.get<MeshStyle>(e).dirty(r);
    }
    void on_update_MeshGeometry(entt::registry& r, entt::entity e)
    {
        on_destroy_MeshGeometryDetail(r, e);
        r.get<MeshGeometry>(e).dirty(r);
    }
    void on_update_Texture(entt::registry& r, entt::entity e)
    {
        auto& d = r.get<MeshTextureDetail>(e);
        d = MeshTextureDetail();
        r.get<MeshTexture>(e).dirty(r);
    }
}


MeshSystemNode::MeshSystemNode(Registry& registry) :
    Inherit(registry)
{
    // temporary transform used by the visitor traversal(s)
    _tempMT = vsg::MatrixTransform::create();
    _tempMT->children.resize(1);
}


void
MeshSystemNode::initialize(VSGContext& vsgcontext)
{
    // make sure all required vulkan features are available:
    auto pd = vsgcontext->device()->getPhysicalDevice();

    if (!pd->supportsDeviceExtension(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME) ||
        !pd->supportsDeviceExtension(VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME) ||
        !pd->supportsDeviceExtension(VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME))
    {
        status = Failure(Failure::ResourceUnavailable,
            "MeshSystem requires the Vulkan extended dynamic state extensions. "
            "These are not supported by the current Vulkan device.");
        Log()->warn("MeshSystem not availabe! {}", status.error().message);
        return;
    }

    auto shaderSet = createShaderSet(vsgcontext);

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
        c.config->shaderHints = vsgcontext->shaderCompileSettings ?
            vsg::ShaderCompileSettings::create(*vsgcontext->shaderCompileSettings) :
            vsg::ShaderCompileSettings::create();

        // activate the arrays we intend to use
        c.config->enableArray("in_vertex", VK_VERTEX_INPUT_RATE_VERTEX, 12);
        c.config->enableArray("in_normal", VK_VERTEX_INPUT_RATE_VERTEX, 12);
        c.config->enableArray("in_color", VK_VERTEX_INPUT_RATE_VERTEX, 16);
        c.config->enableArray("in_uv", VK_VERTEX_INPUT_RATE_VERTEX, 8);

        PipelineUtils::enableViewDependentData(c.config);
        
        struct SetPipelineStates : public vsg::Visitor
        {
            int feature_mask;
            SetPipelineStates(int feature_mask_) : feature_mask(feature_mask_) { }
            void apply(vsg::Object& object) override {
                object.traverse(*this);
            }
            void apply(vsg::RasterizationState& state) override {
                state.polygonMode = VK_POLYGON_MODE_FILL;
                state.cullMode = VK_CULL_MODE_BACK_BIT;
            }
            void apply(vsg::DepthStencilState& state) override {
                state.depthWriteEnable = VK_TRUE;
            }
            void apply(vsg::ColorBlendState& state) override {
                state.attachments = vsg::ColorBlendState::ColorBlendAttachments{
                    { true,
                      VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD,
                      VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD,
                      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT }
                };
            }
#ifdef USE_DYNAMIC_STATE
            void apply(vsg::DynamicState& state) override {
                state.dynamicStates.emplace_back(VK_DYNAMIC_STATE_POLYGON_MODE_EXT);
                state.dynamicStates.emplace_back(VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE_EXT);
                state.dynamicStates.emplace_back(VK_DYNAMIC_STATE_CULL_MODE_EXT);
                state.dynamicStates.emplace_back(VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT);
            }
#endif
        };

#ifdef USE_DYNAMIC_STATE
        c.config->pipelineStates.emplace_back(vsg::DynamicState::create());
#endif

        SetPipelineStates visitor(feature_mask);
        c.config->accept(visitor);

        // Initialize GraphicsPipeline from the data in the configuration.
        c.config->init();

        c.commands = vsg::Commands::create();
        c.commands->addChild(c.config->bindGraphicsPipeline);
        c.commands->addChild(vsg::BindViewDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, c.config->layout, VSG_VIEW_DEPENDENT_DESCRIPTOR_SET_INDEX));
    }

    // Set up our default style detail, which is used when a MeshStyle is missing.
    initializeStyleDetail(getPipelineLayout(Mesh()), _defaultMeshStyleDetail);
    compile(_defaultMeshStyleDetail.bind);

    _registry.write([&](entt::registry& r)
        {
            // install the ENTT callbacks for managing internal data:
            r.on_construct<Mesh>().connect<&on_construct_Mesh>();
            r.on_construct<MeshStyle>().connect<&on_construct_MeshStyle>();
            r.on_construct<MeshGeometry>().connect<&on_construct_MeshGeometry>();
            r.on_construct<MeshTexture>().connect<&on_construct_Texture>();

            r.on_update<Mesh>().connect<&on_update_Mesh>();
            r.on_update<MeshStyle>().connect<&on_update_MeshStyle>();
            r.on_update<MeshGeometry>().connect<&on_update_MeshGeometry>();
            r.on_update<MeshTexture>().connect<&on_update_Texture>();

            r.on_destroy<MeshDetail>().connect<&on_destroy_MeshDetail>();
            r.on_destroy<MeshStyleDetail>().connect<&on_destroy_MeshStyleDetail>();
            r.on_destroy<MeshGeometryDetail>().connect<&on_destroy_MeshGeometryDetail>();
            r.on_destroy<MeshTextureDetail>().connect<&on_destroy_MeshTextureDetail>();

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
    // NB: registry is read-locked
    if (geomDetail)
    {
        meshDetail.node = geomDetail->node;
    }
}

void
MeshSystemNode::createOrUpdateGeometry(const MeshGeometry& geom, MeshGeometryDetail& geomDetail, VSGContext& vsgcontext)
{
    // NB: registry is read-locked

    if (geomDetail.node)
    {
        vsgcontext->dispose(geomDetail.node);
    }

    geomDetail.node = MeshGeometryNode::create();

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

            auto& gn = geomDetail.node;

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
            localizer->addChild(geomDetail.node);
            root = localizer;
        }

        else if (geom.triangles.size() > 0)
        {
            GeoPoint anchor(geom.srs, geom.triangles.front().verts[0]);
            auto [xform, offset] = anchor.parseAsReferencePoint();

            geomDetail.node->reserve(geom.triangles.size() * 3);

            vsg::dvec3 v0, v1, v2;
            vsg::vec3 v32[3];
            for (auto& tri : geom.triangles)
            {
                xform(tri.verts[0], v0); v32[0] = v0 - to_vsg(offset);
                xform(tri.verts[1], v1); v32[1] = v1 - to_vsg(offset);
                xform(tri.verts[2], v2); v32[2] = v2 - to_vsg(offset);

                geomDetail.node->addTriangle(
                    v32,
                    reinterpret_cast<const vsg::vec2*>(tri.uvs),
                    reinterpret_cast<const vsg::vec4*>(tri.colors));
            }

            auto localizer = vsg::MatrixTransform::create(vsg::translate(to_vsg(offset)));
            localizer->addChild(geomDetail.node);
            root = localizer;
        }
    }
    else
    {
        if (geom.verts.size() > 0)
        {
            auto& gn = geomDetail.node;

            gn->_verts.resize(geom.verts.size());
            std::transform(geom.verts.begin(), geom.verts.end(), gn->_verts.begin(),
                [](const glm::dvec3& v) { return to_vsg(v); });

            gn->_normals.resize(geom.normals.size());
            std::transform(geom.normals.begin(), geom.normals.end(), gn->_normals.begin(),
                [](const glm::fvec3& n) { return to_vsg(n); });

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
                geomDetail.node->addTriangle(
                    reinterpret_cast<const vsg::dvec3*>(tri.verts),
                    reinterpret_cast<const vsg::vec2*>(tri.uvs),
                    reinterpret_cast<const vsg::vec4*>(tri.colors));
            }
        }

        root = geomDetail.node;
    }

    compile(geomDetail.node);
}


void
MeshSystemNode::createOrUpdateStyle(const MeshStyle& style, MeshStyleDetail& styleDetail, entt::registry& reg, VSGContext& vsgcontext)
{
    // NB: registry is read-locked
    bool needsCompile = false;
    bool needsUpload = false;

    if (!styleDetail.bind)
    {
        auto layout = getPipelineLayout(Mesh());
        initializeStyleDetail(layout, styleDetail);
        needsCompile = true;
    }

#ifdef USE_DYNAMIC_STATE
    for (auto& pass : styleDetail.passes)
        dispose(pass);

    styleDetail.passes.clear();
    styleDetail.passes.emplace_back(vsg::Commands::create());
    styleDetail.passes[0]->addChild(styleDetail.bind);

    // wireframe:
    styleDetail.passes[0]->addChild(SetPolygonMode::create(vsgcontext->device(),
        style.wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL));

    // cull mode
    styleDetail.passes[0]->addChild(SetCullMode::create(vsgcontext->device(),
        style.drawBackfaces ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT));

    if (style.twoPassAlpha == true && style.writeDepth == false)
    {
        Log()->warn("MeshStyle: twoPassAlpha requires writeDepth to be true; ignoring twoPassAlpha.");
    }

    // when both 2-pass alpha AND writeDepth are enabled, create a second pass.
    // render all objects first with no depth-writes; then write the depth 
    // later in a second pass.
    if (style.twoPassAlpha && style.writeDepth)
    {
        styleDetail.passes.emplace_back(vsg::Commands::create());

        // first pass: no depth writes, full color writes:
        styleDetail.passes[0]->addChild(SetDepthWriteEnable::create(vsgcontext->device(), VK_FALSE));
        styleDetail.passes[0]->addChild(SetColorWriteMask::create(vsgcontext->device(), 0x0F));

        // second pass: depth writes, no color writes:
        styleDetail.passes[1]->addChild(SetDepthWriteEnable::create(vsgcontext->device(), VK_TRUE));
        styleDetail.passes[1]->addChild(SetColorWriteMask::create(vsgcontext->device(), 0x0));
    }
    else
    {
        // depth writes, default color mask
        styleDetail.passes[0]->addChild(SetDepthWriteEnable::create(vsgcontext->device(), style.writeDepth ? VK_TRUE : VK_FALSE));
        styleDetail.passes[0]->addChild(SetColorWriteMask::create(vsgcontext->device(), 0x0F));
    }
#endif

    bool texChanged = style.texture != styleDetail.texture;

    // update the uniform for this style:
    MeshStyleUniform& uniforms = *static_cast<MeshStyleUniform*>(styleDetail.styleUBOData->dataPointer());
    uniforms.style.populate(style);
    needsUpload = !needsCompile;

    if (texChanged)
    {
        // texture changed
        auto* tex = reg.try_get<MeshTexture>(style.texture);
        if (tex)
        {
            // properly dispose of old texture
            if (styleDetail.styleTexture->imageInfoList.size() > 0 &&
                styleDetail.styleTexture->imageInfoList[0].valid())
            {
                dispose(styleDetail.styleTexture->imageInfoList[0]);
            }

            styleDetail.styleTexture->imageInfoList = vsg::ImageInfoList{ tex->imageInfo };
            uniforms.style.hasTexture = 1;
            needsCompile = true;
        }
    }

    if (needsCompile)
        compile(styleDetail.bind);
    else if (needsUpload)
        upload(styleDetail.styleUBO->bufferInfoList);
}

void
MeshSystemNode::addOrUpdateTexture(const MeshTexture& tex, MeshTextureDetail& texDetail, entt::registry& reg)
{
    reg.view<MeshStyleDetail>().each([&](auto& styleDetail)
        {
            if (styleDetail.texture == tex.owner)
            {
                // dispose of old one
                if (styleDetail.styleTexture->imageInfoList.size() > 0 &&
                    styleDetail.styleTexture->imageInfoList[0].valid())
                {
                    dispose(styleDetail.styleTexture->imageInfoList[0]);
                }

                styleDetail.styleTexture->imageInfoList = vsg::ImageInfoList{ tex.imageInfo };
                compile(styleDetail.bind);
            }
        });
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
    if (status.failed()) return;

    RenderingState rs{
        record.getCommandBuffer()->viewID,
        record.getFrameStamp()->frameCount
    };

    _styleDetailBins.clear();
    _styleDetailBins.emplace_back(&_defaultMeshStyleDetail);
    // Collect render leaves while locking the registry
    _registry.read([&](entt::registry& reg)
        {
            reg.view<MeshStyleDetail>().each([&](auto& styleDetail)
                {
                    _styleDetailBins.emplace_back(&styleDetail);
                });

            int count = 0;
            auto view = reg.view<Mesh, MeshDetail, ActiveState, Visibility>();

            view.each([&](auto entity, auto& comp, auto& compDetail, auto& active, auto& visibility)
                {
                    auto* styleDetail = &_defaultMeshStyleDetail;
                    auto* style = reg.try_get<MeshStyle>(comp.style);
                    if (style)
                    {
                        styleDetail = &reg.get<MeshStyleDetail>(comp.style);
                    }

                    if (compDetail.node && visible(visibility, rs))
                    {
                        auto* transformDetail = reg.try_get<TransformDetail>(entity);
                        if (transformDetail)
                        {
                            if (transformDetail->views[rs.viewID].passingCull)
                            {
                                styleDetail->drawList.emplace_back(MeshDrawable{ compDetail.node, transformDetail });
                                ++count;
                            }
                        }
                        else
                        {
                            styleDetail->drawList.emplace_back(MeshDrawable{ compDetail.node, nullptr });
                            ++count;
                        }
                    }
                });

            // Render collected data.
            // TODO: swap vectors into unprotected space to free up the readlock?
            if (count > 0)
            {
                _pipelines[0].commands->accept(record);

                for(auto& styleDetail : _styleDetailBins)
                {
                    if (!styleDetail->drawList.empty())
                    {
                        for (auto& pass : styleDetail->passes)
                        {
                            pass->accept(record);

                            for (auto& drawable : styleDetail->drawList)
                            {
                                if (drawable.xformDetail)
                                {
                                    drawable.xformDetail->push(record);
                                }

                                drawable.node->accept(record);

                                if (drawable.xformDetail)
                                {
                                    drawable.xformDetail->pop(record);
                                }
                            }
                        }

                        styleDetail->drawList.clear();
                    }
                }
            }
        });
}

void
MeshSystemNode::traverse(vsg::ConstVisitor& v) const
{
    for (auto& pipeline : _pipelines)
    {
        pipeline.commands->accept(v);
    }

    // it might be an ECS visitor, in which case we'll communicate the entity being visited
    auto* ecsVisitor = dynamic_cast<ECSVisitor*>(&v);
    std::uint32_t viewID = ecsVisitor ? ecsVisitor->viewID : 0;

    _registry.read([&](entt::registry& reg)
        {
            auto view = reg.view<Mesh, MeshDetail, ActiveState>();

            view.each([&](auto entity, auto& comp, auto& compDetail, auto& active)
                {
                    if (compDetail.node)
                    {
                        if (ecsVisitor)
                            ecsVisitor->currentEntity = entity;

                        auto* transformDetail = reg.try_get<TransformDetail>(entity);
                        if (transformDetail)
                        {
                            _tempMT->matrix = transformDetail->views[viewID].model;
                            _tempMT->children[0] = compDetail.node;
                            _tempMT->accept(v);
                        }
                        else
                        {
                            compDetail.node->accept(v);
                        }
                    }
                });
        });

    Inherit::traverse(v);
}

void
MeshSystemNode::update(VSGContext& vsgcontext)
{
    if (status.failed()) return;

    // start by disposing of any old static objects
    if (!s_toDispose->children.empty())
    {
        dispose(s_toDispose);
        s_toDispose = vsg::Objects::create();
    }

    // process any objects marked dirty
    _registry.read([&](entt::registry& reg)
        {
            MeshTexture::eachDirty(reg, [&](entt::entity e)
                {
                    const auto& [tex, texDetail] = reg.get<MeshTexture, MeshTextureDetail>(e);
                    addOrUpdateTexture(tex, texDetail, reg);
                });

            MeshStyle::eachDirty(reg, [&](entt::entity e)
                {
                    const auto& [style, styleDetail] = reg.get<MeshStyle, MeshStyleDetail>(e);
                    createOrUpdateStyle(style, styleDetail, reg, vsgcontext);
                });

            MeshGeometry::eachDirty(reg, [&](entt::entity e)
                {
                    const auto& [geom, geomDetail] = reg.get<MeshGeometry, MeshGeometryDetail>(e);
                    createOrUpdateGeometry(geom, geomDetail, vsgcontext);
                });

            Mesh::eachDirty(reg, [&](entt::entity e)
                {
                    const auto& [comp, compDetail] = reg.get<Mesh, MeshDetail>(e);
                    auto* styleDetail = comp.style != entt::null ? reg.try_get<MeshStyleDetail>(comp.style) : nullptr;
                    auto* geomDetail = comp.geometry != entt::null ? reg.try_get<MeshGeometryDetail>(comp.geometry) : nullptr;
                    createOrUpdateComponent(comp, compDetail, styleDetail, geomDetail, vsgcontext);
                });
        });

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

        if (_normals.size() < _verts.size())
        {
            _normals.assign(_verts.size(), vsg::vec3(0.0f, 0.0f, 0.0f));

            for (size_t i = 0; i < _indices.size(); i += 3)
            {
                auto i0 = _indices[i];
                auto i1 = _indices[i + 1];
                auto i2 = _indices[i + 2];
                auto v0 = _verts[i0];
                auto v1 = _verts[i1];
                auto v2 = _verts[i2];
                auto edge1 = v1 - v0;
                auto edge2 = v2 - v0;
                auto faceNormal = vsg::cross(edge1, edge2);
                _normals[i0] += faceNormal;
                _normals[i1] += faceNormal;
                _normals[i2] += faceNormal;
            }
            for (auto& n : _normals)
            {
                n = vsg::normalize(n);
            }
        }

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
