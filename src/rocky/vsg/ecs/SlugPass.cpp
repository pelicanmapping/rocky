/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "SlugPass.h"
#ifdef ROCKY_HAS_SLUGHORN
#include "../SharedRenderData.h"
#include "../ShaderDefines.h"
#include "../ViewDependentState.h"
#include <rocky/vsg/VSGUtils.h>
#include <vsg/utils/ShaderCompiler.h>
#include <vsg/vk/State.h>
#include <cstring>
#include <algorithm>
#include <limits>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::detail;

vsg::ref_ptr<SlugPassGeometry> detail::createSlugPassGeometry(VSGContext context, const SlugResource& resource,
    const std::shared_ptr<SlugPassStatistics>& statistics)
{
    const auto count = resource.passLayers.size();
    const auto bytes = count * sizeof(SlugLayerGPU);
    const auto limit = context->device()->getPhysicalDevice()->getProperties().limits.maxStorageBufferRange;
    if (count == 0 || count > std::numeric_limits<std::uint32_t>::max() || bytes > limit ||
        bytes > statistics->budgetBytes || statistics->residentBytes.load() > statistics->budgetBytes - bytes)
        return {};
    const bool segments = resource.passLayers.front().uvToEmX.w < 0.0f;
    // A mixed payload needs per-layer dependencies. Retain the ordinary path until that case is supported.
    for (const auto& layer : resource.passLayers)
        if ((layer.uvToEmX.w < 0.0f) != segments) return {};
    auto geometry = SlugPassGeometry::create();
    geometry->segments = segments;
    auto data = vsg::ubyteArray::create(bytes);
    auto* records = static_cast<unsigned char*>(data->dataPointer());
    std::uint32_t next = 0;
    for (unsigned pass = 0; pass < 2; ++pass)
    {
        for (const auto& layer : resource.passLayers)
        {
            if (layer.isOutline != (pass == 0)) continue;
            SlugLayerGPU record;
            record.color = layer.color;
            record.uvToEmX = layer.uvToEmX;
            record.uvToEmY = layer.uvToEmY;
            record.bandTransform = layer.bandTransform;
            record.shapeData = layer.shapeData;
            std::memcpy(records + sizeof(record) * next++, &record, sizeof(record));
        }
        if (pass == 0) geometry->outlines = next;
    }
    geometry->buffer = vsg::DescriptorBuffer::create(data, 1, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    if (!context->compile(geometry->buffer)) return {};
    geometry->revision = resource.revision;
    geometry->count = static_cast<std::uint32_t>(count);
    geometry->bytes = bytes;
    geometry->statistics = statistics;
    statistics->peakBytes = std::max(statistics->peakBytes, statistics->residentBytes.fetch_add(bytes) + bytes);
    statistics->uploadedBytes += bytes;
    ++statistics->uploads;
    return geometry;
}

bool SlugPassNode::initialize(VSGContext context)
{
    _shared = context->sharedRenderData;
    auto vertex = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main",
        vsg::findFile("shaders/rocky.slug.pass.vert", context->searchPaths), context->readerWriterOptions);
    auto fragment = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main",
        vsg::findFile("shaders/rocky.slug.pass.frag", context->searchPaths), context->readerWriterOptions);
    if (!vertex || !fragment)
    {
        Log()->warn("DecalSystemNode: missing Slug pass shaders");
        return false;
    }

    const auto capacity = context->sharedRenderData->projectedTextureCapacity();
    auto window = _window.ref_ptr();
    if (!window) return false;
    const auto limits = window->getOrCreateDevice()->getPhysicalDevice()->getProperties().limits;
    if (limits.maxPerStageDescriptorStorageBuffers < capacity + 8u ||
        limits.maxDescriptorSetStorageBuffers < capacity + 8u) return false;
    // Numeric array lengths must match the descriptor layout, including non-default startup capacities.
    for (const auto& stage : { vertex, fragment })
    {
        auto& source = stage->module->source;
        auto newline = source.find('\n', source.find("#version"));
        if (newline == std::string::npos) return false;
        source.insert(newline + 1u, "#define MAX_NUM_DECAL_TEXTURES " + std::to_string(capacity) + "\n");
    }
    auto compiler = vsg::ShaderCompiler::create();
    compiler->defaults = context->shaderCompileSettings;
    vsg::ShaderStages stages{ vertex, fragment };
    std::vector<std::string> defines(context->shaderCompileSettings->defines.begin(),
        context->shaderCompileSettings->defines.end());
    if (!compiler->compile(stages, defines, context->readerWriterOptions)) return false;

    auto shaderSet = vsg::ShaderSet::create(vsg::ShaderStages{ vertex, fragment });
    shaderSet->addDescriptorBinding("u_sceneDepth", "", DESCRIPTOR_SET_LOCAL, 0,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, {});
    shaderSet->addDescriptorBinding("b_slugSegments", "", DESCRIPTOR_SET_LOCAL, 1,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, capacity, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, {});
    shaderSet->addDescriptorBinding("b_slugSegmentDecals", "", DESCRIPTOR_SET_LOCAL, 2,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, {});
    shaderSet->addDescriptorBinding("u_slugCurveTexture", "", DESCRIPTOR_SET_GLOBAL,
        BINDING_SLUG_CURVE_TEXTURE, TYPE_SLUG_CURVE_TEXTURE, capacity, VK_SHADER_STAGE_FRAGMENT_BIT, {});
    shaderSet->addDescriptorBinding("u_slugBandTexture", "", DESCRIPTOR_SET_GLOBAL,
        BINDING_SLUG_BAND_TEXTURE, TYPE_SLUG_BAND_TEXTURE, capacity, VK_SHADER_STAGE_FRAGMENT_BIT, {});
    shaderSet->addDescriptorBinding("u_passCamera", "", DESCRIPTOR_SET_LOCAL, 3,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, {});
    addViewDependentStateToShaderSet(shaderSet);
    shaderSet->addPushConstantRange("pc", "",
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 128);

    _config = vsg::GraphicsPipelineConfigurator::create(shaderSet);
    _config->shaderHints = context->shaderCompileSettings;
    _config->enableDescriptor("u_sceneDepth");
    _config->enableDescriptor("b_slugSegments");
    _config->enableDescriptor("b_slugSegmentDecals");
    _config->enableDescriptor("u_passCamera");
    _config->enableDescriptor("u_slugCurveTexture");
    _config->enableDescriptor("u_slugBandTexture");
    enableViewDependentStateUniforms(_config);
    for (auto& state : _config->pipelineStates)
    {
        // Back faces only: one fragment per covered pixel, including from inside the box.
        if (auto* raster = dynamic_cast<vsg::RasterizationState*>(state.get()))
            raster->cullMode = VK_CULL_MODE_FRONT_BIT;
        if (auto* assembly = dynamic_cast<vsg::InputAssemblyState*>(state.get()))
            assembly->topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        // A back face survives only at or behind the scene (reversed Z: LESS_OR_EQUAL),
        // i.e. where the scene point can lie inside the box; tall receiving volumes would
        // otherwise smear thin boxes across many pixels. The shader rejects the rest.
        if (auto* depth = dynamic_cast<vsg::DepthStencilState*>(state.get()))
        {
            depth->depthTestEnable = VK_TRUE;
            depth->depthWriteEnable = VK_FALSE;
            depth->depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        }
        if (auto* blend = dynamic_cast<vsg::ColorBlendState*>(state.get()))
        {
            blend->attachments = { {
                VK_TRUE,
                VK_BLEND_FACTOR_SRC_ALPHA,
                VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                VK_BLEND_OP_ADD,
                VK_BLEND_FACTOR_ONE,
                VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                VK_BLEND_OP_ADD,
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT } };
        }
    }
    _config->init();

    _sampler = vsg::Sampler::create();
    _sampler->magFilter = VK_FILTER_NEAREST;
    _sampler->minFilter = VK_FILTER_NEAREST;
    _sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    _sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    _sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    _sampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    auto group = vsg::StateGroup::create();
    group->add(_config->bindGraphicsPipeline);
    group->add(vsg::BindViewDescriptorSets::create(
        VK_PIPELINE_BIND_POINT_GRAPHICS, _config->layout, DESCRIPTOR_SET_VDS));
    addChild(group);
    // Coherent material batches compile without the other evaluator's register footprint.
    // The third variant is the diagnostic original-order reference, with a runtime material branch.
    for (unsigned material = 1; material <= 2; ++material)
    {
        vsg::ShaderStages stages;
        for (const auto& stage : _config->bindGraphicsPipeline->pipeline->stages)
        {
            auto specialized = vsg::ShaderStage::create(*stage);
            specialized->specializationConstants[0] = vsg::uintValue::create(material);
            stages.push_back(specialized);
        }
        auto variant = vsg::StateGroup::create();
        variant->add(vsg::BindGraphicsPipeline::create(vsg::GraphicsPipeline::create(
            _config->layout, stages, _config->bindGraphicsPipeline->pipeline->pipelineStates)));
        variant->add(group->stateCommands[1]);
        addChild(variant);
    }

    _camera = vsg::vec4Array::create(5);
    _camera->properties.dataVariance = vsg::DYNAMIC_DATA;
    _cameraDescriptor = vsg::DescriptorBuffer::create(_camera, 3, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    _capacity = capacity;
    return true;
}

