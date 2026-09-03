#define STB_TRUETYPE_IMPLEMENTATION
#include <stb/stb_truetype.h>

#include "ui.h"

#include "../graphics/graphics.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <fstream>
#include <sstream>


unsigned int uiProgram;

int uiModelLoc, uiProjectionLoc;
int uiTextModeLoc, uiTextColorLoc;

std::vector<std::unique_ptr<UIElement>> uiRoots;
std::vector<Font> fonts;

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
uniform int TextMode; // 1 on text
uniform vec3 TextColor; // empty if not text

void main() {
    vec4 c = texture(tex0, TexCoord);
    if (c.a < 0.5) discard; // drop transparent pixels
    if (TextMode == 0) {
        FragColor = c;
    } 
    else if (TextMode == 1) {
        float a = c.r;
        FragColor = vec4(TextColor, a);
    }
}
)glsl";

void buildUIProgram()
{

    uiProgram = linkShaderProgram(uiVertShader, uiFragShader);

    uiModelLoc      = glGetUniformLocation(uiProgram, "model");
    uiProjectionLoc = glGetUniformLocation(uiProgram, "projection");

    uiTextModeLoc   = glGetUniformLocation(uiProgram, "TextMode");
    uiTextColorLoc  = glGetUniformLocation(uiProgram, "TextColor");
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

Font bakeFont(const char* path, float pixelHeight)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) std::cout << "ERROR: Couldn't find " << path << '\n';
    std::vector<unsigned char> ttf((std::istreambuf_iterator<char>(f)),
                                    std::istreambuf_iterator<char>());

    Font font;
    font.bakePixelHeight = pixelHeight;
    font.atlasW = 512;
    font.atlasH = 512;

    std::vector<unsigned char> bitmap(font.atlasW * font.atlasH);
    stbtt_bakedchar cdata[96];
    int ok = stbtt_BakeFontBitmap(ttf.data(), 0, pixelHeight, bitmap.data(),
                                  font.atlasW, font.atlasH, 32, 96, cdata);
    if (ok <= 0) std::cout << "FONT ERROR: font ran out of atlas room" << '\n';

    stbtt_fontinfo info;
    stbtt_InitFont(&info, ttf.data(), 0);
    int asc, desc, gap;
    stbtt_GetFontVMetrics(&info, &asc, &desc, &gap);
    float sc = stbtt_ScaleForPixelHeight(&info, pixelHeight);
    font.ascent = asc * sc;
    font.lineHeight = (asc - desc + gap) * sc;

    for (int i = 0; i < 96; ++i)
    {
        const stbtt_bakedchar&c = cdata[i];
        Glyph& g = font.glyphs[i];
        g.u0 = c.x0 / (float)font.atlasW;  g.v0 = c.y0 / (float)font.atlasH;
        g.u1 = c.x1 / (float)font.atlasW;  g.v1 = c.y1 / (float)font.atlasH;
        g.w  = (float)(c.x1 - c.x0);       g.h  = (float)(c.y1 - c.y0);
        g.xoff = c.xoff;                   g.yoff = c.yoff;
        g.xadvance = c.xadvance; 
    }

    glGenTextures(1, &font.atlas);
    glBindTexture(GL_TEXTURE_2D, font.atlas);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED,
                 font.atlasW, font.atlasH, 0,
                 GL_RED, GL_UNSIGNED_BYTE, bitmap.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    return font;
}

// adds glyph to vertices and indices
static void pushGlyphQuad(std::vector<float>& verts, std::vector<unsigned int>& idx,
                          float x0, float y0, float x1, float y1, const Glyph& g)
{
    unsigned int base = (unsigned int)(verts.size() / UI_FLOATS);
    verts.insert(verts.end(), { x0, y0, g.u0, g.v0 });
    verts.insert(verts.end(), { x1, y0, g.u1, g.v0 });
    verts.insert(verts.end(), { x1, y1, g.u1, g.v1 });
    verts.insert(verts.end(), { x0, y1, g.u0, g.v1 });
    idx.insert(idx.end(), { base+0, base+1, base+2, base+0, base+2, base+3 });


}

