#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Rocky's private, C++17-compatible boundary around the C++20 Slughorn SDK.
// Do not include Slughorn headers here: this header is consumed by the Rocky
// target, whose public language requirement remains C++17.
namespace rocky::detail
{
    struct SlugPointInput
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    struct SlugContourInput
    {
        std::vector<SlugPointInput> points;
        bool closed = false;
    };

    struct SlugCircleInput
    {
        float x = 0.0f;
        float y = 0.0f;
        float radius = 0.0f;
    };

    enum class SlugShapeKind : std::uint8_t
    {
        Fill,
        Stroke,
        Circles
    };

    struct SlugShapeInput
    {
        // Opaque value copied to the corresponding output layer. Rocky uses it
        // to associate an atlas shape with its overlay entity.
        std::uint32_t owner = 0u;

        SlugShapeKind kind = SlugShapeKind::Fill;
        std::vector<SlugContourInput> contours;
        std::vector<SlugCircleInput> circles;

        // Affine mapping from projector UV to the coordinates used to author
        // this shape. Each row stores {u coefficient, v coefficient, offset,
        // unused}. Identity preserves the legacy normalized coordinate space.
        std::array<float, 4> uvToEmX = { 1.0f, 0.0f, 0.0f, 0.0f };
        std::array<float, 4> uvToEmY = { 0.0f, 1.0f, 0.0f, 0.0f };

        // Used only for Stroke shapes. Coordinates and width are expressed in
        // the same bounded authoring coordinate space.
        float strokeWidth = 1.0f;

        // Optional visible outline thickness outside a Stroke shape.
        float outlineWidth = 0.0f;
        std::array<float, 4> outlineColor = { 0.0f, 0.0f, 0.0f, 1.0f };

        // When the complete composition is known to be opaque, author the
        // outline as one full-width casing behind the core instead of a
        // two-boundary ring. This reduces Slughorn curve work.
        bool useOpaqueOutlineCasing = false;

        // Linear RGBA.
        std::array<float, 4> color = { 1.0f, 1.0f, 1.0f, 1.0f };
    };

    struct SlugAtlasInput
    {
        // Initial power-of-two width. The adapter may grow it when a shape's
        // largest band cannot fit in one row; the actual logarithm is returned.
        std::uint32_t textureWidth = 512u;

        // Experimental authoring optimization. This only joins consecutive
        // stroke contours when the previous endpoint exactly equals the next
        // starting point.
        bool mergeConnectedLineSegments = true;

        std::vector<SlugShapeInput> shapes;

        // Optional diagnostic export. The C++20 adapter writes the exact atlas
        // it built before converting it into Rocky's GPU payload.
        std::string exportPath;
    };

    enum class SlugTextureFormat : std::uint8_t
    {
        RGBA32F,
        RGBA16UI
    };

    struct SlugTextureOutput
    {
        std::uint32_t width = 0u;
        std::uint32_t height = 0u;
        SlugTextureFormat format = SlugTextureFormat::RGBA32F;
        std::vector<std::uint8_t> bytes;
    };

    struct SlugLayerOutput
    {
        std::uint32_t owner = 0u;
        std::array<float, 4> color = { 1.0f, 1.0f, 1.0f, 1.0f };

        //! True for the outer ring generated ahead of a stroke core.
        bool isOutline = false;

        // em.x = dot({uv,1}, uvToEmX.xyz), likewise for Y.
        std::array<float, 4> uvToEmX = { 1.0f, 0.0f, 0.0f, 0.0f };
        std::array<float, 4> uvToEmY = { 0.0f, 1.0f, 0.0f, 0.0f };

        // bandCoord = em * bandTransform.xy + bandTransform.zw
        std::array<float, 4> bandTransform = { 0.0f, 0.0f, 0.0f, 0.0f };

        // band texture X/Y followed by maximum band index X/Y.
        std::array<std::uint32_t, 4> shapeData = { 0u, 0u, 0u, 0u };
    };

    struct SlugAtlasOutput
    {
        SlugTextureOutput curveTexture;
        SlugTextureOutput bandTexture;
        std::uint32_t textureWidthLog2 = 0u;
        std::uint32_t indirectionSize = 0u;
        std::vector<SlugLayerOutput> layers;

        bool exportAttempted = false;
        bool exportSucceeded = false;
        std::string exportMessage;
    };

    // Builds a complete atlas transactionally. On failure, output is reset and
    // error contains a diagnostic. No Slughorn type crosses this boundary.
    bool buildSlugAtlas(
        const SlugAtlasInput& input,
        SlugAtlasOutput& output,
        std::string& error);
}
