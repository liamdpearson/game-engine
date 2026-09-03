#pragma once

#include "../graphics/graphics.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <vector>


const int UI_FLOATS = 4;

struct UITransform
{
    float x, y;
    float angle;
    float scaleX, scaleY;

    glm::mat3 matrix() const
    {
        float c = std::cos(glm::radians(angle));
        float s = std::sin(glm::radians(angle));
        return glm::mat3(
             c * scaleX,  s * scaleX,  0.0f,
            -s * scaleY,  c * scaleY,  0.0f,
             x,           y,          1.0f
        );
    }

    bool operator==(const UITransform& other)
    {
        return (x == other.x && y == other.y
                && angle == other.angle
                && scaleX == other.scaleX && scaleY == other.scaleY);
    }

    UITransform& operator=(const UITransform&) = default;
};

class UIElement
{
    private:
        glm::mat3 world{1.0f};
        std::string name;
        std::string type;
        std::string tag;

    public:
        bool draw = true;
        UITransform transform;
        std::vector<std::unique_ptr<UIElement>> children;
        UIElement* parent = nullptr;

        UIElement() = default;
        UIElement(const UITransform& t)
            : transform(t) {}

        virtual void UploadUI();
        virtual void ComposeUI();
        virtual void DrawUI();

        void setWorld(const glm::mat3& world) { this->world = world; }
        glm::mat3 getWorld() const  { return this->world; }

        void setName(std::string n) { this->name = n; }
        std::string getName() const { return this->name; }

        void setType(std::string t) { this->type = t; }
        std::string getType() const { return this->type; }

        void setTag(std::string t) { this->tag = t; }
        std::string getTag() const { return this->tag; }

        void addChild(std::unique_ptr<UIElement> child) {
            child->parent = this;
            this->children.push_back(std::move(child));
        }

        virtual ~UIElement() = default;
};

class UIImage : public UIElement
{
    private:
        std::vector<float> vertices;
        std::vector<unsigned int> indices;
        unsigned int texture = 0;

        GLsizei indexCount = 6;
        unsigned int VAO = 0, VBO = 0, EBO = 0;
    
    public:
        UIImage(const UITransform& transform, const char* path)
        {
            std::pair<int, int> dimensions = textureDimensions(path);
            float width = dimensions.first, height = dimensions.second;

            std::vector<float> verts = {
                -width/2, -height/2, 0.0f, 0.0f,
                -width/2,  height/2, 0.0f, 1.0f,
                 width/2,  height/2, 1.0f, 1.0f,
                 width/2, -height/2, 1.0f, 0.0f
            };
            std::vector<unsigned int> idx = std::vector<unsigned int>{
                0, 1, 2,
                0, 2, 3
            };

            this->setVertices(verts);
            this->setIndices(idx);
            this->transform = transform;
            this->setTexture(loadTexture(path, true));
        }

        void UploadUI() override;
        void DrawUI() override;

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

        ~UIImage() override;
};

struct Glyph
{
    float u0, v0, u1, v1; // uv coords for char on font atlas
    float w, h;           // glyph quad size
    float xoff, yoff;
    float xadvance;
};

struct Font
{
    GLuint atlas = 0;
    int atlasW = 0, atlasH = 0;
    float bakePixelHeight = 0.0f;
    float ascent = 0.0f;
    float lineHeight = 0.0f;
    Glyph glyphs[96];
};

class UIText : public UIElement
{
    private:
        std::vector<float> vertices;
        std::vector<unsigned int> indices;
        GLuint VAO = 0, VBO = 0, EBO = 0;

    public:
        std::string text;
        float size;
        int fontIndex = -1;
        glm::vec3 color{1.0f};
        unsigned char anchorX = 'c';
        unsigned char anchorY = 'c';

        UIText(const UITransform& transform, const std::string& text,
               float size, int fontIndex, const glm::vec3& color,
               unsigned char anchorX, unsigned char anchorY)
               : text(text), size(size), fontIndex(fontIndex),
                 color(color), anchorX(anchorX), anchorY(anchorY)
        { this->transform = transform;}
        
        void UploadUI() override;
        void ComposeUI() override;
        void DrawUI() override;

         void setVertices(const std::vector<float>& verts) { this->vertices = verts; }
        std::vector<float> getVertices() const { return this->vertices; }

        void setIndices(const std::vector<unsigned int>& idx) { this->indices = idx; }
        std::vector<unsigned int> getIndices() const { return this->indices; }

        //void setVAO(unsigned int vao) { this->VAO = vao; }
        unsigned int getVAO() const { return this->VAO; }

        //void setVBO(unsigned int vbo) { this->VBO = vbo; }
        unsigned int getVBO() const { return this->VBO; }

        //void setEBO(unsigned int ebo) { this->EBO = ebo; }
        unsigned int getEBO() const { return this->EBO; }
};

extern unsigned int uiProgram;

extern int uiModelLoc, uiProjectionLoc;
extern int uiTextModeLoc, uiTextColorLoc;

extern std::vector<std::unique_ptr<UIElement>> uiRoots;
extern std::vector<Font> fonts;

extern const char* uiVertShader;
extern const char* uiFragShader;

void buildUIProgram();

void beginUI();

void endUI();

Font bakeFont(const char* path, float pixelHeight);

void layoutText(UIText& t);