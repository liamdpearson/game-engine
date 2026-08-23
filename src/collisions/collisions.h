#pragma once

#include "../graphics/graphics.h"

void resolvePlayerCollision(Player& player, const std::vector<TriAABB>& colliders);

void collectSceneColliders();

