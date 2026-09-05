#pragma once

#include "../graphics/graphics.h"
#include <lua/lua.hpp>
#include <sol/sol.hpp>
#include <vector>

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

extern std::vector<ScriptInstance> scripts;

void attachScript(Object* obj, std::string path);

void initScripting();