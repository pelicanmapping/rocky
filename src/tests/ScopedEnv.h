/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once

#include <cstdlib>
#include <optional>
#include <string>

//! Sets an environment variable for one scope and restores its previous value (or unsets it).
class ScopedEnv
{
public:
    ScopedEnv(const char* name, const char* value) :
        _name(name)
    {
        if (const char* previous = std::getenv(name))
            _previous = previous;
        set(value);
    }

    ~ScopedEnv()
    {
        set(_previous ? _previous->c_str() : nullptr);
    }

    ScopedEnv(const ScopedEnv&) = delete;
    ScopedEnv& operator=(const ScopedEnv&) = delete;

private:
    void set(const char* value)
    {
#ifdef _WIN32
        _putenv_s(_name.c_str(), value ? value : "");
#else
        if (value)
            setenv(_name.c_str(), value, 1);
        else
            unsetenv(_name.c_str());
#endif
    }

    std::string _name;
    std::optional<std::string> _previous;
};
