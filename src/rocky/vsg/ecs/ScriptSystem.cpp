
/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#include "ScriptSystem.h"

using namespace ROCKY_NAMESPACE;


ScriptSystem::ScriptSystem(Registry& registry) :
    Inherit(registry)
{
    registry.write([&](entt::registry& reg)
        {
            reg.on_construct<Script>().connect<&ScriptSystem::on_construct_Script>(*this);
            reg.on_destroy<Script>().connect<&ScriptSystem::on_destroy_Script>(*this);
        });
}

void
ScriptSystem::initialize(VSGContext vsg)
{
    //nop
}

void
ScriptSystem::update(VSGContext vsg)
{
    _registry.read([&](entt::registry& reg)
        {
            reg.view<Script>().each([&](auto entity, auto& script)
                {
                    startScript(reg, entity, script);
                    callScriptFunction(reg, entity, script, script.onUpdate, "onUpdate");
                });
        });
}

void
ScriptSystem::on_construct_Script(entt::registry& registry, entt::entity entity)
{
    auto& script = registry.get<Script>(entity);
    startScript(registry, entity, script);
}

void
ScriptSystem::on_destroy_Script(entt::registry& registry, entt::entity entity)
{
    auto& script = registry.get<Script>(entity);
    callScriptFunction(registry, entity, script, script.onDestroy, "onDestroy");
}

void
ScriptSystem::startScript(entt::registry& registry, entt::entity entity, Script& script)
{
    if (!script.started)
    {
        script.started = true;
        callScriptFunction(registry, entity, script, script.onStart, "onStart");
    }
}

void
ScriptSystem::callScriptFunction(
    entt::registry& registry,
    entt::entity entity,
    Script& script,
    sol::protected_function& function,
    const char* functionName)
{
    if (function.valid())
    {
        EntityWrapper entityWrapper(entity, registry);
        script.env["self"] = entityWrapper;

        sol::protected_function_result result = function();
        if (!result.valid())
        {
            sol::error err = result;
            Log()->error("Failed to execute script {}: {}", functionName, err.what());
        }
    }
}
