#pragma once

/* <editor-fold desc="MIT License">

Copyright(c) 2021 Don Burns, Roland Hill and Robert Osfield.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <functional>

#include <vsg/app/Window.h>
#include <vsg/commands/ClearAttachments.h>
#include <vsg/nodes/Group.h>
#include <vsg/vk/DescriptorPool.h>

#include <imgui.h>

namespace vsgImGui
{
    class RenderImGui : public vsg::Inherit<vsg::Group, RenderImGui>
    {
    public:
        RenderImGui(const vsg::ref_ptr<vsg::Window>& window, bool useClearAttachments = false);

        RenderImGui(vsg::ref_ptr<vsg::Device> device, uint32_t queueFamily,
            vsg::ref_ptr<vsg::RenderPass> renderPass,
            uint32_t minImageCount, uint32_t imageCount,
            VkExtent2D imageSize, bool useClearAttachments = false);

        template<typename... Args>
        RenderImGui(const vsg::ref_ptr<vsg::Window>& window, Args&&... args) :
            RenderImGui(window, false)
        {
            (add(args), ...);
        }

        template<typename... Args>
        RenderImGui(const vsg::ref_ptr<vsg::Window>& window, Args&&... args, bool useClearAttachments) :
            RenderImGui(window, useClearAttachments)
        {
            (add(args), ...);
        }

        /// std::function signature used for older vsgImGui versions
        using LegacyFunction = std::function<bool()>;

        /// add a GUI rendering component that provides the ImGui calls to render the required GUI elements.
        void add(const LegacyFunction& legacyFunc);

        /// add a child, equivalent to Group::addChild(..) but adds compatibility with the RenderImGui constructor
        void add(vsg::ref_ptr<vsg::Node> child) { addChild(child); }

        void accept(vsg::RecordTraversal& rt) const override;

        //! Execute an ImGui frame natively (bypassing VSG).
        //! @renderFunction Function that will submit ImGui commands.
        static void frame(std::function<void()> renderFunction);

    private:
        virtual ~RenderImGui();

        vsg::ref_ptr<vsg::Device> _device;
        uint32_t _queueFamily;
        vsg::ref_ptr<vsg::Queue> _queue;
        vsg::ref_ptr<vsg::DescriptorPool> _descriptorPool;

        vsg::ref_ptr<vsg::ClearAttachments> _clearAttachments;

        void _init(const vsg::ref_ptr<vsg::Window>& window, bool useClearAttachments);
        void _init(vsg::ref_ptr<vsg::Device> device, uint32_t queueFamily,
            vsg::ref_ptr<vsg::RenderPass> renderPass,
            uint32_t minImageCount, uint32_t imageCount,
            VkExtent2D imageSize, bool useClearAttachments);
        void _uploadFonts();
    };

    // temporary workaround for Dear ImGui's nonexistent sRGB awareness
    extern void ImGuiStyle_sRGB_to_linear(ImGuiStyle& style);

} // namespace vsgImGui

EVSG_type_name(vsgImGui::RenderImGui);
