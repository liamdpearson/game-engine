#pragma once

#include "../graphics/graphics.h"

void resolveCapsuleCollision(Capsule*& capsule, const std::vector<TriAABB>& colliders);

void collectSceneColliders();