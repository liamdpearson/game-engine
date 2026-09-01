#pragma once

#include "../graphics/graphics.h"


StaticMesh makeStaticMesh(const Transform& transform, const char* objSrc,
                          const char* texSrc, bool pixelated, bool collides);

Mesh makeMesh(const Transform& transform, const char* objSrc,
              const char* texSrc, bool pixelated);

AnimatedMesh makeAnimatedMesh(const Transform& transform, const char* objPath,
                              const char* texPath, bool pixelated);

AnimatedObj makeAnimatedObj(const Transform& transform, const char* objPath);