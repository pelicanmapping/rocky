/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/Common.h>
#include <vsg/all.h>

namespace ROCKY_NAMESPACE
{
    // Shader binding set and binding points for VSG's view-dependent data.
    // See vsg::ViewDependentState
    constexpr int VSG_VIEW_DEPENDENT_DATA_SET = 1;
    constexpr int VSG_VIEW_DEPENDENT_LIGHTS_BINDING = 0;
    constexpr int VSG_VIEW_DEPENDENT_VIEWPORTS_BINDING = 1;
    

    //! Utilities for helping to set up a graphics pipeline.
    struct PipelineUtils
    {
        static void addViewDependentData(vsg::ref_ptr<vsg::ShaderSet> shaderSet, VkShaderStageFlags stageFlags)
        {
            // override it because we're getting weird VK errors -gw
            stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

            // VSG view-dependent data. You must include it all even if you only intend to use
            // one of the uniforms.
            shaderSet->customDescriptorSetBindings.push_back(
                vsg::ViewDependentStateBinding::create(VSG_VIEW_DEPENDENT_DATA_SET));

            shaderSet->addDescriptorBinding(
                "vsg_lights", "",
                VSG_VIEW_DEPENDENT_DATA_SET,
                VSG_VIEW_DEPENDENT_LIGHTS_BINDING,
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                stageFlags, {});

            // VSG viewport state
            shaderSet->addDescriptorBinding(
                "vsg_viewports", "",
                VSG_VIEW_DEPENDENT_DATA_SET,
                VSG_VIEW_DEPENDENT_VIEWPORTS_BINDING,
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                stageFlags, {});
        }

        static vsg::ref_ptr<vsg::DescriptorSetLayout> getViewDependentDescriptorSetLayout()
        {
            return vsg::DescriptorSetLayout::create(
                vsg::DescriptorSetLayoutBindings{
                    {VSG_VIEW_DEPENDENT_LIGHTS_BINDING, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
                    {VSG_VIEW_DEPENDENT_VIEWPORTS_BINDING, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT}
                }
            );
        }

        static void enableViewDependentData(vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> pipelineConfig)
        {
            pipelineConfig->enableDescriptor("vsg_lights");
            pipelineConfig->enableDescriptor("vsg_viewports");
        }
    };

    class ROCKY_EXPORT ComputePipelineConfigurator : public vsg::Inherit<vsg::Object, ComputePipelineConfigurator>
    {
    public:
        ComputePipelineConfigurator(vsg::ref_ptr<vsg::ShaderSet> in_shaderSet = {});

        void traverse(vsg::Visitor& visitor) override;
        void traverse(vsg::ConstVisitor& visitor) const override;

        uint32_t subpass = 0;
        uint32_t baseAttributeBinding = 0;
        vsg::ref_ptr<vsg::ShaderSet> shaderSet;

        void reset();

        bool enableDescriptor(const std::string& name);
        bool enableTexture(const std::string& name);

        bool assignDescriptor(const std::string& name, vsg::ref_ptr<vsg::Data> data = {}, uint32_t dstArrayElement = 0);
        bool assignDescriptor(const std::string& name, const vsg::BufferInfoList& bufferInfoList, uint32_t dstArrayElement = 0);
        bool assignTexture(const std::string& name, vsg::ref_ptr<vsg::Data> textureData = {}, vsg::ref_ptr<vsg::Sampler> sampler = {}, uint32_t dstArrayElement = 0);
        bool assignTexture(const std::string& name, const vsg::ImageInfoList& imageInfoList, uint32_t dstArrayElement = 0);

        /// set the inherited state which if compatible can hint at the state setup and copying to avoid setting inherited state local subgraph
        void assignInheritedState(const vsg::StateCommands& stateCommands);

        // setup by assign calls
        vsg::ref_ptr<vsg::ShaderCompileSettings> shaderHints;
        vsg::ref_ptr<vsg::DescriptorConfigurator> descriptorConfigurator;
        vsg::StateCommands inheritedState;

        int compare(const vsg::Object& rhs) const override;

        /// initialize state objects
        virtual void init();

        /// copy state objects to StateCommands list, return true if state is added.
        virtual bool copyTo(vsg::StateCommands& stateCommands, vsg::ref_ptr<vsg::SharedObjects> sharedObjects = {});

        /// copy state objects to StateGroup, return true if state is added.
        //virtual bool copyTo(vsg::ref_ptr<vsg::StateGroup> stateGroup, vsg::ref_ptr<vsg::SharedObjects> sharedObjects = {});

        // setup by init()
        std::set<uint32_t> inheritedSets;
        vsg::ref_ptr<vsg::PipelineLayout> layout;
        vsg::ref_ptr<vsg::ComputePipeline> computePipeline;
        vsg::ref_ptr<vsg::BindComputePipeline> bindComputePipeline;

    protected:
        //void _assignShaderSetSettings();
        //void _assignInheritedSets();
    };

    class ROCKY_EXPORT DrawIndexedIndirectSSBO : public vsg::Inherit<vsg::DrawIndexedIndirect, DrawIndexedIndirectSSBO>
    {
    public:
        DrawIndexedIndirectSSBO(vsg::ref_ptr<vsg::BufferInfo> in_bufferInfo, uint32_t in_drawCount, uint32_t in_stride)
        {
            bufferInfo = in_bufferInfo;
            drawCount = in_drawCount;
            stride = in_stride;
        }

