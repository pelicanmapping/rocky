/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky_vsg/Common.h>

#include <rocky/Image.h>

#include <vsg/state/Sampler.h>
#include <vsg/state/ImageInfo.h>

namespace rocky
{
    struct TextureData
    {
        shared_ptr<Image> image;
        dmat4 matrix{ 1 };
        vsg::ref_ptr<vsg::ImageInfo> texture;
    };

    enum TextureType
    {
        COLOR,
        COLOR_PARENT,
        ELEVATION,
        NORMAL,
        NUM_TEXTURE_TYPES
    };

    struct TileDescriptorModel
    {
        struct Uniforms
        {
            fmat4 elevation_matrix;
            fmat4 color_matrix;
            fmat4 normal_matrix;
            fvec2 elev_texel_coeff;
        };
        vsg::ref_ptr<vsg::DescriptorImage> color;
        vsg::ref_ptr<vsg::DescriptorImage> colorParent;
        vsg::ref_ptr<vsg::DescriptorImage> elevation;
        vsg::ref_ptr<vsg::DescriptorImage> normal;
        vsg::ref_ptr<vsg::DescriptorBuffer> uniforms;
        vsg::ref_ptr<vsg::BindDescriptorSet> bindDescriptorSetCommand;
    };

    class TileRenderModel
    {
    public:
        TextureData color;
        TextureData elevation;
        TextureData normal;
        TextureData colorParent;

        TileDescriptorModel descriptorModel;

        void applyScaleBias(const dmat4& sb)
        {
            if (color.image)
                color.matrix *= sb;
            if (elevation.image)
                elevation.matrix *= sb;
            if (normal.image)
                normal.matrix *= sb;
            if (colorParent.image)
                colorParent.matrix *= sb;
        }
    };
}
