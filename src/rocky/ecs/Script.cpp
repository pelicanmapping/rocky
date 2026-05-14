/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#include "Script.h"

#include "Registry.h"

using namespace ROCKY_NAMESPACE;


ScriptProgram::ScriptProgram(const std::string& code) :
    code(code)
{
    sol::load_result loadResult = ScriptState::Instance().lua.load(code);
    if (!loadResult.valid())
    {
        sol::error err = loadResult;
        Log()->error("Failed to load script: {}", err.what());
        return;
    }

    sol::protected_function chunk = loadResult.get<sol::protected_function>();
    bytecode = chunk.dump<sol::bytecode>();
    _valid = true;
}

bool ScriptProgram::valid() const
{
    return _valid;
}

bool ScriptProgram::instantiate(
    sol::environment& env,
    sol::protected_function& onStart,
    sol::protected_function& onUpdate,
    sol::protected_function& onDestroy) const
{
    if (!valid())
    {
        return false;
    }

    sol::load_result loadResult = ScriptState::Instance().lua.load_buffer(
        bytecode.data(),
        bytecode.size(),
        "script",
        sol::load_mode::binary);

    if (!loadResult.valid())
    {
        sol::error err = loadResult;
        Log()->error("Failed to load script bytecode: {}", err.what());
        return false;
    }

    sol::protected_function chunk = loadResult.get<sol::protected_function>();
    sol::set_environment(env, chunk);

    sol::protected_function_result scriptResult = chunk();
    if (!scriptResult.valid())
    {
        sol::error err = scriptResult;
        Log()->error("Failed to execute script: {}", err.what());
        return false;
    }

    onStart = env["onStart"];
    onUpdate = env["onUpdate"];
    onDestroy = env["onDestroy"];
    return true;
}

Script::Script(const std::string& code)
{
    setCode(code);
}

Script::Script(std::shared_ptr<ScriptProgram> program)
{
    setProgram(program);
}

void Script::setCode(const std::string& code)
{
    setProgram(ScriptState::Instance().getProgram(code));
}

void Script::setProgram(std::shared_ptr<ScriptProgram> program)
{
    this->program = program;
    this->code = program ? program->code : std::string();

    // create a new environment for this script
    env = sol::environment(ScriptState::Instance().lua, sol::create, ScriptState::Instance().lua.globals());

    onStart = sol::nil;
    onUpdate = sol::nil;
    onDestroy = sol::nil;
    started = false;

    if (program)
    {
        program->instantiate(env, onStart, onUpdate, onDestroy);
    }
}

EntityWrapper::EntityWrapper(entt::entity id, entt::registry& registry) :
    id(id),
    registry(registry)
{
}

sol::object EntityWrapper::getComponent(const std::string& name)
{
    return ScriptState::Instance().getComponent(name, id, registry);
}

void EntityWrapper::dirty(const std::string& name)
{
    if (name == "Label" && registry.all_of<Label>(id))
    {
        Label::dirty(registry, id);
    }
    else if (name == "LabelStyle")
    {
        if (registry.all_of<LabelStyle>(id))
        {
            LabelStyle::dirty(registry, id);
        }
        else if (auto* label = registry.try_get<Label>(id); label && label->style != entt::null && registry.all_of<LabelStyle>(label->style))
        {
            LabelStyle::dirty(registry, label->style);
        }
    }
    else if (name == "Transform")
    {
        if (auto* transform = registry.try_get<Transform>(id))
        {
            transform->dirty(registry);
        }
    }
}

ScriptState& ScriptState::Instance()
{
    static ScriptState instance;
    return instance;
}

ScriptState::ScriptState()
{
    Initialize();
}


void ScriptState::Initialize()
{
    lua.open_libraries(
        sol::lib::base,
        sol::lib::math,
        sol::lib::table,
        sol::lib::string
    );

    RegisterBindings();
}


