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
    /*
     * This is a producer boundary, not Rocky's public Slug API. SlugSystem
     * fills the input records below; the C++20 implementation translates them
     * into Slughorn Canvas calls and returns renderer-neutral atlas bytes and
     * placement metadata. Keeping the header free of SDK types prevents the
     * private dependency's C++20 requirement from propagating into Rocky.
     */

    //! One point in the bounded coordinate space used to author a shape.
    struct SlugPointInput
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    //! One path contour. Stroke contours may be open; fill contours are closed
    //! by the adapter even when closed is false.
    struct SlugContourInput
    {
        std::vector<SlugPointInput> points;
        bool closed = false;
    };

    //! Compact circle primitive used for PointGeometry batches.
    struct SlugCircleInput
    {
        float x = 0.0f;
        float y = 0.0f;
        float radius = 0.0f;
    };

    //! Canvas operation used to commit a SlugShapeInput.
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
        // unused}. Identity preserves the default normalized coordinate space.
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
        // Power-of-two texture width. Curve pairs and band curve lists may
        // span rows; the shader uses the returned logarithm to wrap addresses.
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
        RG16UI
    };

    //! Owned byte copy of one texture emitted by Atlas::build().
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
