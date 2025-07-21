/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#include "WidgetImage.h"

#if defined(ROCKY_HAS_IMGUI)

#include <rocky/vsg/VSGUtils.h>
#include <imgui_impl_vulkan.h>

using namespace ROCKY_NAMESPACE;


struct WidgetImage::Internal
{
    vsg::ref_ptr<vsg::DescriptorSet> descriptorSet;
};


WidgetImage::WidgetImage(Image::Ptr image) :
    vsg::Inherit<vsg::Compilable, WidgetImage>()
{
    if (_internal)
        delete _internal;

    _internal = new Internal();

    _image = image;

    auto data = util::wrapImageData(image);

    auto sampler = vsg::Sampler::create();
    sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler->maxLod = 9.0f;

    vsg::DescriptorSetLayoutBindings bindings{
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr} };

    auto dsl = vsg::DescriptorSetLayout::create(bindings);

    auto texture = vsg::DescriptorImage::create(sampler, data, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    _internal->descriptorSet = vsg::DescriptorSet::create(dsl, vsg::Descriptors{ texture });
}

void
WidgetImage::compile(vsg::Context& context)
{
    if (_internal && _internal->descriptorSet)
    {
        _internal->descriptorSet->compile(context);
        _compiled = true;
    }
}

ImTextureID
WidgetImage::id(std::uint32_t deviceID) const
{
    return _compiled ?
        reinterpret_cast<ImTextureID>(_internal->descriptorSet->vk(deviceID)) :
        ImTextureID{};
}



#endif
