/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/Common.h>

namespace ROCKY_NAMESPACE
{
    // Shader binding set and binding points for VSG's view-dependent data.
    // See vsg::ViewDependentState
    constexpr int VSG_VIEW_DEPENDENT_DESCRIPTOR_SET_INDEX = 1;
    constexpr int VSG_VIEW_DEPENDENT_LIGHTS_BINDING = 0;
    constexpr int VSG_VIEW_DEPENDENT_VIEWPORTS_BINDING = 1;
    

    //! Utilities for helping to set up a graphics pipeline.
    struct PipelineUtils
    {
        static void addViewDependentData(vsg::ShaderSet* shaderSet, VkShaderStageFlags stageFlags)
        {
            // override it because we're getting weird VK errors -gw
            stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

            // VSG view-dependent data. You must include it all even if you only intend to use
            // one of the uniforms.
            shaderSet->customDescriptorSetBindings.push_back(
                vsg::ViewDependentStateBinding::create(VSG_VIEW_DEPENDENT_DESCRIPTOR_SET_INDEX));

            shaderSet->addDescriptorBinding(
                "vsg_lights", "",
                VSG_VIEW_DEPENDENT_DESCRIPTOR_SET_INDEX,
                VSG_VIEW_DEPENDENT_LIGHTS_BINDING,
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                stageFlags, {});

            // VSG viewport state
            shaderSet->addDescriptorBinding(
                "vsg_viewports", "",
                VSG_VIEW_DEPENDENT_DESCRIPTOR_SET_INDEX,
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


    /**
    * Extends vsg::DescriptorBuffer to support
    * (a) additional usage flags for the buffer
    * (b) a flag to indicate whether the buffer needs to be compiled and transferred; set this to false if you are already compiling and copying the buffer elsewhere.
    */
    class ROCKY_EXPORT DescriptorBufferEx : public vsg::Inherit<vsg::DescriptorBuffer, DescriptorBufferEx>
    {
    public:
        explicit DescriptorBufferEx(const vsg::BufferInfoList& in_bufferInfoList, uint32_t dstBinding, uint32_t dstArrayElement, VkDescriptorType descriptorType, VkBufferUsageFlags in_additionalUsageFlags, bool in_compileAndTransferRequired) :
            Inherit(in_bufferInfoList, dstBinding, dstArrayElement, descriptorType),
            additionalUsageFlags(in_additionalUsageFlags),
            compileAndTransferRequired(in_compileAndTransferRequired)
        {
            //nop
        }

        VkBufferUsageFlags additionalUsageFlags = 0;

        bool compileAndTransferRequired = true;

        void compile(vsg::Context& context) override;
    };


    /**
    * A dynamic buffer that you can update on the GPU from CPU memory.
    */
    class ROCKY_EXPORT StreamingGPUBuffer : public vsg::Inherit<vsg::Command, StreamingGPUBuffer>
    {
    public:
        //! The GPU-side buffer, if you need it
        vsg::ref_ptr<vsg::BufferInfo> ssbo;

        //! The descriptor binding the SSBO to the binding point you specified in the constructor
        vsg::ref_ptr<vsg::DescriptorBuffer> descriptor;

        //! Construct a StreamingGPUBuffer
        //! @param binding The binding point for the buffer in the shader
        //! @param size The size of the buffer in bytes
        //! @param in_usage The usage flags for the buffer (e.g. VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
        //! @param in_sharingMode The sharing mode for the buffer
        StreamingGPUBuffer(std::uint32_t binding, VkDeviceSize size, VkBufferUsageFlags in_usage, VkSharingMode in_sharingMode = VK_SHARING_MODE_EXCLUSIVE);

        //! Access the array of data elements so you can update it.
        //! Call dirty(...) after changing the data to force it to sync to the GPU.
        template<class T>
        inline T* data() {
            return static_cast<T*>(_data->dataPointer());
        }

        //! Mark the entire buffer dirty; this will cause it to stream to the GPU
        //! on the next record traversal
        void dirty() {
            dirty_region = VkBufferCopy{ 0, 0, _data->dataSize() };
        }

        //! Mark a region of the buffer dirty; this will cause it to stream that
        //! region to the GPU on the next record traversal
        void dirty(VkDeviceSize offset, VkDeviceSize range) {
            dirty_region = VkBufferCopy{ offset, offset, range };
        }

    public:

        void compile(vsg::Context& context) override;

        void record(vsg::CommandBuffer& commandBuffer) const override;

    protected:
        vsg::ref_ptr<vsg::Data> _data;
        VkBufferUsageFlags usage_flags;
        VkSharingMode sharing_mode;
        vsg::ref_ptr<vsg::Buffer> staging;
        mutable VkBufferCopy dirty_region = VkBufferCopy{ 0, 0, 0 };
    };
}