void ScriptState::RegisterBindings()
{
    lua.new_usertype<Color>("Color",
        sol::constructors<Color(), Color(float, float, float, float)>(),
        "r", sol::property(
            [](const Color& color) { return color[0]; },
            [](Color& color, float value) { color[0] = value; }),
        "g", sol::property(
            [](const Color& color) { return color[1]; },
            [](Color& color, float value) { color[1] = value; }),
        "b", sol::property(
            [](const Color& color) { return color[2]; },
            [](Color& color, float value) { color[2] = value; }),
        "a", sol::property(
            [](const Color& color) { return color[3]; },
            [](Color& color, float value) { color[3] = value; })
    );

    lua.new_usertype<GeoPoint>("GeoPoint",
        "x", sol::property(
            [](const GeoPoint& point) { return point.x; },
            [](GeoPoint& point, double value) { point.x = value; }),
        "y", sol::property(
            [](const GeoPoint& point) { return point.y; },
            [](GeoPoint& point, double value) { point.y = value; }),
        "z", sol::property(
            [](const GeoPoint& point) { return point.z; },
            [](GeoPoint& point, double value) { point.z = value; })
    );

    lua.new_usertype<Label>("Label",
        "text", &Label::text,
        "style", &Label::style
    );

    lua.new_usertype<LabelStyle>("LabelStyle",
        "fontName", &LabelStyle::fontName,
        "textColor", &LabelStyle::textColor,
        "textSize", &LabelStyle::textSize,
        "outlineSize", &LabelStyle::outlineSize,
        "outlineColor", &LabelStyle::outlineColor,
        "borderSize", &LabelStyle::borderSize,
        "borderColor", &LabelStyle::borderColor,
        "backgroundColor", &LabelStyle::backgroundColor
    );

    lua.new_usertype<Transform>("Transform",
        "position", &Transform::position,
        "radius", &Transform::radius,
        "topocentric", &Transform::topocentric,
        "horizonCulled", &Transform::horizonCulled,
        "frustumCulled", &Transform::frustumCulled,
        "revision", &Transform::revision
    );

    lua.new_usertype<EntityWrapper>("EntityWrapper",
        "id", &EntityWrapper::id,
        "getComponent", &EntityWrapper::getComponent,
        "dirty", &EntityWrapper::dirty
    );

    registerComponentBinding("Label", [this](entt::entity entity, entt::registry& registry)
        {
            if (auto* label = registry.try_get<Label>(entity))
            {
                return sol::make_object(lua, label);
            }
            return sol::make_object(lua, sol::nil);
        });

    registerComponentBinding("LabelStyle", [this](entt::entity entity, entt::registry& registry)
        {
            if (auto* style = registry.try_get<LabelStyle>(entity))
            {
                return sol::make_object(lua, style);
            }

            auto* label = registry.try_get<Label>(entity);
            if (label && label->style != entt::null)
            {
                if (auto* style = registry.try_get<LabelStyle>(label->style))
                {
                    return sol::make_object(lua, style);
                }
            }

            return sol::make_object(lua, sol::nil);
        });

    registerComponentBinding("Transform", [this](entt::entity entity, entt::registry& registry)
        {
            if (auto* transform = registry.try_get<Transform>(entity))
            {
                return sol::make_object(lua, transform);
            }
            return sol::make_object(lua, sol::nil);
        });
}

void ScriptState::registerComponentBinding(const std::string& name, std::function<sol::object(entt::entity, entt::registry&)> binding)
{
    _componentBindings[name] = std::move(binding);
}

sol::object ScriptState::getComponent(const std::string& name, entt::entity entity, entt::registry& registry)
{
    auto i = _componentBindings.find(name);
    if (i != _componentBindings.end())
    {
        return i->second(entity, registry);
    }

    return sol::make_object(lua, sol::nil);
}

std::shared_ptr<ScriptProgram> ScriptState::getProgram(const std::string& code)
{
    auto i = _programs.find(code);
    if (i != _programs.end())
    {
        auto program = i->second.lock();
        if (program)
        {
            return program;
        }
    }

    auto program = std::make_shared<ScriptProgram>(code);
    _programs[code] = program;
    return program;
}
