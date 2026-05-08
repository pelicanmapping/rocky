/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <string>
#include <memory>
#include <unordered_map>
#include <rocky/ecs/Component.h>
#include <rocky/ecs/Label.h>
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

        bool instantiate(sol::environment& env, sol::protected_function& onStart, sol::protected_function& onUpdate) const;

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
    };

    class EntityWrapper
    {
    public:

        EntityWrapper(entt::entity id, entt::registry& registry) : id(id), registry(registry)
        {
        }

        void setText(const std::string& text)
        {
            auto* label = registry.try_get<Label>(id);
            if (label)
            {
                label->text = text;
            }
        }

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
       
    };
}
