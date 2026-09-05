#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>                  // vec3, mat4, basic types
#include <glm/gtc/matrix_transform.hpp> // perspective, lookAt, rotate, radians
#include <glm/gtc/type_ptr.hpp>         // value_ptr (hand a matrix to OpenGL)
#include <glm/gtc/quaternion.hpp>       // quat slerp, mat4_cast

#include <xatlas/xatlas.h>

#include <vector>
#include <string>
#include <iostream>
#include <memory>


const int VERTEX_FLOATS = 15;
// maximum bones because I store bone indices as a single
// unsigned int so each bone las to be less than 8 bits.
const int MAX_BONES = 128;

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

// scene light used only in lighting bake
struct Light
{
    std::string name;
    std::string tag;
    glm::vec3 pos;
    glm::vec3 color;
    float intensity;
    float radius;
    float falloff;
};

struct LightGrid
{
    std::vector<std::pair<glm::vec3, glm::vec3>> values;
    glm::vec3 min{INFINITY}, max{-INFINITY};
};

// all vectors are of the same length bone owns each at its index
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

struct Rig
{
    Skeleton skeleton;

    std::vector<glm::mat4> palette;
    std::vector<glm::mat4> boneWorlds;
    std::vector<Animation> animations; // all clips baked from fbx
    int currentAnim = -1;  // current animation index
    float animTime = 0.0f; // seconds into current clip
    int nextAnim = -1;     // animation set to play after current one done
    float animSpeed = 1.0f;

    // lastPose is updated every frame (what player last saw)
    // blendFrom is empty unless currently fading between animations
    std::vector<BonePose> lastPose;
    std::vector<BonePose> blendFrom;
    float blendDuration = 0.0f;
    float blendElapsed = 0.0f;

    void setAnim(int index, float blendTime = 0.0f, int nextAnim = -1);
    void setAnim(const std::string& name, float blendTime = 0.0f, int nextAnim = -1);

    int findBoneIndex(std::string name) {
        for (int b = 0; b < (int)this->skeleton.names.size(); ++b) {
            if (this->skeleton.names[b] == name) return b;
        }
        std::cout << "Couldn't find bone: " << name << '\n';
        return -1;
    }
};

// A node in the scene hierarchy can be an empty node, a camera, a mesh or a static mesh.
class Object
{
    private:
        glm::mat4 world = glm::mat4(1.0f);
        int boneIndex = -1; // -1 unless object is a child of a bone in an animated mesh
        std::string name;
        std::string tag;

    public:
        bool draw = true;
        Transform transform;
        std::vector<std::unique_ptr<Object>> children;
        Object* parent = nullptr; // nullptr if root, points too parent object
        
        Object() = default;
        Object(const Transform& t)
            : transform(t) {}

        virtual void Upload();
        virtual void Compose();
        void ComposeSelf();
        virtual void ComputePose();
        virtual Rig* GetRig() { return nullptr; }
        virtual void Draw();
        virtual void CollectOccluders(const glm::mat4 parentWorld, std::vector<Tri>& out);
        virtual void CollectColliders(const glm::mat4 parentWorld, std::vector<TriAABB>& out);
        virtual void BakeLighting(const glm::mat4 parentWorld);

        void setWorld(const glm::mat4& world) { this->world = world; }
        glm::mat4 getWorld() const  { return this->world; }

        void setBoneIndex(int bi) { this->boneIndex = bi; }
        int getBoneIndex() const { return this->boneIndex; }

        void setName(std::string n) { this->name = n; }
        std::string getName() const { return this->name; }

        void setTag(std::string t) { this->tag = t; }
        std::string getTag() const { return this->tag; }

        virtual void addChild(std::unique_ptr<Object> child, int bi = -1) {
            if (bi != -1)
                std::cout << "Added bone index when adding child to non animated mesh." << '\n';
            child->parent = this;
            this->children.push_back(std::move(child));
        }

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
        
        void setAtlas(xatlas::Atlas* at) { this->atlas = at; }
        xatlas::Atlas* getAtlas() const { return this->atlas; }

        void setCollides(bool c) { this->collides = c; }
        bool getCollides() const { return this->collides; }
    
        ~StaticMesh() override;
};

class AnimatedMesh : public Mesh
{
    public:
        Rig rig;

        AnimatedMesh() = default;

        void Draw() override;
        void ComputePose() override;
        Rig* GetRig() override { return &rig; }
        void addChild(std::unique_ptr<Object> child, int bi = -1) override {
            if (bi > -1)
                child->setBoneIndex(bi);
            child->parent = this;
            this->children.push_back(std::move(child));
        }

        ~AnimatedMesh() = default;
};

class AnimatedObj : public Object
{
    public:
        Rig rig;

        AnimatedObj() = default;

        void ComputePose() override;
        Rig* GetRig() override { return &rig; }
        void addChild(std::unique_ptr<Object> child, int bi = -1) override {
            if (bi > -1)
                child->setBoneIndex(bi);
            child->parent = this;
            this->children.push_back(std::move(child));
        }

        ~AnimatedObj() = default;
};

// camera node containing pos, front, and up needed for configureCamera.
// pos, front, and up are set in Camera::Compose in graphics.cpp
class Camera : public Object
{
    private:
        glm::vec3 pos{0.0f, 0.0f, 0.0f};
        glm::vec3 front{0.0f, 0.0f, -1.0f};
        glm::vec3 up{0.0f, 1.0f, 0.0f};

    public:
        float FOV;

        Camera(const Transform& t, const float FOV)
        : FOV(FOV) { transform = t; }

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

class Capsule : public Object
{
     public:
        float height = 1.8f; // capsule height
        float radius = 0.3f; // capsule width
        glm::vec3 velocity{0.0f};
        bool grounded = false;

        Capsule() = default;
        void Compose() override;

        Capsule(const Transform t,
               const float height, const float radius)
        : height(height), radius(radius) {
            transform = t;
        }

        ~Capsule() = default;
};

// define scene variables
extern std::vector<std::unique_ptr<Object>> rootObjs;

// lighting stuff
extern std::vector<Light> lights;
extern float ambient;
extern std::vector<Tri> occluders;
extern LightGrid lightGrid;

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

extern int modelLoc, projectionLoc, viewLoc, normalMatLoc, boneMatricesLoc;
extern int texLoc, lightMapLoc, lightModeLoc, lightDirLoc, objectLightLoc;

extern const char* vertexShaderSource;
extern const char* fragmentShaderSource;

void framebuffer_size_callback(GLFWwindow* window, int width, int height);

int compileShader(GLenum type, const char* src);

unsigned int linkShaderProgram(const char* vsSrc, const char* fsSrc);

void buildShaderProgram();

std::pair<int, int> textureDimensions(const char* path);

unsigned int loadTexture(const char* src, bool pixelated, bool clampEdge = false);

unsigned int loadLightMap(std::vector<glm::vec3>& pixels, int width, int height);

void configureCamera(Camera*& cam);

Object* findObject(const std::string& name,
                   const std::vector<std::unique_ptr<Object>>& objs);