vsg::ref_ptr<SlugPassNode> SlugPassNode::attach(VSGContext context, Window& window, View& view)
{
    if (!context->slugPassRequested || !window || !view) return {};
    if (auto existing = view.renderGraph->getRefObject<SlugPassNode>("rocky.slugPass")) return existing;
    auto win = window.vsgWindow;
    // Attachment rejection is reported once per view; no routing has changed yet.
    auto unavailable = [&](const char* reason) -> vsg::ref_ptr<SlugPassNode>
    {
        view.vsgView->setValue("rocky.slugPassFallbackReason", std::string(reason));
        Log()->info("Slug additional pass: {}", reason);
        return {};
    };
    if (view.renderGraph->framebuffer || view.renderGraph->renderPass)
        return unavailable("custom framebuffer/render pass");
    if (!context->slugPassStorageIndexing)
        return unavailable("storage-buffer indexing was not enabled on this device");
    const auto limits = win->getOrCreateDevice()->getPhysicalDevice()->getProperties().limits;
    const auto capacity = context->sharedRenderData->projectedTextureCapacity();
    if (limits.maxPerStageDescriptorStorageBuffers < capacity + 8u ||
        limits.maxDescriptorSetStorageBuffers < capacity + 8u)
        return unavailable("storage-buffer descriptor capacity");
    auto device = win->getOrCreateDevice();
    if (context->device() && context->device() != device) return unavailable("window uses a different device");
    if (win->framebufferSamples() != VK_SAMPLE_COUNT_1_BIT)
        return unavailable("MSAA window");
    if ((win->traits()->depthImageUsage & VK_IMAGE_USAGE_SAMPLED_BIT) == 0)
        return unavailable("depth was not created for sampling");
    VkFormatProperties properties{};
    vkGetPhysicalDeviceFormatProperties(device->getPhysicalDevice()->vk(), win->depthFormat(), &properties);
    if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) == 0)
        return unavailable("depth format is not sampleable");
    auto node = SlugPassNode::create();
    node->_window = win;
    node->_mainView = view.vsgView;
    node->_mainGraph = view.renderGraph;
    node->_policy = view.slugPassPolicy;
    if (!node->initialize(context)) return unavailable("shaders unavailable or compilation failed");
    vsg::AttachmentDescription color{};
    color.format = win->surfaceFormat().format;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    vsg::AttachmentDescription z{};
    z.format = win->depthFormat();
    z.samples = VK_SAMPLE_COUNT_1_BIT;
    z.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    z.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    z.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    z.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    z.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    z.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    vsg::SubpassDescription subpass;
    subpass.colorAttachments = { { 0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL } };
    subpass.depthStencilAttachments = { { 1u, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL } };
    auto renderPass = vsg::RenderPass::create(device, vsg::RenderPass::Attachments{ color, z },
        vsg::RenderPass::Subpasses{ subpass }, win->getOrCreateRenderPass()->dependencies);
    node->_barrier = vsg::PipelineBarrier::create(
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0,
        vsg::MemoryBarrier::create(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT));
    auto passView = vsg::View::create();
    passView->share(*view.vsgView);
    passView->camera = view.vsgView->camera;
    passView->viewDependentState = ViewDependentStateEx::create(passView, device);
    passView->addChild(node);
    auto passGraph = vsg::RenderGraph::create(win);
    passGraph->renderPass = renderPass;
    // The camera belongs to the main graph; resizing it twice would scale inset viewports twice.
    passGraph->windowResizeHandler = {};
    passGraph->addChild(passView);
    node->_view = passView;
    node->_graph = passGraph;
    node->refresh(view);
    auto& children = window.commandGraph->children;
    auto position = std::find(children.begin(), children.end(), view.renderGraph);
    if (position == children.end()) { passView->children.clear(); return {}; }
    children.insert(position + 1, { node->_barrier, passGraph });
    view.renderGraph->setObject("rocky.slugPass", node);
    view.vsgView->setObject("rocky.slugPass", node);
    view.renderGraph->setObject("rocky.slugPassGraph", passGraph);
    if (context->viewer()->compileManager) context->compileRenderGraph(passGraph, win);
    return node;
}

