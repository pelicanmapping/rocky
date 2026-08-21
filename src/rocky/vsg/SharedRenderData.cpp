/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "SharedRenderData.h"
#include "ShaderDefines.h"
#include <rocky/vsg/VSGUtils.h>
#include <rocky/Color.h>

using namespace ROCKY_NAMESPACE;

namespace
{
    vsg::ref_ptr<vsg::ImageInfo> makeDefaultImageInfo(vsg::ref_ptr<vsg::Sampler> sampler = {})
    {
        const int d = 16;
        auto image = Image::create(Image::R8G8B8A8_UNORM, d, d);
        image->fill(Color(0, 0, 0, 0));
        for (int i = 0; i < d; ++i)
        {
            image->write(StockColor::Red, i, i);
            image->write(StockColor::Red, i, d - i - 1);
        }

        auto imageData = moveImageToVSG(image);

        if (!sampler)
        {
            sampler = vsg::Sampler::create();
        }

        return vsg::ImageInfo::create(sampler, imageData, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

#ifdef ROCKY_HAS_SLUGHORN
    vsg::ref_ptr<vsg::Sampler> makeSlugAtlasSampler()
    {
        auto sampler = vsg::Sampler::create();
        sampler->minFilter = VK_FILTER_NEAREST;
        sampler->magFilter = VK_FILTER_NEAREST;
        sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler->minLod = 0.0f;
        sampler->maxLod = 0.0f;
        return sampler;
    }

    vsg::ref_ptr<vsg::ImageInfo> makeDefaultSlugCurveImageInfo(
        vsg::ref_ptr<vsg::Sampler> sampler)
    {
        auto data = vsg::vec4Array2D::create(
            1, 1,
            vsg::vec4(0.0f, 0.0f, 0.0f, 0.0f),
            vsg::Data::Properties{ VK_FORMAT_R32G32B32A32_SFLOAT });
        return vsg::ImageInfo::create(
            sampler, data, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    vsg::ref_ptr<vsg::ImageInfo> makeDefaultSlugBandImageInfo(
        vsg::ref_ptr<vsg::Sampler> sampler)
    {
        auto data = vsg::usvec2Array2D::create(
            1, 1,
            vsg::usvec2(0u, 0u),
            vsg::Data::Properties{ VK_FORMAT_R16G16_UINT });
        return vsg::ImageInfo::create(
            sampler, data, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
#endif
}

SharedRenderData::SharedRenderData()
{
#ifdef ROCKY_HAS_DECALS
    configureProjectedTextureCapacity(DEFAULT_PROJECTED_TEXTURE_CAPACITY);
#endif
}

void
SharedRenderData::configureProjectedTextureCapacity(std::uint32_t capacity)
{
#ifdef ROCKY_HAS_DECALS
    capacity = std::max(1u, capacity);
    if (decalTextures && decalTextures->imageInfoList.size() == capacity
#ifdef ROCKY_HAS_SLUGHORN
        &&
        slugCurveTexture && slugCurveTexture->imageInfoList.size() == capacity &&
        slugBandTexture && slugBandTexture->imageInfoList.size() == capacity
#endif
        )
        return;

    // Arena shared by all views. Every entry must contain a valid descriptor,
    // even when no projected texture currently owns the slot.
    auto fallback = makeDefaultImageInfo();
    vsg::ImageInfoList arena(capacity, fallback);
    decalTextures = vsg::DescriptorImage::create(
        arena,
        BINDING_DECAL_TEXTURES,
        0,
        TYPE_DECAL_TEXTURES);

#ifdef ROCKY_HAS_SLUGHORN
    auto slugSampler = makeSlugAtlasSampler();
    auto curveFallback = makeDefaultSlugCurveImageInfo(slugSampler);
    auto bandFallback = makeDefaultSlugBandImageInfo(slugSampler);
    slugCurveTexture = vsg::DescriptorImage::create(
        vsg::ImageInfoList(capacity, curveFallback),
        BINDING_SLUG_CURVE_TEXTURE,
        0,
        TYPE_SLUG_CURVE_TEXTURE);
    slugBandTexture = vsg::DescriptorImage::create(
        vsg::ImageInfoList(capacity, bandFallback),
        BINDING_SLUG_BAND_TEXTURE,
        0,
        TYPE_SLUG_BAND_TEXTURE);
#endif
#else
    (void)capacity;
#endif
}

std::uint32_t
SharedRenderData::projectedTextureCapacity() const
{
#ifdef ROCKY_HAS_DECALS
    return decalTextures ? static_cast<std::uint32_t>(decalTextures->imageInfoList.size()) : 0u;
#else
    return 0u;
#endif
}

void
SharedRenderData::dirtySharedDescriptors()
{
    ++revision;
}

void
SharedRenderData::rebuildVdsDescriptorSet(ViewIDType viewID, ObjectLifecycle* lifecycle)
{
    auto& vds = viewDependentState[viewID];
    ++vds->revision;

    auto old_ds = vds->descriptorSet;

    auto numRockyDescriptors = 3; // renderParams, frustumParams, frustums
#ifdef ROCKY_HAS_DECALS
    numRockyDescriptors += 2; // decals, decalTiles
#ifdef ROCKY_HAS_SLUGHORN
    ++numRockyDescriptors; // slugLayers
#endif
#endif

    vsg::Descriptors newDescriptors;
    for(int i=0; i<old_ds->descriptors.size() - numRockyDescriptors; ++i)
        newDescriptors.emplace_back(old_ds->descriptors[i]);

    newDescriptors.emplace_back(vds->renderParamsBuf);
    newDescriptors.emplace_back(vds->frustumParamsBuf);
    newDescriptors.emplace_back(vds->frustumsBuf);
#ifdef ROCKY_HAS_DECALS
    newDescriptors.emplace_back(vds->decalsBuf);
    newDescriptors.emplace_back(vds->decalTilesBuf);
#ifdef ROCKY_HAS_SLUGHORN
    newDescriptors.emplace_back(vds->slugLayersBuf);
#endif
#endif

    vds->descriptorSet = vsg::DescriptorSet::create(old_ds->setLayout, newDescriptors);

    if (lifecycle)
    {
        lifecycle->dispose(old_ds);
        lifecycle->compile(vds->descriptorSet);
    }
}

