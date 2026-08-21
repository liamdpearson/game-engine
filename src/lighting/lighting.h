#pragma once

#include "../graphics/graphics.h"

#include <glm/glm.hpp>

#include <vector>


// scene light used only in lighting bake
struct Light
{
    glm::vec3 pos;
    glm::vec3 color;
    float intensity;
    float radius;
};

void bakeSceneLighting(const std::vector<Light>& lights,
                       std::vector<Object*>& rootObjs, float ambient);