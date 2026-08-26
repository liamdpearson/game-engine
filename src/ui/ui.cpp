#include "ui.h"

#include "../graphics/graphics.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>


unsigned int uiProgram;

int uiModelLoc, uiProjectionLoc;

std::vector<UIElement*> uiRoots;

const char* uiVertShader = R"glsl(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aUV;

uniform mat3 model;
uniform mat4 projection;

out vec2 TexCoord;

void main() {
    vec3 pos = model * vec3(aPos, 1.0);
    gl_Position = projection * vec4(pos.xy, 0.0, 1.0);
    TexCoord = aUV;
}
)glsl";

const char* uiFragShader = R"glsl(
#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
uniform sampler2D tex0;

void main() {
    vec4 c = texture(tex0, TexCoord);
    if (c.a < 0.5) discard; // drop transparent pixels

    FragColor = c;
}
)glsl";

void buildUIProgram()
{

    uiProgram = linkShaderProgram(uiVertShader, uiFragShader);

    uiModelLoc      = glGetUniformLocation(uiProgram, "model");
    uiProjectionLoc = glGetUniformLocation(uiProgram, "projection");
}

void beginUI()
{
    glUseProgram(uiProgram);

    glm::mat4 projection = glm::ortho(
        0.0f, (float)SW,
        (float)SH, 0.0f,
        -1.0f, 1.0f
    );

    glUniformMatrix4fv(uiProjectionLoc, 1, GL_FALSE, glm::value_ptr(projection));

    glDisable(GL_DEPTH_TEST);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glActiveTexture(GL_TEXTURE0);
}

void endUI()
{
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void UIElement::UploadUI()
{
    for (UIElement*& child : children) child->UploadUI();
}

void UIImage::UploadUI()
{
    glGenVertexArrays(1, &this->VAO);
    glGenBuffers(1, &this->VBO);
    glGenBuffers(1, &this->EBO);

    glBindVertexArray(this->VAO); // start recording into VAO

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
             this->indices.size() * sizeof(unsigned int),
             this->indices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, this->VBO);
    glBufferData(GL_ARRAY_BUFFER,
             this->vertices.size() * sizeof(float),
             this->vertices.data(), GL_STATIC_DRAW);

    const GLsizei stride = UI_FLOATS * sizeof(float);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0); // stop recording

    UIElement::UploadUI();
}

void UIElement::ComposeUI()
{
    this->world = this->parent
                  ? this->parent->getWorld() * this->transform.matrix()
                  : this->transform.matrix();
    
    for (UIElement*& child : this->children) child->ComposeUI();
}

void UIElement::DrawUI()
{
    for (UIElement*& child : this->children) child->DrawUI();
}

void UIImage::DrawUI()
{
    glUniformMatrix3fv(uiModelLoc, 1, GL_FALSE, glm::value_ptr(this->getWorld()));

    glBindTexture(GL_TEXTURE_2D, this->texture);
    glBindVertexArray(this->VAO);
    glDrawElements(GL_TRIANGLES, this->indexCount, GL_UNSIGNED_INT, 0);

    UIElement::DrawUI();
}

// deletes VAO, VBO, EBO, and texture.
UIImage::~UIImage()
{
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteTextures(1, &texture);
}
