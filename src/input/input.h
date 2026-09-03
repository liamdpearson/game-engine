#pragma once

#include <GLFW/glfw3.h>

#include <vector>

extern std::vector<int> keys_pressed;
extern std::vector<int> mouse_buttons_pressed;

extern std::vector<int> keys_held;
extern std::vector<int> mouse_buttons_held;

extern std::vector<int> keys_released;
extern std::vector<int> mouse_buttons_released;

extern float lastX, lastY;
extern float xPos, yPos;
extern float xoff, yoff;
extern bool firstMouse;

void mouseMoveCallback(GLFWwindow*, double xpos, double ypos);

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

bool keyPressed(int key);

bool keyHeld(int key);

bool keyReleased(int key);

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);

bool mouseButtonPressed(int button);

bool mouseButtonHeld(int key);

bool mouseButtonReleased(int button);