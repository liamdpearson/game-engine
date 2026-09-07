#pragma once

#include "../graphics/graphics.h"

#include <glm/glm.hpp>

#include <vector>


glm::vec3 sampleLightAt(const glm::vec3& p);

void bakeSceneLighting();