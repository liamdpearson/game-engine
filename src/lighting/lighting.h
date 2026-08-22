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
    float falloff;
};

glm::vec3 sampleLightAt(const glm::vec3& p);

void bakeSceneLighting();