bool SlugPassNode::detach(VSGContext context, const Window& window, const View& view)
{
    auto node = view.renderGraph->getRefObject<SlugPassNode>("rocky.slugPass");
    if (!node) return true;
    auto passView = node->_view.ref_ptr();
    auto passGraph = node->_graph.ref_ptr();
    node->active = false;
    if (!context->removeViewCompileContexts(passView)) return false;
    auto& children = window.commandGraph->children;
    children.erase(std::remove(children.begin(), children.end(), node->_barrier), children.end());
    children.erase(std::remove(children.begin(), children.end(), passGraph), children.end());
    // Detach recording references before queuing GPU-safe retirement; node-to-graph links are observers.
    if (passView) passView->children.clear();
    view.renderGraph->removeObject("rocky.slugPass");
    view.vsgView->removeObject("rocky.slugPass");
    view.renderGraph->removeObject("rocky.slugPassGraph");
    context->dispose(passGraph);
    context->dispose(node);
    return true;
}

void SlugPassNode::refresh(const View& view)
{
    auto graph = _graph.ref_ptr();
    auto passView = _view.ref_ptr();
    if (!graph || !passView) return;
    passView->camera = view.vsgView->camera;
    graph->renderArea = view.renderGraph->renderArea;
    if (_frame == 0u) return; // Initial graph compilation establishes the first viewport state.
    vsg::UpdateGraphicsPipelines update;
    update.context = vsg::Context::create(graph->renderPass->device);
    update.context->renderPass = graph->renderPass;
    update.context->overridePipelineStates.emplace_back(vsg::MultisampleState::create(VK_SAMPLE_COUNT_1_BIT));
    graph->accept(update);
}