        void compile(vsg::Context& context) override
        {
            if (!bufferInfo->buffer && bufferInfo->data)
            {
                createBufferAndTransferData(context, { bufferInfo },
                    VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                    VK_SHARING_MODE_EXCLUSIVE);
            }
        }
    };

    class DescriptorBufferEx : public vsg::Inherit<vsg::DescriptorBuffer, DescriptorBufferEx>
    {
    public:
        explicit DescriptorBufferEx(const vsg::BufferInfoList& in_bufferInfoList, uint32_t dstBinding, uint32_t dstArrayElement, VkDescriptorType descriptorType, VkBufferUsageFlags in_additionalUsageFlags) :
            Inherit(in_bufferInfoList, dstBinding, dstArrayElement, descriptorType),
            additionalUsageFlags(in_additionalUsageFlags)
        {
            //nop
        }

        VkBufferUsageFlags additionalUsageFlags = 0;


        void compile(vsg::Context& context) override
        {
            if (bufferInfoList.empty()) return;

            auto transferTask = context.transferTask.get();

            VkBufferUsageFlags bufferUsageFlags = additionalUsageFlags;
            switch (descriptorType)
            {
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
                bufferUsageFlags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
                break;
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
                bufferUsageFlags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
                break;
            default:
                break;
            }

            bool requiresAssignmentOfBuffers = false;
            for (auto& bufferInfo : bufferInfoList)
            {
                if (bufferInfo->buffer == nullptr) requiresAssignmentOfBuffers = true;
            }

            auto deviceID = context.deviceID;

            if (requiresAssignmentOfBuffers)
            {
                VkDeviceSize alignment = 4;
                const auto& limits = context.device->getPhysicalDevice()->getProperties().limits;
                if (bufferUsageFlags & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
                    alignment = limits.minUniformBufferOffsetAlignment;
                else if (bufferUsageFlags & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
                    alignment = limits.minStorageBufferOffsetAlignment;

                VkDeviceSize totalSize = 0;

                // compute the total size of BufferInfo that needs to be allocated.
                {
                    VkDeviceSize offset = 0;
                    for (auto& bufferInfo : bufferInfoList)
                    {
                        if (bufferInfo->data && !bufferInfo->buffer)
                        {
                            totalSize = offset + bufferInfo->data->dataSize();
                            offset = (alignment == 1 || (totalSize % alignment) == 0) ? totalSize : ((totalSize / alignment) + 1) * alignment;
                            if (bufferInfo->data->dynamic() || transferTask) bufferUsageFlags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
                        }
                    }
                }

                // if required allocate the buffer and reserve slots in it for the BufferInfo
                if (totalSize > 0)
                {
                    auto buffer = vsg::Buffer::create(totalSize, bufferUsageFlags, VK_SHARING_MODE_EXCLUSIVE);
                    for (auto& bufferInfo : bufferInfoList)
                    {
                        if (bufferInfo->data && !bufferInfo->buffer)
                        {
                            auto [allocated, offset] = buffer->reserve(bufferInfo->data->dataSize(), alignment);
                            if (allocated)
                            {
                                bufferInfo->buffer = buffer;
                                bufferInfo->offset = offset;
                                bufferInfo->range = bufferInfo->data->dataSize();
                            }
                            else
                            {
                                vsg::warn("DescriptorBuffer::compile(..) unable to allocate bufferInfo within associated Buffer.");
                            }
                        }
                    }
                }
            }

            for (auto& bufferInfo : bufferInfoList)
            {
                if (bufferInfo->buffer)
                {
                    if (bufferInfo->buffer->compile(context.device))
                    {
                        if (bufferInfo->buffer->getDeviceMemory(deviceID) == nullptr)
                        {
                            auto memRequirements = bufferInfo->buffer->getMemoryRequirements(deviceID);
                            VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
                            auto [deviceMemory, offset] = context.deviceMemoryBufferPools->reserveMemory(memRequirements, flags);
                            if (deviceMemory)
                            {
                                bufferInfo->buffer->bind(deviceMemory, offset);
                            }
                            else
                            {
                                throw vsg::Exception{ "Error: DescriptorBuffer::compile(..) failed to allocate buffer from deviceMemoryBufferPools.", VK_ERROR_OUT_OF_DEVICE_MEMORY };
                            }
                        }
                    }

                    if (!transferTask && bufferInfo->data && bufferInfo->data->getModifiedCount(bufferInfo->copiedModifiedCounts[deviceID]))
                    {
                        bufferInfo->copyDataToBuffer(context.deviceID);
                    }
                }
            }

            if (transferTask) transferTask->assign(bufferInfoList);
        }
    };

    class ZeroBuffer : public vsg::Inherit<vsg::Command, ZeroBuffer>
    {
    public:
        ZeroBuffer(vsg::ref_ptr<vsg::Buffer> in_buffer) :
            buffer(in_buffer) { }

        vsg::ref_ptr<vsg::Buffer> buffer;

        void record(vsg::CommandBuffer& commandBuffer) const override
        {
            if (buffer)
            {
                vkCmdFillBuffer(
                    commandBuffer,
                    buffer->vk(commandBuffer.deviceID),
                    0, // offset
                    buffer->size,
                    0); // data
            }
        }
    };
}
