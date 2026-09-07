#pragma once

#include <GLFW/glfw3.h>

#include <vector>


void mouseMoveCallback(GLFWwindow*, double xpos, double ypos);

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

bool keyPressed(int key);

bool keyHeld(int key);

bool keyReleased(int key);

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);

bool mouseButtonPressed(int button);

bool mouseButtonHeld(int key);

bool mouseButtonReleased(int button);

float mouseDX();

float mouseDY();

void endFrameInput();