bool SlugPassNode::refreshCapacity(VSGContext context)
{
    if (_capacity == context->sharedRenderData->projectedTextureCapacity()) return true;
    auto replacement = SlugPassNode::create();
    replacement->_window = _window;
    if (!replacement->initialize(context)) return false;
    for (auto& child : children) context->dispose(child);
    context->dispose(_config);
    context->dispose(_local); _local = {};
    context->dispose(_atlas); _atlas = {};
    for (auto& payload : _boundGeometry) context->dispose(payload);
    _boundGeometry.clear();
    children = std::move(replacement->children);
    _config = replacement->_config;
    _camera = replacement->_camera;
    _cameraDescriptor = replacement->_cameraDescriptor;
    _sampler = replacement->_sampler;
    _depth = {};
    _sharedRevision = -1;
    _capacity = replacement->_capacity;
    auto graph = _graph.ref_ptr();
    auto window = _window.ref_ptr();
    if (graph && window) context->compileRenderGraph(graph, window);
    return true;
}

bool SlugPassNode::begin(VSGContext context)
{
    _frame = context->viewer()->getFrameStamp()->frameCount;
    _pending.clear();
    _draws.clear();
    active = false;
    _prepared = false;
    if (!refreshCapacity(context)) { reason = "descriptor capacity configuration"; return false; }
    auto win = _window.ref_ptr();
    auto main = _mainView.ref_ptr();
    auto policy = _policy.lock();
    if (!win || !main || !policy || !policy->fallbackReason)
    {
        reason = "receiver/compositing contract";
        return false;
    }
    auto graph = _mainGraph.ref_ptr();
    if (!graph || graph->framebuffer || graph->renderPass)
    {
        reason = "custom framebuffer/render pass";
        return false;
    }
    if (!main->camera->projectionMatrix->cast<vsg::Perspective>() &&
        !main->camera->projectionMatrix->cast<vsg::Orthographic>())
    {
        reason = "custom projection";
        return false;
    }
    reason = policy->fallbackReason();
    if (!reason.empty()) return false;
    auto passView = _view.ref_ptr();
    auto passGraph = _graph.ref_ptr();
    if (!passView || !passGraph) { reason = "detached"; return false; }
    passView->camera = main->camera;
    passGraph->previous_extent = win->extent2D();
    const auto viewport = main->camera->getViewport();
    passGraph->renderArea.offset = { static_cast<int>(viewport.x), static_cast<int>(viewport.y) };
    passGraph->renderArea.extent = { static_cast<unsigned>(viewport.width), static_cast<unsigned>(viewport.height) };
    const auto depth = win->getDepthImageView();
    if (!depth || !depth->image || (depth->image->usage & VK_IMAGE_USAGE_SAMPLED_BIT) == 0)
    {
        reason = "depth unavailable";
        return false;
    }
    if (depth != _depth)
    {
        _depth = depth;
        // Combined depth/stencil attachments need a depth-only view when sampled.
        auto sampledDepth = vsg::ImageView::create(_depth->image, VK_IMAGE_ASPECT_DEPTH_BIT);
        _depthDescriptor = vsg::DescriptorImage::create(
            vsg::ImageInfo::create(_sampler, sampledDepth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL),
            0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        context->dispose(_local);
        _local = {};
    }
    auto shared = _shared.lock();
    if (!shared) return false;
    if (!_atlas || shared->sharedDescriptorsChanged(_sharedRevision))
    {
        auto replacement = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, _config->layout,
            DESCRIPTOR_SET_GLOBAL, vsg::DescriptorSet::create(_config->layout->setLayouts[DESCRIPTOR_SET_GLOBAL],
                vsg::Descriptors{ shared->slugCurveTexture, shared->slugBandTexture }));
        if (!context->compile(replacement)) { reason = "atlas compilation"; return false; }
        context->dispose(_atlas);
        _atlas = replacement;
    }
    const auto inverse = glm::inverse(to_glm(main->camera->projectionMatrix->transform()));
    for (unsigned i = 0; i < 4; ++i) _camera->at(i) = to_vsg(glm::vec4(inverse[i]));
    const auto vp = main->camera->getViewport();
    _camera->at(4) = vsg::vec4(vp.x, vp.y, vp.width, vp.height);
    _camera->dirty();
    _prepared = true;
    return true;
}

