/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */

#include "PipelineState.h"

using namespace ROCKY_NAMESPACE;

StreamingGPUBuffer::StreamingGPUBuffer(std::uint32_t binding, VkDeviceSize size, VkBufferUsageFlags in_usage, VkSharingMode in_sharingMode) :
    usage_flags(in_usage),
    sharing_mode(in_sharingMode)
{
    if (size > 0)
    {
        _data = vsg::ubyteArray::create(size);
    }

    ssbo = vsg::BufferInfo::create();

    descriptor = DescriptorBufferEx::create(
        vsg::BufferInfoList{ ssbo },
        binding,
        0, // array index
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        usage_flags,
        false);
}


void
StreamingGPUBuffer::compile(vsg::Context& context)
{
    if (ssbo->buffer == nullptr)
    {
        // the device-side-only SSBO:
        ssbo->buffer = vsg::createBufferAndMemory(
            context.device,
            _data->dataSize(),
            usage_flags | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            sharing_mode,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        ssbo->offset = 0;
        ssbo->range = _data->dataSize();

        // the CPU-mapped staging buffer:
        staging = vsg::createBufferAndMemory(
            context.device,
            _data->dataSize(),
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            sharing_mode,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        ssbo->buffer->compile(context);
        staging->compile(context);

        dirty_region = VkBufferCopy{ 0, 0, _data->dataSize() };
    }
}

void
StreamingGPUBuffer::record(vsg::CommandBuffer& commandBuffer) const
{
    if (dirty_region.size > 0)
    {
        auto* dm = staging->getDeviceMemory(commandBuffer.deviceID);
        if (dm)
        {
            void* mapped_data = nullptr;

            VkResult result = dm->map(
                staging->getMemoryOffset(commandBuffer.deviceID) + dirty_region.srcOffset,
                dirty_region.size,
                0, // flags
                &mapped_data);

            ROCKY_SOFT_ASSERT_AND_RETURN(result == 0, void());

            char* dst = reinterpret_cast<char*>(mapped_data);
            const char* src = reinterpret_cast<const char*>(_data->dataPointer()) + dirty_region.srcOffset;

            std::memcpy(dst, src, dirty_region.size);

            dm->unmap();
        }

        vkCmdCopyBuffer(
            commandBuffer,
            staging->vk(commandBuffer.deviceID),
            ssbo->buffer->vk(commandBuffer.deviceID),
            1, &dirty_region);

        dirty_region = VkBufferCopy{ 0, 0, 0 };
    }
}


void
DescriptorBufferEx::compile(vsg::Context& context)
{
    if (!compileAndTransferRequired) return; // Rocky

    if (bufferInfoList.empty()) return;

    auto transferTask = context.transferTask.get();
    
    VkBufferUsageFlags bufferUsageFlags = additionalUsageFlags;
    switch (descriptorType)
    {
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        bufferUsageFlags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT; // Rocky
        break;
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        bufferUsageFlags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; // Rocky
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
