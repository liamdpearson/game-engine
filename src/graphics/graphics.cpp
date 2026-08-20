#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#include "graphics.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>                  // vec3, mat4, basic types
#include <glm/gtc/matrix_transform.hpp> // perspective, lookAt, rotate, radians
#include <glm/gtc/type_ptr.hpp>         // value_ptr (hand a matrix to OpenGL)

#include <iostream>


GLFWwindow* window;
int SW, SH;

float deltaTime, lastFrame, currentFrame;

unsigned int shaderProgram;

int modelLoc, projectionLoc, viewLoc, normalMatLoc;
int texLoc, lightMapLoc, lightModeLoc, viewPosLoc;

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
uniform sampler2D lightMap;
uniform int lightMode;
uniform vec3 viewPos;

float ambient = 0.2;
float specularStrength = 0.5;
vec3 lightColor = vec3(1.0, 1.0, 1.0);

void main()
{
    vec3 lit;

    if (lightMode == 0)
    {
        vec3 n = normalize(Normal);
        vec3 lightDir = normalize(vec3(1.0, 0.0f, 1.0));

        vec3 diff = max(dot(n, lightDir), 0.0) * lightColor; 

        vec3 viewDir = normalize(viewPos - FragPos);
        vec3 reflectDir = reflect(-lightDir, n);

        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
        vec3 specular = spec * specularStrength * lightColor;

        lit = ambient + diff + specular;
        
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

unsigned int compileShader(GLenum type, const char* src)
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
    viewPosLoc = glGetUniformLocation(shaderProgram, "viewPos");
}

unsigned int loadPNGJPG(const char* src)
{
    unsigned int texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

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
    //glGenerateMipmap(GL_TEXTURE_2D);

    return lightMap;
}

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
    glUniform3fv(viewPosLoc, 1, glm::value_ptr(cam.getPos()));
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

void Object::Draw()
{
    for (Object*& child : children)
        child->Draw();
}

void Mesh::Draw()
{
    glUniform1i(lightModeLoc, lightMode);

    glm::mat4 w = this->getWorld();
    glm::mat3 normalMat = glm::mat3(glm::transpose(glm::inverse(w)));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(w));
    glUniformMatrix3fv(normalMatLoc, 1, GL_FALSE, glm::value_ptr(normalMat));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(texLoc, 0);

    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);

    Object::Draw();
}

void StaticMesh::Draw()
{
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, lightMap);
    glUniform1i(lightMapLoc, 1);

    Mesh::Draw();
}

void Object::Compose()
{
    for (Object*& child : children) {
        child->setWorld(this->world * child->transform.matrix());
        child->Compose();
    }
}

void Camera::Compose()
{   
    glm::mat4 world = this->getWorld();
    glm::mat3 basis(world);

    pos   = glm::vec3(world[3]);
    front = glm::normalize(basis * glm::vec3(0.0f, 0.0f, -1.0f));
    up    = glm::normalize(basis * glm::vec3(0.0f, 1.0f,  0.0f));

    Object::Compose();
}

Mesh::~Mesh()
{
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteTextures(1, &texture);
}

StaticMesh::~StaticMesh()
{
    glDeleteTextures(1, &lightMap);
}