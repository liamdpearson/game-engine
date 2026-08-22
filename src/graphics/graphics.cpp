#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#include "graphics.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>                  // vec3, mat4, basic types
#include <glm/gtc/matrix_transform.hpp> // perspective, lookAt, rotate, radians
#include <glm/gtc/type_ptr.hpp>         // value_ptr (hand a matrix to OpenGL)

#include <iostream>
#include <algorithm>


// define scene variables
std::vector<Object*> rootObjs;

// lighting stuff
std::vector<Light*> lights;
float ambient = 0.4f;
std::vector<Tri> occluders;
std::vector<glm::vec3> lightGrid;

// for finding the bounds box of the scene for light grid
float minX = INFINITY;
float maxX = -INFINITY;
float minY = INFINITY;
float maxY = -INFINITY;
float minZ = INFINITY;
float maxZ = -INFINITY;

// define opengl variables
GLFWwindow* window;
int SW, SH;

float deltaTime, lastFrame, currentFrame;

unsigned int shaderProgram;

int modelLoc, projectionLoc, viewLoc, normalMatLoc;
int texLoc, lightMapLoc, lightModeLoc, lightDirLoc, objectLightLoc;

const char* vertexShaderSource = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in vec2 aLightMapCoord;

out vec2 TexCoord;
out vec3 Normal;
out vec2 LightMapCoord;
out vec3 FragPos;

uniform mat4 model;
uniform mat4 projection;
uniform mat4 view;
uniform mat3 normalMat;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    TexCoord = aTexCoord;
    LightMapCoord = aLightMapCoord;
    Normal = normalMat * aNormal;
    FragPos = vec3(model * vec4(aPos, 1.0));
}
)glsl";

const char* fragmentShaderSource = R"glsl(
#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
in vec3 Normal;
in vec2 LightMapCoord;
in vec3 FragPos;

uniform sampler2D tex;
uniform sampler2D lightMap; // only used when lightMode = 1
uniform int lightMode;
uniform vec3 lightDir;    // only used when lightMode = 0
uniform vec3 objectLight; // only used when lightMode = 0

void main()
{
    vec3 lit;

    if (lightMode == 0)
    {
        vec3 n = normalize(Normal);
        vec3 lightDir = normalize(vec3(lightDir));

        vec3 diff = max(dot(n, lightDir), 0.0) * objectLight;

        lit = objectLight;
    }
    else if (lightMode == 1)
    {
        vec4 l = texture(lightMap, LightMapCoord);
        lit = l.rgb;
    }

    vec4 t = texture(tex, TexCoord);
    FragColor = vec4(t.rgb * lit, t.a);
}
)glsl";


void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

// compiles glsl shader.
static unsigned int compileShader(GLenum type, const char* src)
{
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);

    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    return shader;
}

// links shaders to shader program.
static unsigned int linkShaderProgram(const char* vsSrc, const char* fsSrc)
{
    unsigned int vs = compileShader(GL_VERTEX_SHADER, vsSrc);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fsSrc);
    unsigned int sp = glCreateProgram();

    glAttachShader(sp, vs);
    glAttachShader(sp, fs);
    glLinkProgram(sp);

    int success;
    char infoLog[512];
    glGetProgramiv(sp, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(sp, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINK_FAILED\n" << infoLog << std::endl;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return sp;

}

// builds shader program and sets locations for shader variables.
void buildShaderProgram()
{
    shaderProgram = linkShaderProgram(vertexShaderSource, fragmentShaderSource);

    // vert shader uniforms
    modelLoc      = glGetUniformLocation(shaderProgram, "model");
    projectionLoc = glGetUniformLocation(shaderProgram, "projection");
    viewLoc       = glGetUniformLocation(shaderProgram, "view");
    normalMatLoc  = glGetUniformLocation(shaderProgram, "normalMat");

    // frag shader uniforms
    texLoc = glGetUniformLocation(shaderProgram, "tex");
    lightMapLoc = glGetUniformLocation(shaderProgram, "lightMap");
    lightModeLoc = glGetUniformLocation(shaderProgram, "lightMode");
    lightDirLoc = glGetUniformLocation(shaderProgram, "lightDir");
    objectLightLoc = glGetUniformLocation(shaderProgram, "objectLight");
}

// uses stb to image files as textures.
unsigned int loadTexture(const char* src, bool pixelated, bool clampEdge)
{
    unsigned int texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    GLint wrap = clampEdge ? GL_CLAMP_TO_EDGE : GL_REPEAT;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    clampEdge ? GL_LINEAR : GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                    pixelated ? GL_NEAREST : GL_LINEAR);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true); // pngs are top down gl expects bottom up
    unsigned char* data = stbi_load(src, &width, &height, &nrChannels, 0);
    if (data)
    {   
        GLenum format = nrChannels == 4 ? GL_RGBA : nrChannels == 3 ? GL_RGB : GL_RED;
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    } else {
        std::cout << "Failed to load texture: " << src << std::endl;
    }

    stbi_image_free(data);
    return texture;
}

// converts light map pixel rgb values to a texture.
unsigned int loadLightMap(std::vector<glm::vec3>& pixels, int width, int height)
{
    unsigned int lightMap;
    glGenTextures(1, &lightMap);
    glBindTexture(GL_TEXTURE_2D, lightMap);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8, width, height, 0, GL_RGB, GL_FLOAT, pixels.data());

    return lightMap;
}

