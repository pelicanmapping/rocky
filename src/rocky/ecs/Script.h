/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>
#include <rocky/ecs/Component.h>
#include <rocky/ecs/Label.h>
#include <rocky/ecs/Transform.h>
#include <sol/sol.hpp>

namespace ROCKY_NAMESPACE
{
    /**
    * Shared compiled Lua script code.
    */
    class ROCKY_EXPORT ScriptProgram
    {
    public:
        ScriptProgram(const std::string& code);

        bool valid() const;

        bool instantiate(
            sol::environment& env,
            sol::protected_function& onStart,
            sol::protected_function& onUpdate,
            sol::protected_function& onDestroy) const;

        std::string code;

    private:
        sol::bytecode bytecode;
        bool _valid = false;
    };

    /**
    * Script ECS component.
    */
    struct ROCKY_EXPORT Script : public Component<Script>
    {
        Script(const std::string& code);

        Script(std::shared_ptr<ScriptProgram> program);
        
        void setCode(const std::string& code);

        void setProgram(std::shared_ptr<ScriptProgram> program);

        std::string code;
        std::shared_ptr<ScriptProgram> program;
        sol::environment env;
        sol::protected_function onStart;
        sol::protected_function onUpdate;
        sol::protected_function onDestroy;
        bool started = false;
    };

    class ROCKY_EXPORT LabelComponent
    {
    public:
        LabelComponent(entt::entity id, entt::registry& registry);

        bool valid() const;
        std::string getText() const;
        void setText(const std::string& text);

        entt::entity id;
        entt::registry& registry;
    };

    class ROCKY_EXPORT TransformComponent
    {
    public:
        TransformComponent(entt::entity id, entt::registry& registry);

        bool valid() const;

        double getLongitude() const;
        void setLongitude(double value);

        double getLatitude() const;
        void setLatitude(double value);

        double getAltitude() const;
        void setAltitude(double value);

        void setPosition(double longitude, double latitude, double altitude);
        void translate(double longitude_delta, double latitude_delta, double altitude_delta = 0.0);

        entt::entity id;
        entt::registry& registry;
    };

    class ROCKY_EXPORT LabelStyleComponent
    {
    public:
        LabelStyleComponent(entt::entity id, entt::registry& registry);

        bool valid() const;

        float getTextSize() const;
        void setTextSize(float value);

        void setTextColor(float r, float g, float b, float a = 1.0f);
        void setBackgroundColor(float r, float g, float b, float a = 1.0f);
        void setBorderColor(float r, float g, float b, float a = 1.0f);

        entt::entity id;
        entt::registry& registry;
    };

    class EntityWrapper
    {
    public:

        EntityWrapper(entt::entity id, entt::registry& registry);

        sol::object getComponent(const std::string& name);

        entt::entity id;
        entt::registry& registry;
    };


    /**
    * ScriptSystem.  
    */
    class ScriptState
    {
    public:
        static ScriptState& Instance();

        sol::state lua;

        std::shared_ptr<ScriptProgram> getProgram(const std::string& code);

        void registerComponentBinding(const std::string& name, std::function<sol::object(entt::entity, entt::registry&)> binding);

        sol::object getComponent(const std::string& name, entt::entity entity, entt::registry& registry);

    private:
        ScriptState();

        ~ScriptState() = default;

        ScriptState(const ScriptState&) = delete;
        ScriptState& operator=(const ScriptState&) = delete;
        ScriptState(ScriptState&&) = delete;
        ScriptState& operator=(ScriptState&&) = delete;

        void Initialize();

        void RegisterBindings();

        std::unordered_map<std::string, std::weak_ptr<ScriptProgram>> _programs;
        std::unordered_map<std::string, std::function<sol::object(entt::entity, entt::registry&)>> _componentBindings;
       
    };
}
