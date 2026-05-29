/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "ScriptSystem.h"

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::detail;

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

    auto& det = r.emplace<detail::ScriptDetail>(e);
}

void
ScriptSystem::on_destroy_Script(entt::registry& r, entt::entity e)
{
    auto& script = r.get<Script>(e);
    auto runner = script.runner;
    if (runner)
        _toDestroy.emplace_back(runner, e);
    r.remove<detail::ScriptDetail>(e);
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

    _registry.read([&](entt::registry& r)
        {
            r.view<Script, ScriptDetail>().each([&](auto& script, auto& det)
                {
                    if (script.runner)
                    {
                        if (!det.onCreateInvoked)
                        {
                            _toCreate.emplace_back(&script);
                            det.onCreateInvoked = true;
                        }

                        _toUpdate.emplace_back(&script);
                    }
                });
        });

    for (auto* script : _toCreate)
    {
        script->runner->onCreate(_registry, script->owner);
    }

    for(auto* script : _toUpdate)
    {
        script->runner->onUpdate(_registry, script->owner, deltaTime);
    }

    for (auto entry : _toDestroy)
    {
        entry.first->onDestroy(_registry, entry.second);
    }

    if (!_toUpdate.empty())
    {
        vsgcontext->requestFrame();
    }

    _toCreate.clear();
    _toUpdate.clear();
    _toDestroy.clear();

    _lastTime = time;
}