void layoutText(UIText& t)
{
    std::vector<float> verts;
    std::vector<unsigned int> idx;

    if (t.fontIndex < 0 || (size_t)t.fontIndex >= fonts.size()) return;

    const Font& f = fonts[t.fontIndex];
    float s = t.size / f.bakePixelHeight;

    std::istringstream stream(t.text);
    std::vector<std::string> lines;
    std::string line;

    while(std::getline(stream, line))
        lines.push_back(line);

    int lineCount = lines.size();

    float baseY = s;
    switch (t.anchorY)
    {
        case 'b':
            baseY *= f.lineHeight * -(float)(lineCount) + f.ascent;
            break;

        case 'c':
            baseY *= (f.lineHeight * -(float)(lineCount) + 2.0f*f.ascent) / 2.0f;
            break;

        default:
            baseY *= f.ascent;
            break;
    }   

    for (std::string& line : lines)
    {
        float lineLength = 0.0f;
        for (unsigned char ch : line)
        {
            if (ch < 32 || ch > 126) continue; // out of range skip it

            const Glyph& g = f.glyphs[ch - 32];
            lineLength += g.xadvance;
        }

        float penX = s;
        switch (t.anchorX)
        {
            case 'r':
                penX *= -lineLength;
                break;

            case 'c':
                penX *= -lineLength / 2.0f;
                break;

            default:
                penX *= 0.0f;
                break;
        }

        for (unsigned char ch : line)
        {
            if (ch < 32 || ch > 126) continue; // out of range skip it

            const Glyph& g = f.glyphs[ch - 32];
            if (g.w > 0 && g.h > 0)
            {
                float x0 = penX + g.xoff * s;
                float y0 = baseY + g.yoff * s;
                pushGlyphQuad(verts, idx, x0, y0, x0 + g.w * s, y0 + g.h * s, g);
            }
            penX += g.xadvance * s;
        }

        baseY += f.lineHeight * s;
    }

    t.setVertices(verts);
    t.setIndices(idx);
}

UIElement* findUIElement(const std::string& name,
                         const std::vector<std::unique_ptr<UIElement>>& elements)
{
    for (const std::unique_ptr<UIElement>& ui : elements)
    {
        if (ui->getName() == name) return ui.get();
        UIElement* found = findUIElement(name, ui->children);
        if (found) return found;
    }
    return nullptr;
}

void UIElement::UploadUI()
{
    for (std::unique_ptr<UIElement>& child : children) child->UploadUI();
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
    // pos
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    // uv
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0); // stop recording

    UIElement::UploadUI();
}

void UIText::UploadUI()
{
    glGenVertexArrays(1, &this->VAO);
    glGenBuffers(1, &this->VBO);
    glGenBuffers(1, &this->EBO);

    glBindVertexArray(this->VAO); // start recording into VAO

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->EBO);
    glBindBuffer(GL_ARRAY_BUFFER, this->VBO);

    const GLsizei stride = UI_FLOATS * sizeof(float);
    // pos
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    // uv
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
    
    for (std::unique_ptr<UIElement>& child : children) child->ComposeUI();
}

void UIText::ComposeUI()
{
    UIElement::ComposeUI();

    layoutText(*this);

    glBindVertexArray(this->VAO);

    glBindBuffer(GL_ARRAY_BUFFER, this->VBO);
    glBufferData(GL_ARRAY_BUFFER,
             this->vertices.size() * sizeof(float),
             this->vertices.data(), GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
             this->indices.size() * sizeof(unsigned int),
             this->indices.data(), GL_DYNAMIC_DRAW);

    glBindVertexArray(0);
}

void UIElement::DrawUI()
{
    for (std::unique_ptr<UIElement>& child : children) child->DrawUI();
}

void UIImage::DrawUI()
{
    if (!draw) return;
    glUniformMatrix3fv(uiModelLoc, 1, GL_FALSE, glm::value_ptr(this->getWorld()));
    glUniform1i(uiTextModeLoc, 0);

    glBindTexture(GL_TEXTURE_2D, this->texture);
    glBindVertexArray(this->VAO);
    glDrawElements(GL_TRIANGLES, this->indexCount, GL_UNSIGNED_INT, 0);

    UIElement::DrawUI();
}

void UIText::DrawUI()
{
    if (!draw) return;
    if (this->fontIndex < 0 || (size_t)this->fontIndex >= fonts.size() || this->indices.empty())
    { UIElement::DrawUI(); return; }

    glUniformMatrix3fv(uiModelLoc, 1, GL_FALSE, glm::value_ptr(this->getWorld()));
    glUniform1i(uiTextModeLoc, 1);
    glUniform3fv(uiTextColorLoc, 1, glm::value_ptr(this->color));

    glBindTexture(GL_TEXTURE_2D, fonts[fontIndex].atlas);
    glBindVertexArray(this->VAO);
    glDrawElements(GL_TRIANGLES, (GLsizei)this->indices.size(), GL_UNSIGNED_INT, 0);

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