bool SlugPassNode::add(std::uint32_t decal,
    vsg::ref_ptr<SlugPassGeometry> geometry, const Color& highlight)
{
    if (!_prepared || !geometry) return false;
    _pending.push_back({ geometry, decal, highlight });
    return true;
}

bool SlugPassNode::prepareBatch(VSGContext context)
{
    struct Span { glm::uvec4 range; glm::vec4 highlight; };
    static_assert(sizeof(Span) == 32, "Pass spans match the shader's std430 layout");
    std::vector<Span> spans(1);
    std::vector<vsg::ref_ptr<SlugPassGeometry>> geometry;
    std::unordered_map<SlugPassGeometry*, std::uint32_t> indices;
    spans.reserve(1u + 2u * _draws.size());
    geometry.reserve(_draws.size());
    indices.reserve(_draws.size());
    _batches.clear();
    std::uint64_t count = 0;
    for (unsigned pass = 0; pass < 2; ++pass)
        for (std::size_t i = 0; i < _draws.size(); ++i)
        {
            const auto& instance = _draws[i];
            const auto& payload = instance.geometry;
            const auto first = pass == 0 ? 0u : payload->outlines;
            const auto size = pass == 0 ? payload->outlines : payload->count - payload->outlines;
            if (size == 0u) continue;
            auto inserted = indices.emplace(payload.get(), static_cast<std::uint32_t>(geometry.size()));
            if (inserted.second) geometry.push_back(payload);
            if (_batches.empty() || _batches.back().segments != payload->segments)
                _batches.push_back({ static_cast<std::uint32_t>(count), 0u, payload->segments });
            count += size;
            if (count > std::numeric_limits<std::uint32_t>::max()) return false;
            _batches.back().count += size;
            spans.push_back({ glm::uvec4(static_cast<std::uint32_t>(count), first,
                inserted.first->second, instance.decal), instance.highlight });
        }
    _instanceCount = static_cast<std::uint32_t>(count);
    const auto capacity = context->sharedRenderData->projectedTextureCapacity();
    if (geometry.size() > capacity) return false;
    if (count == 0u)
    {
        context->dispose(_local); _local = {};
        for (auto& payload : _boundGeometry) context->dispose(payload);
        _boundGeometry.clear();
        return true;
    }
    spans.front().range = glm::uvec4(static_cast<std::uint32_t>(spans.size() - 1u), 0u, 0u, 0u);
    const auto bytes = spans.size() * sizeof(Span);
    if (bytes > context->device()->getPhysicalDevice()->getProperties().limits.maxStorageBufferRange) return false;
    if (!_spans || _spans->dataSize() < bytes)
    {
        std::size_t allocation = 256u;
        while (allocation < bytes) allocation *= 2u;
        _spans = vsg::ubyteArray::create(allocation);
        std::memset(_spans->dataPointer(), 0, allocation);
        _spans->properties.dataVariance = vsg::DYNAMIC_DATA;
        _spanDescriptor = vsg::DescriptorBuffer::create(_spans, 2, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        context->dispose(_local); _local = {};
    }
    if (std::memcmp(_spans->dataPointer(), spans.data(), bytes) != 0)
    {
        std::memcpy(_spans->dataPointer(), spans.data(), bytes);
        _spans->dirty();
    }
    if (!_local || geometry != _boundGeometry)
    {
        vsg::BufferInfoList buffers;
        for (const auto& payload : geometry) buffers.push_back(payload->buffer->bufferInfoList.front());
        // Every descriptor is valid; unused entries alias the first immutable payload.
        buffers.resize(capacity, buffers.front());
        auto payloads = vsg::DescriptorBuffer::create(buffers, 1, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        auto replacement = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, _config->layout,
            DESCRIPTOR_SET_LOCAL, vsg::DescriptorSet::create(_config->layout->setLayouts[DESCRIPTOR_SET_LOCAL],
                vsg::Descriptors{ _depthDescriptor, payloads, _spanDescriptor, _cameraDescriptor }));
        if (!context->compile(replacement)) return false;
        context->dispose(_local);
        for (auto& payload : _boundGeometry) context->dispose(payload);
        _local = replacement;
        _boundGeometry = std::move(geometry);
    }
    return true;
}

bool SlugPassNode::finish(VSGContext context, bool compatible)
{
    active = _prepared && compatible;
    if (active)
    {
        _draws.swap(_pending);
        active = prepareBatch(context);
        reason = active ? "active" : "batch compilation or capacity";
    }
    else if (_prepared) reason = "unsupported payload or capacity";
    if (!active)
    {
        _instanceCount = 0;
        context->dispose(_local); _local = {};
        for (auto& payload : _boundGeometry) context->dispose(payload);
        _boundGeometry.clear();
    }
    if (_reportedReason != reason)
    {
        Log()->info("Slug additional pass: {}", reason);
        _reportedReason = reason;
    }
    return active;
}

void SlugPassNode::traverse(vsg::RecordTraversal& record) const
{
    if (!active || !_atlas) return;
    auto shared = _shared.lock();
    auto main = _mainView.ref_ptr();
    if (!shared || !main) return;
    auto vds = shared->viewDependentState[main->viewID];
    if (!vds) return;
    auto* commandBuffer = record.getCommandBuffer();
    auto* previous = commandBuffer->viewDependentState;
    commandBuffer->viewDependentState = vds.get();
    auto* state = record.getState();
    if (_local && _instanceCount > 0u)
    {
        for (std::size_t batch = 0; batch < (preserveSubmissionOrder ? 1u : _batches.size()); ++batch)
        {
            const auto range = preserveSubmissionOrder ? Batch{ 0u, _instanceCount, false } : _batches[batch];
            const auto material = preserveSubmissionOrder ? 2u : range.segments ? 1u : 0u;
            auto* group = static_cast<vsg::StateGroup*>(children[material].get());
            vsg::StateCommands commands{ group->stateCommands[0], group->stateCommands[1], _local, _atlas };
            state->push(commands);
            vsg::Draw draw(14u, range.count, 0u, range.first);
            draw.accept(record);
            state->pop(commands);
        }
    }
    commandBuffer->viewDependentState = previous;
}
#endif
