/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/ecs/Script.h>
#include <rocky/vsg/ecs/System.h>

namespace ROCKY_NAMESPACE
{
    namespace detail
    {
        struct ScriptDetail
        {
            enum class State {
                Creating,
                Updating,
                Destroying
            };
            State state = State::Creating;
        };
    };

    /**
    * ECS System to process Script components.
    */
    class ROCKY_EXPORT ScriptSystem : public System
    {
    public:
        ScriptSystem(Registry& registry);

        ~ScriptSystem();

        static std::shared_ptr<ScriptSystem> create(Registry& registry) {
            return std::make_shared<ScriptSystem>(registry);
        }

        void update(VSGContext vsgcontext) override;

    private:
        void on_construct_Script(entt::registry& registry, entt::entity entity);
        void on_destroy_Script(entt::registry& registry, entt::entity entity);

        vsg::time_point _lastTime = vsg::time_point::min();
        std::vector<Script*> _toCreate;
        std::vector<Script*> _toUpdate;
        std::vector<std::pair<std::shared_ptr<Script::Runner>, entt::entity>> _toDestroy;
    };
}
