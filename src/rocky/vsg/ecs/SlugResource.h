/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Color.h>
#include <rocky/ecs/Component.h>
#include <vsg/state/ImageInfo.h>
#include <string>
#include <vector>

namespace ROCKY_NAMESPACE
{
    /**
     * GPU-facing metadata for one vector layer in an overlay's Slug atlas.
     *
     * Atlas texels alone are insufficient to evaluate a shape: the shader also
     * needs the projector-UV-to-authoring transform and the location of that
     * shape's band tables. DecalSystem copies these fields into SlugLayerGPU.
     */
    struct SlugLayerResource
    {
        //! Shape color before Overlay/ProjectedTexture modulation.
        Color color = StockColor::White;

        //! Outer stroke/casing; emitted before non-outline layers across payloads.
        bool isOutline = false;

        //! Affine rows converting projector UV into Slughorn em coordinates.
        glm::fvec4 uvToEmX = { 1.0f, 0.0f, 0.0f, 0.0f };
        glm::fvec4 uvToEmY = { 0.0f, 1.0f, 0.0f, 0.0f };

        //! Converts em coordinates into band-space coordinates.
        glm::fvec4 bandTransform = { 0.0f, 0.0f, 0.0f, 0.0f };

        //! Band texture X/Y followed by maximum band index X/Y.
        glm::uvec4 shapeData = { 0u, 0u, 0u, 0u };
    };

    /**
     * Renderer-private result of encoding one Overlay into its Slug atlas.
     * This deliberately contains no Slughorn SDK types.
     */
    struct SlugResource : public Component<SlugResource>
    {
        //! The curve/band atlas pair owned by this overlay payload.
        vsg::ref_ptr<vsg::ImageInfo> curveTexture;
        vsg::ref_ptr<vsg::ImageInfo> bandTexture;

        //! Ordered layers stored in the atlas. DecalSystem groups outlines
        //! ahead of cores when it writes the per-view layer buffer.
        std::vector<SlugLayerResource> layers;

        //! Runtime atlas width and indirection-table contract consumed by the
        //! shader. Width is logarithmic because band offsets wrap by bit shift.
        std::uint32_t textureWidthLog2 = 0u;
        std::uint32_t indirectionSize = 0u;

        //! Ready means the atlas pair and layer metadata form a valid producer
        //! result. Descriptor residency is tracked separately by DecalSystem.
        bool ready = false;
        //! Producer failure/status text when ready is false.
        std::string message;

        //! Changes whenever this entity's published layer set changes.
        std::uint64_t revision = 0u;

        //! Identifies this payload's atlas revision.
        std::uint64_t atlasGeneration = 0u;

        //! Producer bookkeeping; consumers should use revision and ready.
        std::size_t sourceSignature = 0u;
        bool sourceSignatureValid = false;

        /**
         * Renderer-private diagnostic export handshake. Set exportPath to
         * request a .slug/.slugb snapshot of this overlay's atlas. The Slug
         * system clears it after one attempt without replacing an unchanged
         * live GPU resource.
         */
        std::string exportPath;
        bool exportSucceeded = false;
        std::string exportMessage;
    };
}
