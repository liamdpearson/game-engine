#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>                  // vec3, mat4, basic types
#include <glm/gtc/matrix_transform.hpp> // perspective, lookAt, rotate, radians
#include <glm/gtc/type_ptr.hpp>         // value_ptr (hand a matrix to OpenGL)

#include <iostream>
#include <vector>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <map>

extern GLFWwindow* window;
extern int SW, SH;

extern float deltaTime, lastFrame, currentFrame;

extern unsigned int shaderProgram;

extern int modelLoc, projectionLoc, viewLoc;

extern const char* vertexShaderSource;
extern const char* fragmentShaderSource;

struct Transform
{
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float yaw = 0.0f, pitch = 0.0f, roll = 0.0f;
    float scaleX = 1.0f, scaleY = 1.0f, scaleZ = 1.0f;

    // builds local to world matrix
    glm::mat4 matrix() const
    {
        glm::mat4 m(1.0f);
        m = glm::translate(m, glm::vec3(x, y, z));
        m = glm::rotate(m, glm::radians(yaw), glm::vec3(0, 1, 0));
        m = glm::rotate(m, glm::radians(pitch), glm::vec3(-1, 0, 0));
        m = glm::rotate(m, glm::radians(roll), glm::vec3(0, 0, 1));
        m = glm::scale(m, glm::vec3(scaleX, scaleY, scaleZ));
        return m;
    }

    bool operator==(const Transform& other)
    {
        return (x == other.x && y == other.y && z == other.z
                && yaw == other.yaw && pitch == other.pitch && roll == other.roll
                && scaleX == other.scaleX && scaleY == other.scaleY && scaleZ == other.scaleZ);
    }

    Transform& operator=(const Transform&) = default;
};

class Object
{
    private:
        glm::mat4 world = glm::mat4(1.0f);

    public:
        Transform transform;
        std::vector<Object*> children;

        Object() = default;
        Object(const Transform& t)
            : transform(t) {}

        virtual void Upload();
        virtual void Compose();
        virtual void Draw();

        void setWorld(const glm::mat4& world) { this->world = world; }
        glm::mat4 getWorld() const  { return this->world; }

        virtual ~Object() = default;
};

class Mesh : public Object
{
    private:
        std::vector<float> vertices;
        std::vector<unsigned int> indices;
        unsigned int texture = 0;
        GLsizei indexCount = 0;
        unsigned int VAO = 0, VBO = 0, EBO = 0;

    public:
        Mesh() = default;

        void Upload() override;
        void Draw() override;

        void setVertices(const std::vector<float>& verts) { this->vertices = verts; }
        std::vector<float> getVertices() const { return this->vertices; }

        void setIndices(const std::vector<unsigned int>& idx) { this->indices = idx; }
        std::vector<unsigned int> getIndices() const { return this->indices; }

        void setTexture(const unsigned int& tex) { this->texture = tex; }
        unsigned int getTexture() const { return this->texture; }

        void setIndexCount(const GLsizei& ic) { this->indexCount = ic; }
        GLsizei getIndexCount() const { return this->indexCount; }

        ~Mesh() override;
};

class Camera : public Object
{
    private:
        glm::vec3 pos;
        glm::vec3 front;
        glm::vec3 up;

    public:
        float FOV;

        Camera(const float FOV, const glm::vec3 pos,
               glm::vec3 front, glm::vec3 up)
        : FOV(FOV), pos(pos), front(front), up(up)
        {
            // Compose() recomputes pos/front/up from `transform` every frame,
            // so the transform (not just the fields above) must encode position.
            transform.x = pos.x;
            transform.y = pos.y;
            transform.z = pos.z;
        }

        void Upload() override;
        void Compose() override; // derives pos/front/up from world

        glm::vec3 getPos() const { return this->pos; }
        void setPos(const glm::vec3& p) { this->pos = p; }

        glm::vec3 getFront() const { return this->front; }
        void setFront(const glm::vec3& f) { this->front = f; }

        glm::vec3 getUp() const { return this->up; }
        void setUp(const glm::vec3& u) { this->up = u; }

        ~Camera() = default;
};

void framebuffer_size_callback(GLFWwindow* window, int width, int height);

unsigned int compileShader(GLenum type, const char* src);

void buildShaderProgram();

unsigned int loadTexture(const char* src);

void configureCamera(const Camera& cam);

Mesh makeObj(const Transform& transform, const char* objSrc,
             const char* texSrc);