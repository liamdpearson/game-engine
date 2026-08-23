#pragma once

#include "../graphics/graphics.h"

StaticMesh makeStaticObj(const Transform& transform, const char* objSrc,
                         const char* texSrc, bool pixelated, bool collides);

Mesh makeObj(const Transform& transform, const char* objSrc,
             const char* texSrc, bool pixelated);