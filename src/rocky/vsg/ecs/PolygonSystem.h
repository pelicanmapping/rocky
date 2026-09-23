/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include "ECSNode.h"
#include "RenderTextureParticipant.h"
#include <rocky/ecs/Polygon.h>

namespace ROCKY_NAMESPACE
{
    /**
     * Maintains the ordinary-rendering representation of Polygon components
     * and exposes their source bounds to render-to-texture projector fitting.
     */
    class ROCKY_EXPORT PolygonSystemNode :
        public vsg::Inherit<detail::SimpleSystemNodeBase, PolygonSystemNode>,
        public RenderTextureParticipant
    {
    public:
        PolygonSystemNode(Registry& registry);

        RenderTextureParticipant* renderTextureParticipant() override { return this; }
        vsg::Node* renderTextureNode() override { return nullptr; }
        vsg::ref_ptr<vsg::Node> renderTextureCompileNode() override { return {}; }
        int renderTextureOrder() const override { return RenderTextureOrder::Polygon; }
        RenderTextureSourceStatus renderTextureSourceStatus(
            entt::registry&, entt::entity) const override;
        void expandRenderTextureBounds(
            entt::registry&, entt::entity, RenderTextureBounds&, const SRS&, bool) override;
        void contributeRenderTextureRevision(
            entt::registry&, entt::entity, RenderTextureRevision&) override;

        void update(VSGContext) override;

    private:
        void on_construct_Polygon(entt::registry&, entt::entity);
        void on_update_Polygon(entt::registry&, entt::entity);
        void on_destroy_Polygon(entt::registry&, entt::entity);
        //! Retires derived resources even when whole-entity destruction removes the adapter before Polygon.
        void on_destroy_PolygonMeshAdapter(entt::registry&, entt::entity);
        void on_construct_PolygonGeometry(entt::registry&, entt::entity);
        void on_update_PolygonGeometry(entt::registry&, entt::entity);
        void on_destroy_PolygonGeometry(entt::registry&, entt::entity);
        void on_construct_PolygonStyle(entt::registry&, entt::entity);
        void on_update_PolygonStyle(entt::registry&, entt::entity);
        void on_destroy_PolygonStyle(entt::registry&, entt::entity);
        void on_construct_Overlay(entt::registry&, entt::entity);
        void on_update_Overlay(entt::registry&, entt::entity);
        void on_destroy_Overlay(entt::registry&, entt::entity);
    };
}

EVSG_type_name(rocky::PolygonSystemNode)
