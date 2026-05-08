/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/ecs/ECSNode.h>

#if defined(ROCKY_HAS_LUA)


namespace ROCKY_NAMESPACE
{
    /**
     * System that executes Script
     */
    class ROCKY_EXPORT ScriptSystem : public rocky::Inherit<System, ScriptSystem>
    {
    public:
        //! Construct the system
        ScriptSystem(Registry& registry);

        //! One time setup of the system
        void initialize(VSGContext) override;

        //! Per frame update
        void update(VSGContext) override;

    private:
        void on_construct_Script(entt::registry& registry, entt::entity entity);
        void on_destroy_Script(entt::registry& registry, entt::entity entity);

        void startScript(entt::registry& registry, entt::entity entity, Script& script);
        void callScriptFunction(
            entt::registry& registry,
            entt::entity entity,
            Script& script,
            sol::protected_function& function,
            const char* functionName);
    };
}

#endif
