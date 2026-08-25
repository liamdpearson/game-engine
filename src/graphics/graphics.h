#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>                  // vec3, mat4, basic types
#include <glm/gtc/matrix_transform.hpp> // perspective, lookAt, rotate, radians
#include <glm/gtc/type_ptr.hpp>         // value_ptr (hand a matrix to OpenGL)
#include <glm/gtc/quaternion.hpp>       // quat slerp, mat4_cast

#include <xatlas/xatlas.h>

#include <vector>

const int VERTEX_FLOATS = 15;

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

struct Tri{ glm::vec3 a, b, c; };

struct AABB
{
    glm::vec3 min{0.0f};
    glm::vec3 max{0.0f};

    bool overlaps(const AABB& other) const
    {
        return min.x <= other.max.x && max.x >= other.min.x
            && min.y <= other.max.y && max.y >= other.min.y
            && min.z <= other.max.z && max.z >= other.min.z;
    }

    void expand(float margin)
    {
        min -= glm::vec3(margin);
        max += glm::vec3(margin);
    }
};

struct TriAABB : Tri { AABB aabb; };

// initialized here so compiler is happy when I declare BakeLighting.
// see definition in lighting.h.
struct Light;

// A node in the scene hierarchy can be an empty node, a camera, a mesh or a static mesh.
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
        virtual void CollectOccluders(const glm::mat4 parentWorld, std::vector<Tri>& out);
        virtual void CollectColliders(const glm::mat4 parentWorld, std::vector<TriAABB>& out);
        virtual void BakeLighting(const glm::mat4 parentWorld);

        void setWorld(const glm::mat4& world) { this->world = world; }
        glm::mat4 getWorld() const  { return this->world; }

        virtual ~Object() = default;
};

// dynamic mesh
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

        //void setVAO(unsigned int vao) { this->VAO = vao; }
        unsigned int getVAO() const { return this->VAO; }

        //void setVBO(unsigned int vbo) { this->VBO = vbo; }
        unsigned int getVBO() const { return this->VBO; }

        //void setEBO(unsigned int ebo) { this->EBO = ebo; }
        unsigned int getEBO() const { return this->EBO; }

        ~Mesh() override;
};

// static mesh that has its light map baked onto it in bakeSceneLighting.
class StaticMesh : public Mesh
{
    private:
        unsigned int lightMap = 0;
        xatlas::Atlas* atlas = nullptr;
        bool collides = false;

    public:
        StaticMesh() = default;

        void Draw() override;
        void CollectOccluders(const glm::mat4 parentWorld, std::vector<Tri>& out);
        void CollectColliders(const glm::mat4 parentWorld, std::vector<TriAABB>& out);
        void BakeLighting(const glm::mat4 parentWorld);

        void setLightMap(const unsigned int& lm) { this->lightMap = lm; }
        unsigned int getLightMap() const { return this->lightMap; }
        
        void setAtlas(xatlas::Atlas*& at) { this->atlas = at; }
        xatlas::Atlas* getAtlas() const { return this->atlas; }

        void setCollides(bool c) { this->collides = c; }
        bool getCollides() const { return this->collides; }
    
        ~StaticMesh() override;
};

// all vectors are of the same length bone has one of each
struct Skeleton
{
    std::vector<glm::mat4> inverseBind;
    std::vector<int> parent;
    std::vector<std::string> names;
    std::vector<glm::mat4> parentWorld;
};

struct BoneTrack
{
    std::vector<glm::vec3> pos;
    std::vector<glm::quat> rot;
    std::vector<glm::vec3> scale;
};

struct BonePose
{
    glm::vec3 pos{0.0f};
    glm::quat rot{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
};

struct Animation
{
    std::string name;
    std::vector<BoneTrack> tracks;
    int frameCount = 0;
    float fps = 30.0f;
    float duration = 0.0f; // seconds
};

class AnimatedMesh : public Mesh
{
    private:
        Skeleton skeleton;

    public:
        AnimatedMesh() = default;

        void setSkeleton(const Skeleton& skel) { this->skeleton = skel; }
        Skeleton getSkeleton() const { return this->skeleton; }
};

// camera node containing pos, front, and up needed for configureCamera.
// pos, front, and up are set in Camera::Compose in graphics.cpp
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
        : pos(pos), front(front), up(up), FOV(FOV)
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

class Player : public Object
{
     public:
        float height = 1.8f; // capsule height
        float radius = 0.3f; // capsule width
        glm::vec3 velocity{0.0f};
        bool grounded = false;

        Player() = default;
        void Compose() override;

        Player(const Transform t,
               const float height, const float radius)
        : height(height), radius(radius) {
            transform = t;
        }

        ~Player() = default;
};

// define scene variables
extern std::vector<Object*> rootObjs;

// lighting stuff
extern std::vector<Light*> lights;
extern float ambient;
extern std::vector<Tri> occluders;
extern std::vector<std::pair<glm::vec3, glm::vec3>> lightGrid;

// for finding the bounds box of the scene for light grid
extern float minX;
extern float maxX;
extern float minY;
extern float maxY;
extern float minZ;
extern float maxZ;

// collision stuff
extern std::vector<TriAABB> colliders;

// define opengl variables
extern GLFWwindow* window;
extern int SW, SH;

extern float deltaTime, lastFrame, currentFrame;

extern unsigned int shaderProgram;

extern int modelLoc, projectionLoc, viewLoc, normalMatLoc;
extern int texLoc, lightMapLoc, lightModeLoc, lightDirLoc, objectLightLoc;

extern const char* vertexShaderSource;
extern const char* fragmentShaderSource;

void framebuffer_size_callback(GLFWwindow* window, int width, int height);

void buildShaderProgram();

unsigned int loadTexture(const char* src, bool pixelated, bool clampEdge = false);

unsigned int loadLightMap(std::vector<glm::vec3>& pixels, int width, int height);

void configureCamera(const Camera& cam);