// creates proj and view matrices for the camera and sends them to shader locations.
// takes in cam as an argument so a scene can have multiple cameras.
void configureCamera(const Camera& cam)
{
    int frameBuffWidth, frameBuffHeight;
    glfwGetFramebufferSize(window, &frameBuffWidth, &frameBuffHeight);

    glm::mat4 projection = glm::perspective(
        glm::radians(cam.FOV),                            // fov
        (float)frameBuffWidth / (float)frameBuffHeight, // window aspect ratio
        0.1f, 100.0f                                    // near and far clip dist
    );

    glm::mat4 view = glm::lookAt(
        cam.getPos(),
        cam.getPos() + cam.getFront(),
        cam.getUp()
    );

    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(viewLoc,       1, GL_FALSE, glm::value_ptr(view));
}

void Object::Upload()
{
    for (Object*& child : children)
        child->Upload();
}

void Mesh::Upload()
{
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO); // start recording into the VAO

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, 
            vertices.size() * sizeof(float), 
            vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 
            indices.size() * sizeof(unsigned int), 
            indices.data(), GL_STATIC_DRAW);

    const GLsizei stride = VERTEX_FLOATS * sizeof(float);
    // pos
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    // uv
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // norm
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(2);
    // uv2
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, (void*)(8 * sizeof(float)));
    glEnableVertexAttribArray(3);

    glBindVertexArray(0);

    Object::Upload();
}

void Camera::Upload()
{
    Object::Upload();
}

// recursive part of the draw walk.
void Object::Draw()
{
    for (Object*& child : children)
        child->Draw();
}

// takes in a point in space and returns the trilinearly interpolated
// color of that point based on the 8 known values in the light grid
// that make up the cube around the point.
// FUNCTION GENERATED BY CLAUDE *** AI ALERT!!! ***
static glm::vec3 gridLightAt(const glm::vec3& p)
{
    if (lightGrid.empty()) return glm::vec3(1.0f);

    const int nx = (int)(maxX - minX) + 1;
    const int ny = (int)(maxY - minY) + 1;
    const int nz = (int)(maxZ - minZ) + 1;

    float fx = std::clamp(p.x - minX, 0.0f, (float)(nx - 1));
    float fy = std::clamp(p.y - minY, 0.0f, (float)(ny - 1));
    float fz = std::clamp(p.z - minZ, 0.0f, (float)(nz - 1));

    int x0 = (int)std::floor(fx), x1 = std::min(x0 + 1, nx - 1);
    int y0 = (int)std::floor(fy), y1 = std::min(y0 + 1, ny - 1);
    int z0 = (int)std::floor(fz), z1 = std::min(z0 + 1, nz - 1);

    float tx = fx - x0, ty = fy - y0, tz = fz - z0;

    auto cell = [&](int gx, int gy, int gz) -> const glm::vec3& {
        return lightGrid[(gx * ny + gy) * nz + gz];
    };

    // Collapse z, then y, then x.
    glm::vec3 c00 = glm::mix(cell(x0, y0, z0), cell(x0, y0, z1), tz);
    glm::vec3 c01 = glm::mix(cell(x0, y1, z0), cell(x0, y1, z1), tz);
    glm::vec3 c10 = glm::mix(cell(x1, y0, z0), cell(x1, y0, z1), tz);
    glm::vec3 c11 = glm::mix(cell(x1, y1, z0), cell(x1, y1, z1), tz);

    glm::vec3 c0 = glm::mix(c00, c01, ty);
    glm::vec3 c1 = glm::mix(c10, c11, ty);

    return glm::mix(c0, c1, tx);
}
// FUNCTION GENERATED BY CLAUDE *** AI ALERT!!! ***

// if object is a mesh this will run.
void Mesh::Draw()
{
    glUniform1i(lightModeLoc, 0);

    
    

    glm::mat4 w = this->getWorld();
    glm::mat3 normalMat = glm::mat3(glm::transpose(glm::inverse(w)));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(w));
    glUniformMatrix3fv(normalMatLoc, 1, GL_FALSE, glm::value_ptr(normalMat));

    glm::vec3 lit = gridLightAt(glm::vec3(w[3][0], w[3][1], w[3][2]));
    glUniform3fv(objectLightLoc, 1, glm::value_ptr(lit));
    glUniform3fv(lightDirLoc, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 1.0f)));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(texLoc, 0);

    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);

    Object::Draw();
}

// if object is a static mesh this will run.
void StaticMesh::Draw()
{
    glUniform1i(lightModeLoc, 1);

    glm::mat4 w = this->getWorld();
    glm::mat3 normalMat = glm::mat3(glm::transpose(glm::inverse(w)));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(w));
    glUniformMatrix3fv(normalMatLoc, 1, GL_FALSE, glm::value_ptr(normalMat));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, this->getTexture());
    glUniform1i(texLoc, 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, lightMap);
    glUniform1i(lightMapLoc, 1);

    glBindVertexArray(this->getVAO());
    glDrawElements(GL_TRIANGLES, this->getIndexCount(), GL_UNSIGNED_INT, 0);

    Object::Draw();
}

// recursive part of the compose walk.
void Object::Compose()
{
    for (Object*& child : children) {
        child->setWorld(this->world * child->transform.matrix());
        child->Compose();
    }
}

// if object is a camera this will run instead of Object::Compose.
// sets pos, front, up and then calls Object::Compose on itself.
void Camera::Compose()
{   
    glm::mat4 world = this->getWorld();
    glm::mat3 basis(world);

    pos   = glm::vec3(world[3]);
    front = glm::normalize(basis * glm::vec3(0.0f, 0.0f, -1.0f));
    up    = glm::normalize(basis * glm::vec3(0.0f, 1.0f,  0.0f));

    Object::Compose();
}

// deletes VAO, VBO, EBO, and texture.
Mesh::~Mesh()
{
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteTextures(1, &texture);
}

// just deletes light map. doesnt need to call Mesh::~Mesh
// because destructors always run for parent classes unlike
// regular overriden functions.
StaticMesh::~StaticMesh()
{
    glDeleteTextures(1, &lightMap);
}