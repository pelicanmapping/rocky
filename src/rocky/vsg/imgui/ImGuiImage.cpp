/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#include "ImGuiImage.h"

#if defined(ROCKY_HAS_IMGUI)

#include <rocky/vsg/VSGUtils.h>

using namespace ROCKY_NAMESPACE;


struct ImGuiImage::Internal
{
    // Viewer::compile will not compile a descriptorset directly, so we need a holder that
    // implements Compilable...
    struct Holder : public vsg::Inherit<vsg::Compilable, Holder> {
        void compile(vsg::Context& c) override {
            descriptorSet->compile(c);
        }
        vsg::ref_ptr<vsg::DescriptorSet> descriptorSet;
    };

    vsg::ref_ptr<Holder> compilable;

    Internal() {
        compilable = Holder::create();
    }
};


ImGuiImage::ImGuiImage(Image::Ptr image, VSGContext vsg)
{
    ROCKY_SOFT_ASSERT(image && vsg);
    if (!image || !vsg)
        return;

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

    _internal->compilable->descriptorSet = vsg::DescriptorSet::create(dsl, vsg::Descriptors{ texture });

    vsg->compile(_internal->compilable);
}

ImTextureID
ImGuiImage::id(std::uint32_t deviceID) const
{
    return _internal ?
        reinterpret_cast<ImTextureID>(_internal->compilable->descriptorSet->vk(deviceID)) :
        ImTextureID{};
}



#endif
