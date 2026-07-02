#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>                  // vec3, mat4, basic types
#include <glm/gtc/matrix_transform.hpp> // perspective, lookAt, rotate, radians
#include <glm/gtc/type_ptr.hpp>       // value_ptr (hand a matrix to OpenGL)

#include <cstdio>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <map>
#include <utility>
#include <algorithm>
#include <cmath>
#include <iostream>

const float PI = 3.1415926;

struct Transform
{
    float x;
    float y;
    float z;
    float yaw;
    float pitch;
    float scale;

    Transform operator+(const Transform& parent)
    {
        glm::mat4 model(1.0f);

        model = glm::translate(model, glm::vec3(parent.x, parent.y, parent.z));
        model = glm::rotate(model, glm::radians(parent.yaw), glm::vec3(0, 1, 0));
        model = glm::rotate(model, glm::radians(parent.pitch), glm::vec3(-1, 0, 0));
        model = glm::scale(model, glm::vec3(parent.scale));

        glm::vec4 worldPos = model * glm::vec4(x, y, z, 1.0f);

        return Transform{
            worldPos.x,
            worldPos.y,
            worldPos.z,
            parent.yaw + yaw,
            parent.pitch + pitch,
            parent.scale * scale
        };
    }
    bool operator==(const Transform& other)
    {
        return (x == other.x && y == other.y && z == other.z
                && yaw == other.yaw && pitch == other.pitch
                && scale == other.scale);
    }
    Transform& operator=(const Transform&) = default;
};


struct Object
{
    Transform transform;
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    unsigned int VAO = 0, VBO = 0, EBO = 0;
    unsigned int texture = 0;
    GLsizei indexCount = 0;


    Transform world_pos = transform;
    std::vector<Object*> children = {};
    bool billboard = false;  // if true, the quad is rotated to face the camera each frame

    Object() = default;

    void Upload();

    void Draw();

    ~Object();
};

extern int SW;
extern int SH;

extern glm::vec3 cameraPos;
extern glm::vec3 cameraFront;
extern glm::vec3 cameraUp;

extern float yaw;
extern float pitch;
extern float lastX, lastY;   // last mouse pos (start at screen center)
extern bool  firstMouse;

extern const float sensitivity;
extern float deltaTime, lastFrame;

extern const char* vertexShaderSrc;
extern const char* fragmentShaderSrc;

extern std::vector<Object*> parents;

extern GLFWwindow* window;

extern unsigned int vs;
extern unsigned int fs;
extern unsigned int shaderProgram;
extern int modelLoc, viewLoc, projectionLoc;

unsigned int compileShader(GLenum type, const char* src);

void framebufferSizeCallback(GLFWwindow*, int width, int height);

unsigned int loadTexture(const char* path);

void mouseCallback(GLFWwindow*, double xpos, double ypos);

bool loadOBJ(const char* path,
                    std::vector<float>& outVerts,
                    std::vector<unsigned int>& outIndices);

Object makeObject(const char* objPath, const char* texPath,
                  Transform Transform);

Object makeSprite(const char* texPath, Transform transform);

glm::mat4 billboardModel(const glm::vec3& pos, float scale, const glm::vec3& camPos);

void uploadObject(Object &obj);

void buildShaderProgram();

int initWindow();

void drawObj(Object& obj);

void clearBG(float r, float g, float b, float a);