/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <rocky/ecs/Component.h>
#include <rocky/ecs/Registry.h>

namespace ROCKY_NAMESPACE
{
    /**
    * Script component.
    * A Script contains a pointer to a Runner that will execute each frame.
    */
    class Script : public Component<Script>
    {
    public:
        //! Sublass this to define behaviors on creation, update (frame) and destruction.
        class Runner
        {
        public:
            virtual ~Runner() = default;

            virtual std::shared_ptr<Runner> clone() const = 0;
            virtual void onCreate(Registry& registry, entt::entity entity) { }
            virtual void onUpdate(Registry& registry, entt::entity entity, float deltaTime) { }
            virtual void onDestroy(Registry& registry, entt::entity entity) {}
        };

        Script() = default;

        Script(std::shared_ptr<Runner> in_runner) :
            runner(std::move(in_runner)) { }

    private:
        std::shared_ptr<Runner> runner;
        friend class ScriptSystem;
    };
}
