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

LabelComponent::LabelComponent(entt::entity id, entt::registry& registry) :
    id(id),
    registry(registry)
{
}

bool LabelComponent::valid() const
{
    return registry.all_of<Label>(id);
}

std::string LabelComponent::getText() const
{
    auto* label = registry.try_get<Label>(id);
    return label ? label->text : std::string();
}

void LabelComponent::setText(const std::string& text)
{
    auto* label = registry.try_get<Label>(id);
    if (label)
    {
        label->text = text;
        Label::dirty(registry, id);
    }
}

TransformComponent::TransformComponent(entt::entity id, entt::registry& registry) :
    id(id),
    registry(registry)
{
}

bool TransformComponent::valid() const
{
    return registry.all_of<Transform>(id);
}

double TransformComponent::getLongitude() const
{
    auto* transform = registry.try_get<Transform>(id);
    return transform ? transform->position.x : 0.0;
}

void TransformComponent::setLongitude(double value)
{
    auto* transform = registry.try_get<Transform>(id);
    if (transform)
    {
        transform->position.x = value;
        transform->dirty(registry);
    }
}

double TransformComponent::getLatitude() const
{
    auto* transform = registry.try_get<Transform>(id);
    return transform ? transform->position.y : 0.0;
}

void TransformComponent::setLatitude(double value)
{
    auto* transform = registry.try_get<Transform>(id);
    if (transform)
    {
        transform->position.y = value;
        transform->dirty(registry);
    }
}

double TransformComponent::getAltitude() const
{
    auto* transform = registry.try_get<Transform>(id);
    return transform ? transform->position.z : 0.0;
}

void TransformComponent::setAltitude(double value)
{
    auto* transform = registry.try_get<Transform>(id);
    if (transform)
    {
        transform->position.z = value;
        transform->dirty(registry);
    }
}

void TransformComponent::setPosition(double longitude, double latitude, double altitude)
{
    auto* transform = registry.try_get<Transform>(id);
    if (transform)
    {
        auto srs = transform->position.srs.valid() ? transform->position.srs : SRS::WGS84;
        transform->position = GeoPoint(srs, longitude, latitude, altitude);
        transform->dirty(registry);
    }
}

void TransformComponent::translate(double longitude_delta, double latitude_delta, double altitude_delta)
{
    auto* transform = registry.try_get<Transform>(id);
    if (transform)
    {
        transform->position.x += longitude_delta;
        transform->position.y += latitude_delta;
        transform->position.z += altitude_delta;
        transform->dirty(registry);
    }
}

LabelStyleComponent::LabelStyleComponent(entt::entity id, entt::registry& registry) :
    id(id),
    registry(registry)
{
}

bool LabelStyleComponent::valid() const
{
    return registry.all_of<LabelStyle>(id);
}

float LabelStyleComponent::getTextSize() const
{
    auto* style = registry.try_get<LabelStyle>(id);
    return style ? style->textSize : 0.0f;
}

void LabelStyleComponent::setTextSize(float value)
{
    auto* style = registry.try_get<LabelStyle>(id);
    if (style)
    {
        style->textSize = value;
        style->dirty(registry);
    }
}

void LabelStyleComponent::setTextColor(float r, float g, float b, float a)
{
    auto* style = registry.try_get<LabelStyle>(id);
    if (style)
    {
        style->textColor = Color(r, g, b, a);
        style->dirty(registry);
    }
}

void LabelStyleComponent::setBackgroundColor(float r, float g, float b, float a)
{
    auto* style = registry.try_get<LabelStyle>(id);
    if (style)
    {
        style->backgroundColor = Color(r, g, b, a);
        style->dirty(registry);
    }
}

void LabelStyleComponent::setBorderColor(float r, float g, float b, float a)
{
    auto* style = registry.try_get<LabelStyle>(id);
    if (style)
    {
        style->borderColor = Color(r, g, b, a);
        style->dirty(registry);
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
    lua.new_usertype<LabelComponent>("LabelComponent",
        "valid", &LabelComponent::valid,
        "text", sol::property(&LabelComponent::getText, &LabelComponent::setText),
        "getText", &LabelComponent::getText,
        "setText", &LabelComponent::setText
    );

    lua.new_usertype<TransformComponent>("TransformComponent",
        "valid", &TransformComponent::valid,
        "longitude", sol::property(&TransformComponent::getLongitude, &TransformComponent::setLongitude),
        "latitude", sol::property(&TransformComponent::getLatitude, &TransformComponent::setLatitude),
        "altitude", sol::property(&TransformComponent::getAltitude, &TransformComponent::setAltitude),
        "setPosition", &TransformComponent::setPosition,
        "translate", sol::overload(
            [](TransformComponent& self, double longitude_delta, double latitude_delta)
            {
                self.translate(longitude_delta, latitude_delta);
            },
            &TransformComponent::translate)
    );

    lua.new_usertype<LabelStyleComponent>("LabelStyleComponent",
        "valid", &LabelStyleComponent::valid,
        "textSize", sol::property(&LabelStyleComponent::getTextSize, &LabelStyleComponent::setTextSize),
        "setTextColor", sol::overload(
            [](LabelStyleComponent& self, float r, float g, float b)
            {
                self.setTextColor(r, g, b);
            },
            &LabelStyleComponent::setTextColor),
        "setBackgroundColor", sol::overload(
            [](LabelStyleComponent& self, float r, float g, float b)
            {
                self.setBackgroundColor(r, g, b);
            },
            &LabelStyleComponent::setBackgroundColor),
        "setBorderColor", sol::overload(
            [](LabelStyleComponent& self, float r, float g, float b)
            {
                self.setBorderColor(r, g, b);
            },
            &LabelStyleComponent::setBorderColor)
    );

    lua.new_usertype<EntityWrapper>("EntityWrapper",
        "id", &EntityWrapper::id,
        "getComponent", &EntityWrapper::getComponent
    );

    registerComponentBinding("Label", [this](entt::entity entity, entt::registry& registry)
        {
            if (registry.all_of<Label>(entity))
            {
                return sol::make_object(lua, LabelComponent(entity, registry));
            }
            return sol::make_object(lua, sol::nil);
        });

    registerComponentBinding("LabelStyle", [this](entt::entity entity, entt::registry& registry)
        {
            if (registry.all_of<LabelStyle>(entity))
            {
                return sol::make_object(lua, LabelStyleComponent(entity, registry));
            }

            auto* label = registry.try_get<Label>(entity);
            if (label && label->style != entt::null && registry.all_of<LabelStyle>(label->style))
            {
                return sol::make_object(lua, LabelStyleComponent(label->style, registry));
            }

            return sol::make_object(lua, sol::nil);
        });

    registerComponentBinding("Transform", [this](entt::entity entity, entt::registry& registry)
        {
            if (registry.all_of<Transform>(entity))
            {
                return sol::make_object(lua, TransformComponent(entity, registry));
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
