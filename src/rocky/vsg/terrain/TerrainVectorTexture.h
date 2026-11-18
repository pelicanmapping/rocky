/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/VSGContext.h>
#include <rocky/VectorTileLayer.h>

namespace ROCKY_NAMESPACE
{
    class Map;
    struct TerrainVectorGeometry;

    //! std430 terrain paint record; projection maps carrier UV into an isotropic Mercator frame when requested.
    struct TerrainVectorLayerGPU
    {
        glm::vec4 color, uvToEmX, uvToEmY, bandTransform;
        glm::uvec4 shapeData;
        glm::vec4 projection;
    };
    static_assert(sizeof(TerrainVectorLayerGPU) == 96, "Terrain vector shader layout mismatch");

    /** Immutable snapshot of the ordered vector sources required by a terrain tile. */
    struct TerrainVectorRequest
    {
        struct Source
        {
            VectorTileLayer::Ptr layer;
            TileKey key;
            Revision revision = 0;
            float opacity = 1.0f;
            std::optional<double> styleZoom;
        };
        TileKey key;
        std::size_t signature = 0u;
        std::vector<Source> sources;
    };

    /** Shared analytic surface resource. A child tile holds this resource plus its own UV matrix.
     * Descriptors are immutable after publication; consumers retire their bind commands through VSGContext.
     */
    struct TerrainVectorTexture : public vsg::Inherit<vsg::Object, TerrainVectorTexture>
    {
        TileKey key;
        std::size_t signature = 0u;
        std::uint32_t layerCount = 0u;
        //! Keeps shared atlas geometry alive independently of this revision's paint records.
        std::shared_ptr<const TerrainVectorGeometry> geometry;
        bool reusedGeometry = false;
        vsg::ref_ptr<vsg::DescriptorImage> curves;
        vsg::ref_ptr<vsg::DescriptorImage> bands;
        vsg::ref_ptr<vsg::DescriptorBuffer> layers;
        vsg::ref_ptr<vsg::DescriptorBuffer> grid;
    };

    //! Snapshots source revisions/opacity on update. Destination limits select source ancestors independently of geometry.
    ROCKY_EXPORT TerrainVectorRequest terrainVectorRequest(
        const Map& map, const TileKey& key, std::optional<double> viewStyleZoom = {});

    //! Estimates Mapbox's 512-pixel zoom at the view-center ellipsoid/plane intersection.
    //! Supports geocentric and projected maps; returns no value for invalid/off-map views.
    ROCKY_EXPORT std::optional<double> terrainViewStyleZoom(const SRS& worldSRS,
        const glm::dmat4& modelview, const glm::dmat4& projection, double viewportHeight);

    //! Builds CPU atlas/upload data on a worker, without touching the scene or allocating Vulkan objects.
    //! Empty sources produce transparent coverage. Any source/validation/capacity failure preserves the old resource.
    ROCKY_EXPORT Result<vsg::ref_ptr<TerrainVectorTexture>> buildTerrainVectorTexture(
        const TerrainVectorRequest& request, const IOOptions& io, std::uint32_t maxImageDimension);
}
