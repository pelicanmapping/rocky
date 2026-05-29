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

        template<class T, typename... Args>
        static Script create(Args&&... args) {
            return Script(std::make_shared<T>(std::forward<Args>(args)...));
        }

        void OnCreate(entt::registry& registry, entt::entity entity) {
            auto safe_runner = runner;
            if (safe_runner) safe_runner->OnCreate(registry, entity);
        }

        void OnDestroy(entt::registry& registry, entt::entity entity) {
            auto safe_runner = runner;
            if (safe_runner) safe_runner->OnDestroy(registry, entity);
        }

        void OnUpdate(entt::registry& registry, entt::entity entity, float deltaTime) {
            auto safe_runner = runner;
            if (safe_runner) safe_runner->OnUpdate(registry, entity, deltaTime);
        }

        std::shared_ptr<Runner> runner;
    };
}
