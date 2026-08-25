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
layout (location = 4) in uint aBoneIndices;
layout (location = 5) in vec4 aBoneWeights;

const int MAX_BONES = 256;

out vec2 TexCoord;
out vec3 Normal;
out vec2 LightMapCoord;
out vec3 FragPos;

uniform mat4 model;
uniform mat4 projection;
uniform mat4 view;
uniform mat3 normalMat;
uniform mat4 boneMatrices[MAX_BONES];

void main()
{
    float total = aBoneWeights.x + aBoneWeights.y + aBoneWeights.z + aBoneWeights.w;

    vec4 pos;
    if (total > 0.0001) {
        uint b0 = (aBoneIndices >> 24) & 0xFFu;
        uint b1 = (aBoneIndices >> 16) & 0xFFu;
        uint b2 = (aBoneIndices >>  8) & 0xFFu;
        uint b3 =  aBoneIndices        & 0xFFu;

        mat4 skin = 
            aBoneWeights.x * boneMatrices[b0] +
            aBoneWeights.y * boneMatrices[b1] +
            aBoneWeights.z * boneMatrices[b2] +
            aBoneWeights.w * boneMatrices[b3];
        pos = skin * vec4(aPos, 1.0);

    } else {
        pos = vec4(aPos, 1.0);
    }

    gl_Position = projection * view * model * pos;
    TexCoord = aTexCoord;
    LightMapCoord = aLightMapCoord;
    Normal = normalMat * aNormal;
    FragPos = vec3(model * pos);
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

        lit = objectLight * 0.5f + diff * 0.5f;
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
    // bone indices
    glVertexAttribIPointer(4, 1, GL_UNSIGNED_INT, stride, (void*)(10 * sizeof(float)));
    glEnableVertexAttribArray(4);
    // bone weights
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, stride, (void*)(11 * sizeof(float)));
    glEnableVertexAttribArray(5);

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

// CLAUDE GENERATED FUNCTION
// flattens a grid coordinate the same way bakeSceneLighting stored it,
// walking x on the outside and z on the inside.
static size_t gridIndex(int x, int y, int z, int ny, int nz)
{
    return ((size_t)x * ny + y) * nz + z;
}

// CLAUDE GENERATED FUNCTION
// takes in a point in space and returns the trilinearly interpolated
// color and dominant light direction of that point based on the 8 known
// values in the light grid that make up the cube around the point.
static std::pair<glm::vec3, glm::vec3> gridLightAt(const glm::vec3& p)
{
    // the grid holds one sample per unit, from min to max inclusive.
    int nx = (int)maxX - (int)minX + 1;
    int ny = (int)maxY - (int)minY + 1;
    int nz = (int)maxZ - (int)minZ + 1;

    // nothing baked yet (or a grid that doesn't match the bounds box), so
    // fall back to flat ambient with no direction.
    if (nx < 1 || ny < 1 || nz < 1 || lightGrid.size() != (size_t)nx * ny * nz)
        return std::pair<glm::vec3, glm::vec3>{glm::vec3(ambient), glm::vec3(0.0f)};

    // position relative to the corner of the bounds box, clamped inside it
    // so points outside the scene just use the nearest samples.
    float fx = glm::clamp(p.x - minX, 0.0f, (float)(nx - 1));
    float fy = glm::clamp(p.y - minY, 0.0f, (float)(ny - 1));
    float fz = glm::clamp(p.z - minZ, 0.0f, (float)(nz - 1));

    // low corner of the cube around p. held one short of the last sample so
    // the high corner is always in range, and both collapse if an axis only
    // has a single sample.
    int x0 = std::min((int)fx, std::max(nx - 2, 0));
    int y0 = std::min((int)fy, std::max(ny - 2, 0));
    int z0 = std::min((int)fz, std::max(nz - 2, 0));
    int x1 = std::min(x0 + 1, nx - 1);
    int y1 = std::min(y0 + 1, ny - 1);
    int z1 = std::min(z0 + 1, nz - 1);

    // how far p sits between the two corners on each axis.
    float tx = fx - (float)x0;
    float ty = fy - (float)y0;
    float tz = fz - (float)z0;

    size_t i000 = gridIndex(x0, y0, z0, ny, nz);
    size_t i100 = gridIndex(x1, y0, z0, ny, nz);
    size_t i010 = gridIndex(x0, y1, z0, ny, nz);
    size_t i110 = gridIndex(x1, y1, z0, ny, nz);
    size_t i001 = gridIndex(x0, y0, z1, ny, nz);
    size_t i101 = gridIndex(x1, y0, z1, ny, nz);
    size_t i011 = gridIndex(x0, y1, z1, ny, nz);
    size_t i111 = gridIndex(x1, y1, z1, ny, nz);

    // blend the corner colors along x, then y, then z.
    glm::vec3 lit00 = glm::mix(lightGrid[i000].first, lightGrid[i100].first, tx);
    glm::vec3 lit10 = glm::mix(lightGrid[i010].first, lightGrid[i110].first, tx);
    glm::vec3 lit01 = glm::mix(lightGrid[i001].first, lightGrid[i101].first, tx);
    glm::vec3 lit11 = glm::mix(lightGrid[i011].first, lightGrid[i111].first, tx);
    glm::vec3 lit = glm::mix(glm::mix(lit00, lit10, ty), glm::mix(lit01, lit11, ty), tz);

    // same blend for the directions. each corner points at whatever lit it
    // most, so the weighted average of the 8 is the dominant direction here.
    glm::vec3 dir00 = glm::mix(lightGrid[i000].second, lightGrid[i100].second, tx);
    glm::vec3 dir10 = glm::mix(lightGrid[i010].second, lightGrid[i110].second, tx);
    glm::vec3 dir01 = glm::mix(lightGrid[i001].second, lightGrid[i101].second, tx);
    glm::vec3 dir11 = glm::mix(lightGrid[i011].second, lightGrid[i111].second, tx);
    glm::vec3 dir = glm::mix(glm::mix(dir00, dir10, ty), glm::mix(dir01, dir11, ty), tz);

    // averaging unit vectors doesn't give a unit vector, and corners that
    // cancel out or were never lit leave nothing to normalise.
    if (glm::dot(dir, dir) > 0.0f) dir = glm::normalize(dir);

    return std::pair<glm::vec3, glm::vec3>{lit, dir};
}

// if object is a mesh this will run.
void Mesh::Draw()
{
    glUniform1i(lightModeLoc, 0);

    glm::mat4 w = this->getWorld();
    glm::mat3 normalMat = glm::mat3(glm::transpose(glm::inverse(w)));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(w));
    glUniformMatrix3fv(normalMatLoc, 1, GL_FALSE, glm::value_ptr(normalMat));

    std::pair<glm::vec3, glm::vec3> litAndDir = gridLightAt(glm::vec3(w[3][0], w[3][1], w[3][2]));
    glUniform3fv(objectLightLoc, 1, glm::value_ptr(litAndDir.first));
    glUniform3fv(lightDirLoc,    1, glm::value_ptr(litAndDir.second));

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