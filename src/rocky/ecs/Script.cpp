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

bool ScriptProgram::instantiate(sol::environment& env, sol::protected_function& onStart, sol::protected_function& onUpdate) const
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

    if (program)
    {
        program->instantiate(env, onStart, onUpdate);
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
    lua.new_usertype<EntityWrapper>("EntityWrapper",
        "id", &EntityWrapper::id,
        "setText", &EntityWrapper::setText
    );
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

