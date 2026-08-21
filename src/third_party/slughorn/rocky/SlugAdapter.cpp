#include "SlugAdapter.h"

#include <slughorn/canvas.hpp>
#include <slughorn/serial.hpp>

#include <algorithm>
#include <cmath>
#include <exception>
#include <string>
#include <utility>
#include <vector>

namespace rocky::detail
{
    namespace
    {
        using slughorn::Atlas;
        using slughorn::Color;
        using slughorn::Key;
        using slughorn::Layer;
        using slughorn::canvas::Canvas;
        using slughorn::canvas::LineCap;
        using slughorn::canvas::LineJoin;
        using slughorn::canvas::Path;

        struct PendingLayer
        {
            std::uint32_t owner = 0u;
            Layer layer;
            std::array<float, 4> uvToEmX = { 1.0f, 0.0f, 0.0f, 0.0f };
            std::array<float, 4> uvToEmY = { 0.0f, 1.0f, 0.0f, 0.0f };
            bool isOutline = false;
        };

        bool finite(float value)
        {
            return std::isfinite(value);
        }

        bool valid(const SlugPointInput& point)
        {
            return finite(point.x) && finite(point.y);
        }

        Color makeColor(const std::array<float, 4>& value)
        {
            return Color{ value[0], value[1], value[2], value[3] };
        }

        bool equal(const SlugPointInput& lhs, const SlugPointInput& rhs)
        {
            return lhs.x == rhs.x && lhs.y == rhs.y;
        }

        template<typename PathType>
        bool addContours(
            PathType& path,
            const SlugShapeInput& input,
            bool closeForFill,
            bool mergeConnectedSegments)
        {
            bool added = false;
            bool chainOpen = false;
            SlugPointInput chainEnd;

            for (const auto& contour : input.contours)
            {
                const std::size_t minimum = closeForFill ? 3u : 2u;
                if (contour.points.size() < minimum)
                    continue;

                for (const auto& point : contour.points)
                {
                    if (!valid(point))
                        return false;
                }

                const bool append =
                    mergeConnectedSegments &&
                    !closeForFill &&
                    !contour.closed &&
                    chainOpen &&
                    equal(chainEnd, contour.points.front());

                if (!append)
                {
                    path.moveTo(contour.points.front().x, contour.points.front().y);
                }

                for (std::size_t i = 1u; i < contour.points.size(); ++i)
                    path.lineTo(contour.points[i].x, contour.points[i].y);

                if (closeForFill || contour.closed)
                {
                    path.closePath();
                    chainOpen = false;
                }
                else if (mergeConnectedSegments)
                {
                    chainEnd = contour.points.back();
                    chainOpen = true;
                }

                added = true;
            }

            return added;
        }

        void copyTexture(
            const Atlas::TextureData& source,
            SlugTextureFormat format,
            SlugTextureOutput& destination)
        {
            destination.width = source.width;
            destination.height = source.height;
            destination.format = format;
            destination.bytes = source.bytes;
        }

        std::uint32_t log2(std::uint32_t value)
        {
            std::uint32_t result = 0u;
            while (value > 1u)
            {
                value >>= 1u;
                ++result;
            }
            return result;
        }

        int bandCountForCurveCount(std::size_t curveCount)
        {
            // Match Slughorn's existing one-band-per-two-curves policy for
            // small shapes. Once that policy reaches its old 16-band ceiling,
            // jump directly to all 32 indirection cells. Exact 1/32 bands are
            // nested inside the old 1/16 bands, guaranteeing that a selected
            // band's per-fragment curve list cannot grow.
            const auto automaticCount =
                std::max<std::size_t>(1u, curveCount / 2u);
            const auto count = automaticCount > 16u ?
                std::size_t{ Atlas::INDIRECTION_SIZE } : automaticCount;
            return static_cast<int>(count);
        }

        std::pair<std::vector<slughorn::slug_t>, std::vector<slughorn::slug_t>>
        complexityAwareBandSplits(const Atlas::Curves& curves)
        {
            const int bandCount = bandCountForCurveCount(curves.size());
            return Atlas::computeUniformSplits(curves, bandCount, bandCount);
        }
    }

