#pragma once

#include "graphics/graphics.h"


void loadScene(const char* path);

// wipes scene data and calls load scene again
// wipes:
//  colliders
//  rootObjs
//  currentCam
//  lights
//  ambient
//  occluders
//  light grid
//  lua
//  scripts
//  uiroots
//  fonts
void swapScene(const char* path);