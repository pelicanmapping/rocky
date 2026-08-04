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
}

SharedRenderData::SharedRenderData()
{
#ifdef ROCKY_HAS_DECALS

    // arena to hold all decal textures (shared by all views).
    // All entries are initialized to a valid fallback image.
    auto fallback = makeDefaultImageInfo();
    vsg::ImageInfoList arena(MAX_NUM_DECAL_TEXTURES, fallback);

    decalTextures = vsg::DescriptorImage::create(
        arena,
        BINDING_DECAL_TEXTURES,
        0, // array element
        TYPE_DECAL_TEXTURES);
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
#endif

    vds->descriptorSet = vsg::DescriptorSet::create(old_ds->setLayout, newDescriptors);

    if (lifecycle)
    {
        lifecycle->dispose(old_ds);
        lifecycle->compile(vds->descriptorSet);
    }
}

