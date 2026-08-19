/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/ecs/ProjectedTexture.h>
#include <rocky/vsg/Common.h>
#include <string>

namespace ROCKY_NAMESPACE
{
    struct RenderTextureSourceStatus
    {
        enum class State { Ready, Waiting, Failed };
        State state = State::Ready;
        std::string message;
        bool retry = false;
    };

    namespace RenderTextureOrder
    {
        constexpr int Transform = 0;
        constexpr int Mesh = 100;
        constexpr int Line = 200;
        constexpr int Point = 300;
        constexpr int Model = 400;
    }

    /**
     * Optional capability implemented by systems that can draw ECS entities
     * into a RenderTexture job.
     *
     * Keeping this contract separate from System lets non-rendering systems
     * remain unaware of render-to-texture concepts and gives extensions one
     * explicit place to describe rendering, bounds, revisions, and ordering.
     */
    class ROCKY_EXPORT RenderTextureParticipant
    {
    public:
        virtual ~RenderTextureParticipant() = default;

        //! Scene-graph node to traverse for a RenderTexture pass.
        virtual vsg::Node* renderTextureNode() = 0;

        /**
         * Minimal node needed to compile this participant for a new render pass.
         *
         * The default preserves support for arbitrary scene-graph participants.
         * Registry-backed systems should override this when they can expose just
         * their pipeline templates; compiling renderTextureNode() may otherwise
         * collect every entity owned by the system.
         */
        virtual vsg::ref_ptr<vsg::Node> renderTextureCompileNode()
        {
            return vsg::ref_ptr<vsg::Node>(renderTextureNode());
        }

        //! Stable cross-system draw order. Lower values render first.
        virtual int renderTextureOrder() const = 0;

        //! Contribute source bounds used by RenderTexture auto-fitting.
        virtual void expandRenderTextureBounds(
            entt::registry&, entt::entity, RenderTextureBounds&, const SRS&, bool) { }

        //! Contribute persistent bounds/content revisions for a source.
        virtual void contributeRenderTextureRevision(
            entt::registry&, entt::entity, RenderTextureRevision&) { }

        //! Whether this participant has enough source data to render/bound it.
        virtual RenderTextureSourceStatus renderTextureSourceStatus(entt::registry&, entt::entity) const
        {
            return {};
        }
    };

    //! Expands a source point in the same coordinate frame used by rendering.
    //! When requested, the entity Transform (including a non-unit local pose)
    //! is applied after conversion to the render world's coordinates.
    ROCKY_EXPORT void expandRenderTextureSourcePoint(
        entt::registry& registry,
        entt::entity source,
        RenderTextureBounds& bounds,
        const SRS& inputSRS,
        const glm::dvec3& input,
        const SRS& worldSRS,
        bool applySourceTransform);
}
