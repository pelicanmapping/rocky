/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/Common.h>
#include <rocky/vsg/terrain/SurfaceNode.h>
#include <rocky/Image.h>
#include <rocky/TileKey.h>
#include <vsg/io/ReaderWriter.h>
#include <vsg/nodes/PagedLOD.h>
#include <vsg/state/DescriptorBuffer.h>
#include <vsg/state/DescriptorImage.h>
#include <vsg/ui/FrameStamp.h>
#include <filesystem>
#include <shared_mutex>
#include <map>

namespace ROCKY_NAMESPACE
{
    struct TextureData
    {
        std::string name;
        std::shared_ptr<Image> image;
        glm::dmat4 matrix{ 1 };
    };

    enum TextureType
    {
        COLOR,
        COLOR_PARENT,
        ELEVATION,
        NORMAL,
        NUM_TEXTURE_TYPES
    };

    struct TerrainTileDescriptors
    {
        struct Uniforms
        {
            glm::fmat4 elevation_matrix;
            glm::fmat4 color_matrix;
            glm::fmat4 normal_matrix;
            glm::fmat4 model_matrix;
        };
        vsg::ref_ptr<vsg::DescriptorImage> color;
        vsg::ref_ptr<vsg::DescriptorImage> colorParent;
        vsg::ref_ptr<vsg::DescriptorImage> elevation;
        vsg::ref_ptr<vsg::DescriptorImage> normal;
        vsg::ref_ptr<vsg::DescriptorBuffer> uniforms;
    };

    class TerrainTileRenderModel
    {
    public:
        glm::fmat4 modelMatrix;
        TextureData color;
        TextureData elevation;
        TextureData normal;
        TextureData colorParent;

        TerrainTileDescriptors descriptors;

#if 0
        void applyScaleBias(const glm::dmat4& sb)
        {
            if (color.image)
                color.matrix *= sb;
            if (elevation.image)
                elevation.matrix *= sb;
            if (normal.image)
                normal.matrix *= sb;
            if (colorParent.image)
                colorParent.matrix *= sb;

            if (uniformsBuffer)
            {
                auto& uniforms = *reinterpret_cast<TerrainTileDescriptors::Uniforms*>(uniformsBuffer->data());
                uniforms.elevation_matrix = elevation.matrix;
                uniforms.color_matrix = color.matrix;
                uniforms.normal_matrix = normal.matrix;
                uniforms.model_matrix = modelMatrix;
                uniformsBuffer->dirty();
            }
        }
#endif
    };

    /**
    * Represents a single terrain tile in the scene graph, along with its
    * potentially pageable children.
    */
    class ROCKY_EXPORT TerrainTileNode : public vsg::Inherit<vsg::PagedLOD, TerrainTileNode>
    {
    public:
        TileKey key;
        TerrainTileRenderModel renderModel;
        vsg::ref_ptr<SurfaceNode> surface;
        vsg::ref_ptr<vsg::StateGroup> stategroup;

        //inline void inheritFrom(vsg::ref_ptr<TerrainTileNode> parent);
    };

    /**
    * Plugin that reads the 4 child terrain tiles of a parent tile whose key is
    * encoded int he filename.
    */
    class TerrainTileNodeQuadReader : public vsg::Inherit<vsg::ReaderWriter, TerrainTileNodeQuadReader>
    {
    public:
        TerrainTileNodeQuadReader()
        {
            _features.extensionFeatureMap[vsg::Path(".rocky_terrain_tile_parent")] = READ_FILENAME;
        }

        Features _features;

        bool getFeatures(Features& out) const override
        {
            out = _features;
            return true;
        }

        vsg::ref_ptr<vsg::Object> read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const override;

        static vsg::Path makePath(const TileKey& key)
        {
            return vsg::Path(
                std::to_string(key.LOD()) + ',' +
                std::to_string(key.tileX()) + ',' +
                std::to_string(key.tileY()) +
                ".rocky_terrain_tile_parent");
        }

        void tick(const vsg::FrameStamp* fs)
        {
            frameCount = fs ? fs->frameCount : 0;
        }

    private:
        mutable unsigned frameCount = 0;
    };
}
