/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <rocky/ecs/Component.h>

namespace ROCKY_NAMESPACE
{
    /**
    * Script component.
    * A Script contains a pointer to a Runner that will execute each frame.
    */
    class Script : public Component<Script>
    {
    public:
        class Runner
        {
        public:
            virtual ~Runner() = default;

            virtual std::shared_ptr<Runner> clone() const = 0;
            virtual void OnCreate(entt::registry& registry, entt::entity entity) { }
            virtual void OnDestroy(entt::registry& registry, entt::entity entity) { }
            virtual void OnUpdate(entt::registry& registry, entt::entity entity, float deltaTime) { }
        };

        Script() = default;

        Script(std::shared_ptr<Runner> in_runner) :
            runner(std::move(in_runner)) { }

    private:
        std::shared_ptr<Runner> runner;
        friend class ScriptSystem;
    };
}
