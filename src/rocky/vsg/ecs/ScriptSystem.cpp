
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
                    EntityWrapper entityWrapper(entity, reg);
                    script.env["self"] = entityWrapper;
                    if (script.onUpdate.valid())
                    {
                        sol::protected_function_result result = script.onUpdate();
                        if (!result.valid())
                        {
                            sol::error err = result;
                            Log()->error("Failed to execute script update: {}", err.what());
                        }
                    }
                    //std::cout << "ScriptSystem: executing script for entity " << int(entity) << "\n";
                    //script.onUpdate();
                    //script.execute(reg, entity);
                });
        });
}
