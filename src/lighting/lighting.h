#pragma once

#include "../graphics/graphics.h"

#include <glm/glm.hpp>

#include <vector>


struct Light
{
    glm::vec3 pos;
    glm::vec3 color;
    float intensity;
    float radius;
};

bool bakeSceneLighting(const std::vector<Light>& lights,
                       std::vector<Object*>& rootObjs);