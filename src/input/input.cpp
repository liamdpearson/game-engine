#include "input.h"

#include <GLFW/glfw3.h>

#include <vector>
#include <algorithm>

std::vector<int> keys_pressed;
std::vector<int> mouse_buttons_pressed;

std::vector<int> keys_held;
std::vector<int> mouse_buttons_held;

std::vector<int> keys_released;
std::vector<int> mouse_buttons_released;

float lastX, lastY;
float xPos, yPos;
float xoff, yoff;
bool firstMouse = true;

void mouseMoveCallback(GLFWwindow*, double xpos, double ypos)
{
    if (firstMouse) {
        lastX = (float)xpos; lastY = (float)ypos;
        xPos = (float)xpos; yPos = (float)ypos;
        firstMouse = false;
    } else {
        lastX = xPos; lastY = yPos;
        xPos = xpos; yPos = ypos;

        xoff += xPos - lastX;
        yoff += lastY - yPos;
    }
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS) {
        keys_pressed.push_back(key);
        keys_held.push_back(key);
    }

    if (action == GLFW_RELEASE) {
        keys_released.push_back(key);
        keys_held.erase(std::remove(
            keys_held.begin(), keys_held.end(), key),
            keys_held.end()
        );
    }
}

bool keyPressed(int key)
{
    return std::find(keys_pressed.begin(), keys_pressed.end(), key) != keys_pressed.end();
}

bool keyHeld(int key)
{
    return std::find(keys_held.begin(), keys_held.end(), key) != keys_held.end();
}

bool keyReleased(int key)
{
    return std::find(keys_released.begin(), keys_released.end(), key) != keys_released.end();
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    if (action == GLFW_PRESS) {
        mouse_buttons_pressed.push_back(button);
        mouse_buttons_held.push_back(button);
    }
    if (action == GLFW_RELEASE) {
        mouse_buttons_released.push_back(button);
        mouse_buttons_held.erase(
            std::remove(mouse_buttons_held.begin(), mouse_buttons_held.end(), button),
            mouse_buttons_held.end()
        );
    }
}

bool mouseButtonPressed(int button)
{
    return std::find(mouse_buttons_pressed.begin(), mouse_buttons_pressed.end(), button) != mouse_buttons_pressed.end();
}

bool mouseButtonHeld(int key)
{
    return std::find(mouse_buttons_held.begin(), mouse_buttons_held.end(), key) != mouse_buttons_held.end();
}

bool mouseButtonReleased(int button)
{
    return std::find(mouse_buttons_released.begin(), mouse_buttons_released.end(), button) != mouse_buttons_released.end();
}