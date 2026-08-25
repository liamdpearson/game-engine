#pragma once

#include "../graphics/graphics.h"

StaticMesh makeStaticObj(const Transform& transform, const char* objSrc,
                         const char* texSrc, bool pixelated, bool collides);

Mesh makeObj(const Transform& transform, const char* objSrc,
             const char* texSrc, bool pixelated);

AnimatedMesh makeAnimatedObj(const Transform& transform, const char* objPath,
                             const char* texPath, bool pixelated);