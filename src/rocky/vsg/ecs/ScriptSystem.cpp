/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "ScriptSystem.h"

using namespace ROCKY_NAMESPACE;

ScriptSystem::ScriptSystem(Registry& registry) :
    System(registry)
{
    registry.write([&](entt::registry& r)
        {
            r.on_construct<Script>().connect<&ScriptSystem::on_construct_Script>(*this);
            r.on_destroy<Script>().connect<&ScriptSystem::on_destroy_Script>(*this);
        });
}

ScriptSystem::~ScriptSystem()
{
    _registry.write([&](entt::registry& r)
        {
            r.on_construct<Script>().disconnect<&ScriptSystem::on_construct_Script>(*this);
            r.on_destroy<Script>().disconnect<&ScriptSystem::on_destroy_Script>(*this);
        });
}

void
ScriptSystem::on_construct_Script(entt::registry& r, entt::entity e)
{
    auto& script = r.get<Script>(e);
    script.owner = e;
    auto safe_runner = r.get<Script>(e).runner;
    if (safe_runner) safe_runner->OnCreate(r, e);
}

void
ScriptSystem::on_destroy_Script(entt::registry& r, entt::entity e)
{
    auto safe_runner = r.get<Script>(e).runner;
    if (safe_runner) safe_runner->OnDestroy(r, e);
}

void
ScriptSystem::update(VSGContext vsgcontext)
{
    if (status.failed()) return;

    auto frameStamp = vsgcontext->viewer()->getFrameStamp();
    if (!frameStamp) return;

    auto time = frameStamp->time;
    float deltaTime = 0.0f;

    if (_lastTime != vsg::time_point::min())
    {
        deltaTime = std::chrono::duration_cast<std::chrono::duration<float>>(time - _lastTime).count();
    }

    bool hasScripts = false;

    _registry.write([&](entt::registry& r)
        {            
            r.view<Script>().each([&](auto entity, auto& script)
                {
                    if (r.valid(entity))
                    {
                        hasScripts = true;
                        auto safe_runner = script.runner;
                        if (safe_runner) safe_runner->OnUpdate(r, entity, deltaTime);
                    }
                });
        });

    if (hasScripts)
    {
        vsgcontext->requestFrame();
    }

    _lastTime = time;
}
