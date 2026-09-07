#pragma once

#include "../graphics/graphics.h"


std::unique_ptr<StaticMesh> makeStaticMesh(const Transform& transform, const char* objSrc,
                                           const char* texSrc, bool pixelated, bool collides);

std::unique_ptr<Mesh> makeMesh(const Transform& transform, const char* objSrc,
                               const char* texSrc, bool pixelated);

std::unique_ptr<AnimatedMesh> makeAnimatedMesh(const Transform& transform, const char* objPath,
                                               const char* texPath, bool pixelated);

std::unique_ptr<AnimatedObj> makeAnimatedObj(const Transform& transform, const char* objPath);

void loadScene(const char* path);
