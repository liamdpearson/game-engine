#pragma once

#include "../graphics/graphics.h"
#include <vector>

struct ScriptInstance;

extern std::vector<ScriptInstance> scripts;

void attachScript(Object* obj, std::string path);

void resetScripting();

void loadScripts();

void initScripting();

void startScripts();

void updateScripts();