    bool buildSlugAtlas(
        const SlugAtlasInput& input,
        SlugAtlasOutput& output,
        std::string& error)
    {
        output = {};
        error.clear();

        if (input.textureWidth < 128u ||
            (input.textureWidth & (input.textureWidth - 1u)) != 0u)
        {
            error = "Slughorn atlas width must be a power of two and at least 128 texels";
            return false;
        }

        // A single Slug band must fit in one texture row. Complex feature
        // batches (for example, an MVT road tile) can exceed the requested
        // width, so grow transactionally and rebuild from the original inputs.
        // 4096 is the Vulkan-required minimum maxImageDimension2D.
        std::uint32_t textureWidth = input.textureWidth;
        constexpr std::uint32_t maxAutomaticTextureWidth = 4096u;

        for (;;)
        {
            try
            {
                Atlas atlas{ textureWidth };
                Canvas canvas{ atlas, slughorn::KeyIterator{ "rocky-overlay" } };

                // Every decal fragment supplies the projector's full [0,1] UV.
                // Full-cell metrics preserve that coordinate system instead of
                // tight-fitting each shape across the projector.
                canvas.setAutoMetrics(false);
                canvas.setTolerance(slughorn::TOLERANCE_FINE);
                canvas.setSplitStrategy(complexityAwareBandSplits);

            std::vector<PendingLayer> pending;
            pending.reserve(input.shapes.size() * 2u);

            for (std::size_t index = 0u; index < input.shapes.size(); ++index)
            {
                const auto& shape = input.shapes[index];
                const auto keyStem = "rocky-overlay-" + std::to_string(index);
                const Key key{ keyStem };
                const Color color = makeColor(shape.color);

                for (float channel : shape.color)
                {
                    if (!finite(channel))
                    {
                        error = "Slug shape " + std::to_string(index) + " has a non-finite color";
                        return false;
                    }
                }

                for (float coefficient : shape.uvToEmX)
                {
                    if (!finite(coefficient))
                    {
                        error = "Slug shape " + std::to_string(index) +
                            " has a non-finite UV mapping";
                        return false;
                    }
                }
                for (float coefficient : shape.uvToEmY)
                {
                    if (!finite(coefficient))
                    {
                        error = "Slug shape " + std::to_string(index) +
                            " has a non-finite UV mapping";
                        return false;
                    }
                }

                canvas.beginPath();
                Layer layer;
                bool committed = false;

                switch (shape.kind)
                {
                case SlugShapeKind::Fill:
                    if (addContours(canvas, shape, true, false))
                    {
                        layer = canvas.fill(color, 1.0f, key);
                        committed = layer.key == key;
                    }
                    else
                    {
                        for (const auto& contour : shape.contours)
                        {
                            for (const auto& point : contour.points)
                            {
                                if (!valid(point))
                                {
                                    error = "Slug fill shape " + std::to_string(index) +
                                        " has a non-finite point";
                                    return false;
                                }
                            }
                        }
                    }
                    break;

                case SlugShapeKind::Stroke:
                    if (!finite(shape.strokeWidth) || shape.strokeWidth <= 0.0f)
                    {
                        error = "Slug stroke shape " + std::to_string(index) +
                            " has an invalid width";
                        return false;
                    }

                    if (!finite(shape.outlineWidth) || shape.outlineWidth < 0.0f)
                    {
                        error = "Slug stroke shape " + std::to_string(index) +
                            " has an invalid outline width";
                        return false;
                    }

                    if (shape.outlineWidth > 0.0f)
                    {
                        for (float channel : shape.outlineColor)
                        {
                            if (!finite(channel))
                            {
                                error = "Slug stroke shape " + std::to_string(index) +
                                    " has a non-finite outline color";
                                return false;
                            }
                        }
                    }

                    {
                        Path centerline;
                        if (!addContours(
                            centerline,
                            shape,
                            false,
                            input.mergeConnectedLineSegments))
                        {
                            for (const auto& contour : shape.contours)
                            {
                                for (const auto& point : contour.points)
                                {
                                    if (!valid(point))
                                    {
                                        error = "Slug stroke shape " + std::to_string(index) +
                                            " has a non-finite point";
                                        return false;
                                    }
                                }
                            }
                            break;
                        }

                        if (shape.outlineWidth > 0.0f)
                        {
                            const float outerWidth =
                                shape.strokeWidth + 2.0f * shape.outlineWidth;
                            const Key outlineKey{ keyStem + "-outline" };
                            Layer outlineLayer;

                            if (shape.useOpaqueOutlineCasing)
                            {
                                // Opaque cores completely cover the casing's
                                // interior, so do not author the redundant
                                // reversed inner boundary used by a ring.
                                outlineLayer = canvas.stroke(
                                    centerline,
                                    outerWidth,
                                    makeColor(shape.outlineColor),
                                    1.0f,
                                    outlineKey,
                                    {},
                                    LineJoin::Round,
                                    LineCap::Round,
                                    4.0f);
                            }
                            else
                            {
                                Path ring = centerline;
                                Path hole = centerline;
                                if (!ring.strokePath(
                                        outerWidth, false,
                                        LineJoin::Round, LineCap::Round, 4.0f) ||
                                    !hole.strokePath(
                                        shape.strokeWidth, true,
                                        LineJoin::Round, LineCap::Round, 4.0f))
                                {
                                    error = "Slug stroke shape " + std::to_string(index) +
                                        " could not construct its outline";
                                    return false;
                                }

                                ring.addPath(hole);
                                outlineLayer = canvas.fill(
                                    ring,
                                    makeColor(shape.outlineColor),
                                    1.0f,
                                    outlineKey);
                            }

                            if (outlineLayer.key != outlineKey)
                            {
                                error = "Slug stroke shape " + std::to_string(index) +
                                    " could not commit its outline";
                                return false;
                            }
                            pending.push_back(PendingLayer{
                                shape.owner,
                                std::move(outlineLayer),
                                shape.uvToEmX,
                                shape.uvToEmY,
                                true });
                        }

                        layer = canvas.stroke(
                            centerline,
                            shape.strokeWidth,
                            color,
                            1.0f,
                            key,
                            {},
                            LineJoin::Round,
                            LineCap::Round,
                            4.0f);
                        committed = layer.key == key;
                        if (!committed)
                        {
                            error = "Slug stroke shape " + std::to_string(index) +
                                " could not commit its core";
                            return false;
                        }
                    }
                    break;

                case SlugShapeKind::Circles:
                    for (const auto& circle : shape.circles)
                    {
                        if (!finite(circle.x) || !finite(circle.y) ||
                            !finite(circle.radius) || circle.radius <= 0.0f)
                        {
                            error = "Slug circle shape " + std::to_string(index) +
                                " has invalid geometry";
                            return false;
                        }

                        canvas.circle(circle.x, circle.y, circle.radius);
                        committed = true;
                    }

                    if (committed)
                    {
                        layer = canvas.fill(color, 1.0f, key);
                        committed = layer.key == key;
                    }
                    break;
                }

                if (committed)
                    pending.push_back(PendingLayer{
                        shape.owner,
                        std::move(layer),
                        shape.uvToEmX,
                        shape.uvToEmY });
            }

            atlas.build();

            const bool exportAttempted = !input.exportPath.empty();
            bool exportSucceeded = false;
            std::string exportMessage;
            if (exportAttempted)
            {
                try
                {
                    slughorn::serial::write(atlas, input.exportPath);
                    exportSucceeded = true;
                    exportMessage = "Wrote " + input.exportPath;
                }
                catch (const std::exception& e)
                {
                    // Diagnostic export must not invalidate an otherwise good
                    // runtime atlas.
                    exportMessage = e.what();
                }
                catch (...)
                {
                    exportMessage = "Unknown exception while writing " + input.exportPath;
                }
            }

            const auto& curves = atlas.getCurveTextureData();
            const auto& bands = atlas.getBandTextureData();

            if (curves.format != Atlas::TextureData::Format::RGBA32F ||
                bands.format != Atlas::TextureData::Format::RGBA16UI)
            {
                error = "Slughorn returned unexpected atlas texture formats";
                return false;
            }

            SlugAtlasOutput result;
            copyTexture(curves, SlugTextureFormat::RGBA32F, result.curveTexture);
            copyTexture(bands, SlugTextureFormat::RGBA16UI, result.bandTexture);
            result.textureWidthLog2 = log2(textureWidth);
            result.indirectionSize = Atlas::INDIRECTION_SIZE;
            result.exportAttempted = exportAttempted;
            result.exportSucceeded = exportSucceeded;
            result.exportMessage = std::move(exportMessage);
            result.layers.reserve(pending.size());

            for (const auto& pendingLayer : pending)
            {
                const auto shape = atlas.getShape(pendingLayer.layer.key);
                if (!shape)
                {
                    error = "Slughorn atlas is missing a committed shape";
                    return false;
                }

                const float inverseScale = pendingLayer.layer.scale != 0.0f ?
                    1.0f / pendingLayer.layer.scale : 1.0f;

                SlugLayerOutput out;
                out.owner = pendingLayer.owner;
                out.isOutline = pendingLayer.isOutline;
                out.color = {
                    pendingLayer.layer.color.r,
                    pendingLayer.layer.color.g,
                    pendingLayer.layer.color.b,
                    pendingLayer.layer.color.a
                };
                out.uvToEmX = {
                    pendingLayer.uvToEmX[0] * inverseScale,
                    pendingLayer.uvToEmX[1] * inverseScale,
                    (pendingLayer.uvToEmX[2] - pendingLayer.layer.transform.x) * inverseScale,
                    0.0f
                };
                out.uvToEmY = {
                    pendingLayer.uvToEmY[0] * inverseScale,
                    pendingLayer.uvToEmY[1] * inverseScale,
                    (pendingLayer.uvToEmY[2] - pendingLayer.layer.transform.y) * inverseScale,
                    0.0f
                };
                out.bandTransform = {
                    shape->bandScaleX,
                    shape->bandScaleY,
                    shape->bandOffsetX,
                    shape->bandOffsetY
                };
                out.shapeData = {
                    shape->bandTexX,
                    shape->bandTexY,
                    shape->bandMaxX,
                    shape->bandMaxY
                };
                result.layers.emplace_back(std::move(out));
            }

                output = std::move(result);
                return true;
            }
            catch (const std::exception& e)
            {
                const std::string message = e.what();
                const bool rowTooNarrow =
                    message.find("does not fit in a texture row") != std::string::npos;

                if (rowTooNarrow && textureWidth < maxAutomaticTextureWidth)
                {
                    textureWidth *= 2u;
                    continue;
                }

                output = {};
                error = message;
                return false;
            }
            catch (...)
            {
                output = {};
                error = "Unknown exception while building the Slughorn atlas";
                return false;
            }
        }
    }
}
