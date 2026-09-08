#pragma once

#include "scripts.h"
#include <lua/lua.hpp>
#include <sol/sol.hpp>

struct ScriptInstance
{
    std::string path;
    Object* obj;

    sol::environment env;
    sol::protected_function startFn;
    sol::protected_function updateFn;
    void Start() const;
    void Update(float deltaTime) const;
};

extern sol::state lua;