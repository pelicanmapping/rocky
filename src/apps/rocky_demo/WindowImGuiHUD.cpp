/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "WindowImGuiHUD.h"
#include <imgui_impl_vulkan.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <unordered_map>

namespace
{
    // Restore the caller's ImGui context, including when Vulkan resource creation throws.
    struct ContextScope
    {
        ImGuiContext* previous = ImGui::GetCurrentContext();

        //! Restores the context belonging to the surrounding demo UI.
        ~ContextScope() { ImGui::SetCurrentContext(previous); }
    };

    //! Preserves the resolved scene color and synchronizes blending after all preceding views.
    vsg::ref_ptr<vsg::RenderPass> createHUDPass(vsg::Device* device, VkFormat format)
    {
        auto color = vsg::defaultColorAttachment(format);
        color.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        color.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        vsg::SubpassDescription subpass;
        subpass.colorAttachments.push_back({ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

        vsg::SubpassDependency dependency;
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        return vsg::RenderPass::create(device, vsg::RenderPass::Attachments{ color },
            vsg::RenderPass::Subpasses{ subpass }, vsg::RenderPass::Dependencies{ dependency });
    }
}

// Owns a separate ImGui backend in the executable, avoiding context switches across Rocky's DLL boundary.
// Records directly into the window command buffer, without allocating a VSG View or traversing the scene.
class WindowImGuiHUD::HUDNode : public vsg::Inherit<vsg::Node, HUDNode>
{
public:
    //! Retains the target window and callback; Vulkan resources are created on the first visible frame.
    HUDNode(vsg::ref_ptr<vsg::Window> window, vsg::ref_ptr<State> state, DrawFunction draw) :
        _window(window), _state(state), _draw(std::move(draw)) { }

    //! Releases backend resources after the installation has waited for GPU completion.
    ~HUDNode() override
    {
        ContextScope scope;
        release();
    }

    //! Records once at the end of this window's command graph; other traversal types need no resources.
    void accept(vsg::RecordTraversal& traversal) const override
    {
        if (_state->visible)
            const_cast<HUDNode*>(this)->record(traversal);
    }

    //! Waits for this HUD's buffers without bypassing VSG's queue synchronization.
    void waitForIdle()
    {
        if (_queue)
            _queue->waitIdle();
    }

private:
    //! Releases this context and its swapchain resources with no GPU work outstanding.
    void release()
    {
        if (_context)
        {
            ImGui::SetCurrentContext(_context);
            if (_backendInitialized)
                ImGui_ImplVulkan_Shutdown();
            ImGui::DestroyContext(_context);
            _context = nullptr;
            _backendInitialized = false;
        }
        _textures.clear();
        _framebuffers.clear();
        _pool = {};
        _pass = {};
        _swapchain = {};
    }

    //! Rebuilds resources for the actual swapchain, including changed image counts and surface formats.
    void initialize(vsg::ref_ptr<vsg::Swapchain> swapchain)
    {
        auto device = _window->getDevice();
        // Resize is infrequent; old framebuffers and ImGui buffers must no longer be in flight.
        if (_swapchain)
            waitForIdle();
        release();

        _pass = createHUDPass(device, swapchain->getImageFormat());
        auto extent = swapchain->getExtent();
        for (auto& imageView : swapchain->getImageViews())
            _framebuffers.push_back(vsg::Framebuffer::create(
                _pass, vsg::ImageViews{ imageView }, extent.width, extent.height, 1));

        _pool = vsg::DescriptorPool::create(device, 32,
            vsg::DescriptorPoolSizes{ { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 32 } });
        _context = ImGui::CreateContext();
        ImGui::SetCurrentContext(_context);
        ImGui::GetIO().IniFilename = nullptr;
        ImGui::GetIO().LogFilename = nullptr;

        auto physicalDevice = device->getPhysicalDevice();
        uint32_t queueFamily = 0;
        std::tie(queueFamily, std::ignore) = physicalDevice->getQueueFamily(
            _window->traits()->queueFlags, _window->getSurface());
        _queue = device->getQueue(queueFamily);

        ImGui_ImplVulkan_InitInfo info{};
        info.Instance = *device->getInstance();
        info.PhysicalDevice = *physicalDevice;
        info.Device = *device;
        info.QueueFamily = queueFamily;
        info.Queue = *_queue;
        info.DescriptorPool = *_pool;
        info.MinImageCount = 2;
        info.ImageCount = std::max(2u, static_cast<uint32_t>(_framebuffers.size()));
#if IMGUI_VERSION_NUM >= 19200
        info.PipelineInfoMain.RenderPass = *_pass;
        info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
#else
        info.RenderPass = *_pass;
        info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
#endif
        _backendInitialized = ImGui_ImplVulkan_Init(&info);
        if (!_backendInitialized)
            throw vsg::Exception{ "Could not initialize the window HUD", VK_ERROR_INITIALIZATION_FAILED };
#if IMGUI_VERSION_NUM < 19200
        unsigned char* pixels = nullptr;
        int width = 0, height = 0;
        ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        auto texture = uploadTexture(pixels, width, height, 4);
        ImGui::GetIO().Fonts->SetTexID(reinterpret_cast<ImTextureID>(texture->vk(device->deviceID)));
        _textures[nullptr] = texture;
#endif
        _swapchain = swapchain;
    }

    //! Uploads a small UI texture through VSG's locked queue and waits until it is safe to use.
    vsg::ref_ptr<vsg::DescriptorSet> uploadTexture(const unsigned char* pixels, int width, int height, int channels)
    {
        auto data = vsg::ubvec4Array2D::create(width, height);
        data->properties.format = VK_FORMAT_R8G8B8A8_UNORM;
        if (channels == 4)
            std::memcpy(data->dataPointer(), pixels, data->dataSize());
        else
            for (size_t i = 0; i < data->size(); ++i)
                (*data)[i] = vsg::ubvec4(255, 255, 255, pixels[i]);

        auto sampler = vsg::Sampler::create();
        sampler->addressModeU = sampler->addressModeV = sampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        auto image = vsg::DescriptorImage::create(sampler, data);
        auto layout = vsg::DescriptorSetLayout::create(vsg::DescriptorSetLayoutBindings{
            { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr } });
        auto texture = vsg::DescriptorSet::create(layout, vsg::Descriptors{ image });

        auto context = vsg::Context::create(_window->getDevice());
        context->graphicsQueue = _queue;
        context->commandPool = vsg::CommandPool::create(_window->getDevice(), _queue->queueFamilyIndex());
        texture->compile(*context);
        context->record();
        context->waitForCompletion();
        return texture;
    }

#if IMGUI_VERSION_NUM >= 19200
    //! Handles font atlas changes without ImGui's raw queue submissions racing background terrain compilation.
    void updateTextures(ImDrawData& data)
    {
        if (!data.Textures)
            return;
        for (auto* texture : *data.Textures)
        {
            if (texture->Status == ImTextureStatus_WantCreate || texture->Status == ImTextureStatus_WantUpdates)
            {
                auto descriptor = uploadTexture(static_cast<const unsigned char*>(texture->GetPixels()),
                    texture->Width, texture->Height, texture->BytesPerPixel);
                // The upload fence also completes earlier graphics submissions using the previous descriptor.
                _textures[texture] = descriptor;
                texture->SetTexID(reinterpret_cast<ImTextureID>(descriptor->vk(_window->getDevice()->deviceID)));
                texture->SetStatus(ImTextureStatus_OK);
            }
            else if (texture->Status == ImTextureStatus_WantDestroy)
            {
                waitForIdle();
                _textures.erase(texture);
                texture->SetTexID(ImTextureID_Invalid);
                texture->SetStatus(ImTextureStatus_Destroyed);
            }
        }
    }
#endif

    //! Draws in framebuffer pixel coordinates, leaving the final image ready for presentation.
    void record(vsg::RecordTraversal& traversal)
    {
        auto swapchain = _window->getSwapchain();
        auto extent = _window->extent2D();
        auto imageIndex = _window->imageIndex();
        if (!swapchain || extent.width == 0 || extent.height == 0 || imageIndex >= _window->numFrames())
            return;

        ContextScope scope;
        if (swapchain != _swapchain)
            initialize(swapchain);
        ImGui::SetCurrentContext(_context);

        auto now = std::chrono::steady_clock::now();
        auto& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(static_cast<float>(extent.width), static_cast<float>(extent.height));
        io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
        io.DeltaTime = std::max(0.001f, std::chrono::duration<float>(now - _lastFrame).count());
        _lastFrame = now;

#if IMGUI_VERSION_NUM >= 19200
        ImGui_ImplVulkan_NewFrame();
#endif
        // Older backends' NewFrame uploads its own atlas; we already supplied one through VSG.
        ImGui::NewFrame();
        _draw(_context, _state->mousePosition);
        ImGui::Render();
        auto* drawData = ImGui::GetDrawData();
#if IMGUI_VERSION_NUM >= 19200
        updateTextures(*drawData);
#endif

        VkRenderPassBeginInfo begin{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
        begin.renderPass = *_pass;
        begin.framebuffer = *_framebuffers[imageIndex];
        begin.renderArea.extent = extent;
        auto commandBuffer = traversal.getCommandBuffer();
        vkCmdBeginRenderPass(*commandBuffer, &begin, VK_SUBPASS_CONTENTS_INLINE);
        ImGui_ImplVulkan_RenderDrawData(drawData, *commandBuffer);
        vkCmdEndRenderPass(*commandBuffer);
    }

    vsg::ref_ptr<vsg::Window> _window;
    vsg::ref_ptr<State> _state;
    DrawFunction _draw;
    vsg::ref_ptr<vsg::Swapchain> _swapchain;
    vsg::ref_ptr<vsg::RenderPass> _pass;
    vsg::ref_ptr<vsg::DescriptorPool> _pool;
    vsg::ref_ptr<vsg::Queue> _queue;
    std::unordered_map<const void*, vsg::ref_ptr<vsg::DescriptorSet>> _textures;
    std::vector<vsg::ref_ptr<vsg::Framebuffer>> _framebuffers;
    ImGuiContext* _context = nullptr;
    bool _backendInitialized = false;
    std::chrono::steady_clock::time_point _lastFrame = std::chrono::steady_clock::now();
};

class WindowImGuiHUD::MouseObserver : public vsg::Inherit<vsg::Visitor, MouseObserver>
{
public:
    //! Observes mouse events, even when a view consumed them, without changing event handling.
    MouseObserver(vsg::ref_ptr<vsg::Window> window, vsg::ref_ptr<State> state, rocky::VSGContext context) :
        _window(window), _state(state), _context(context) { }

    //! Updates the readout during both ordinary movement and map drags; requests on-demand repainting.
    void apply(vsg::MoveEvent& event) override
    {
        if (event.window.ref_ptr() == _window)
        {
            _state->mousePosition = ImVec2(static_cast<float>(event.x), static_cast<float>(event.y));
            if (_state->visible)
                _context->requestFrame();
        }
    }

private:
    vsg::ref_ptr<vsg::Window> _window;
    vsg::ref_ptr<State> _state;
    rocky::VSGContext _context;
};

WindowImGuiHUD::WindowImGuiHUD(rocky::Application& app, const rocky::Window& window, DrawFunction draw) :
    _app(app), _window(window), _state(State::create()),
    _node(HUDNode::create(window.vsgWindow, _state, std::move(draw))),
    _events(MouseObserver::create(window.vsgWindow, _state, app.vsgcontext))
{
    moveToEnd();
    _app.viewer->getEventHandlers().insert(_app.viewer->getEventHandlers().begin(), _events);
    _subscriptions += _app.display.onAddView([this](rocky::Window& window, rocky::View&)
        {
            if (window == _window)
                moveToEnd();
        });
    _subscriptions += _app.display.onRemoveWindow([this](const rocky::Window& window)
        {
            if (window == _window)
                detach();
        });
}

WindowImGuiHUD::~WindowImGuiHUD()
{
    detach();
}

void WindowImGuiHUD::moveToEnd()
{
    auto& children = _window.commandGraph->children;
    children.erase(std::remove(children.begin(), children.end(), _node), children.end());
    children.push_back(_node);
}

void WindowImGuiHUD::detach()
{
    _subscriptions.clear();
    if (!_node)
        return;
    _node->waitForIdle();
    auto& children = _window.commandGraph->children;
    children.erase(std::remove(children.begin(), children.end(), _node), children.end());
    auto& handlers = _app.viewer->getEventHandlers();
    handlers.erase(std::remove(handlers.begin(), handlers.end(), _events), handlers.end());
    _events = {};
    _node = {};
}
