/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/ecs/AudioSource.h>
#include <rocky/vsg/ecs/System.h>
#include <memory>

namespace ROCKY_NAMESPACE
{
    /**
    * ECS system that plays AudioSource components with miniaudio.
    */
    class ROCKY_EXPORT AudioSystem : public vsg::Inherit<vsg::Node, AudioSystem>, public System
    {
    public:
        //! Construct the system.
        AudioSystem(Registry& registry);

        //! Destruct the system and shut down miniaudio.
        ~AudioSystem() override;

        //! One time setup of the audio engine.
        void initialize(VSGContext vsgcontext) override;

        //! Process source changes and playback commands.
        void update(VSGContext vsgcontext) override;

        //! Update listener and source positions for the current view.
        void traverse(vsg::RecordTraversal& record) const override;

    protected:
        void on_construct_AudioSource(entt::registry& registry, entt::entity entity);
        void on_update_AudioSource(entt::registry& registry, entt::entity entity);
        void on_destroy_AudioSource(entt::registry& registry, entt::entity entity);

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;
    };
}
