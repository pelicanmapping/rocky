/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/Label.h>
#include <rocky/vsg/ECS.h>
#include <vsg/text/Text.h>

namespace ROCKY_NAMESPACE
{
    struct ROCKY_EXPORT LabelRenderable : public ECS::NodeComponent
    {
        vsg::ref_ptr<vsg::Text> textNode;
        vsg::ref_ptr<vsg::stringValue> valueBuffer;
        vsg::ref_ptr<vsg::StandardLayout> layout;
        vsg::ref_ptr<vsg::Options> options;
    };

    /**
     * Creates commands for rendering icon primitives.
     */
    class ROCKY_EXPORT LabelSystemNode :
        public vsg::Inherit<ECS::SystemNode<Label, LabelRenderable>, LabelSystemNode>
    {
    public:
        //! Construct the mesh renderer
        LabelSystemNode(entt::registry& registry);

        virtual ~LabelSystemNode();

        enum Features
        {
            NONE = 0x0,
            NUM_PIPELINES = 0
        };

        //! One time setup of the system
        void initializeSystem(Runtime&) override;

        void on_construct(entt::registry& registry, entt::entity);

    private:
        bool update(entt::entity, Runtime& runtime) override;
    };
}
