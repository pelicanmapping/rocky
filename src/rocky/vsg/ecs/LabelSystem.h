/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/ecs/Label.h>
#include <rocky/vsg/ecs/ECSNode.h>
#include <vsg/text/Font.h>
#include <map>

namespace ROCKY_NAMESPACE
{
    /**
     * Creates commands for rendering icon primitives.
     */
    class ROCKY_EXPORT LabelSystemNode : public vsg::Inherit<detail::SystemNode<Label>, LabelSystemNode>
    {
    public:
        //! Construct the mesh renderer
        LabelSystemNode(Registry& registry);

        enum Features
        {
            NONE = 0x0,
            NUM_PIPELINES = 0
        };

        //! One time setup of the system
        void initialize(VSGContext&) override;

        void createOrUpdateNode(Label&, detail::BuildInfo&, VSGContext&) const;


    protected:
        using FontCache = std::map<std::string, vsg::ref_ptr<vsg::Font>>;
        mutable FontCache _fontCache;
    };
}
