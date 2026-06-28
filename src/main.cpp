// obj-creatir - Milestone 2: draw a triangle.

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>                  // vec3, mat4, basic types
#include <glm/gtc/matrix_transform.hpp> // perspective, lookAt, rotate, radians
#include <glm/gtc/type_ptr.hpp>         // value_ptr (hand a matrix to OpenGL)

#include <cstdio>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>


static const int SW = 1280;
static const int SH = 720;

// Runs once per vertex. Its only job: set gl_Position, the vertex's final
// position in "clip space". For now we pass our coordinates straight through.
// `layout (location = 0)` ties aPos to attribute slot 0 (we configure slot 0 below).
static const char* vertexShaderSrc = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)glsl";


// Runs once per fragment (≈ per pixel the triangle covers). Outputs the color.
// Here every fragment is the same orange.
static const char* fragmentShaderSrc = R"glsl(
#version 330 core
out vec4 FragColor;
void main()
{
    FragColor = vec4(1.0, 0.5, 0.2, 1.0); // R,G,B,A
}
)glsl";


// ----------------------------------------------------------------------------
// Helper: compile one shader and report any GLSL compile errors.
// (Shaders compile on the GPU driver — if you get a typo, you NEED this log.)
// ----------------------------------------------------------------------------
static unsigned int compileShader(GLenum type, const char* src)
{
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Shader compile error:\n%s\n", log);
    }
    return shader;
}


static void framebufferSizeCallback(GLFWwindow*, int width, int height)
{
    glViewport(0, 0, width, height);
}


glm::vec3 cameraPos   = glm::vec3(0.0f, 0.0f, 3.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f, 0.0f);

float yaw   = -90.0f;   // degrees. -90 so we start looking down -Z, not +X
float pitch = 0.0f;
float lastX = SW/2, lastY = SH/2;   // last mouse pos (start at screen center)
bool  firstMouse = true;

const float sensitivity = 0.1f;
float deltaTime = 0.0f, lastFrame = 0.0f;  // for frame-rate-independent speed


static void mouseCallback(GLFWwindow*, double xpos, double ypos)
{
    if (firstMouse) { lastX = (float)xpos; lastY = (float)ypos; firstMouse = false;}

    float xoffset = (float)(xpos - lastX);
    float yoffset = (float)(lastY - ypos);
    lastX = float(xpos);
    lastY = float(ypos);

    yaw += xoffset * sensitivity;
    pitch += yoffset * sensitivity;

    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}


static bool loadOBJ(const char* path,
                    std::vector<float>& outVerts,
                    std::vector<unsigned int>& outIndices,
                    const float xoff, const float yoff, const float zoff,
                    const float yaw, const float pitch)
{
    std::ifstream file(path);
    if (!file) { std::fprintf(stderr, "Cannot open %s\n", path); return false; }

    unsigned int num_verts = outVerts.size() / 3;

    std::string line;
    while(std::getline(file, line))
    {
        std::istringstream ss(line);
        std::string tag;
        ss >> tag;

        if (tag == "v") {
            float x, y, z;
            ss >> x >> y >> z;

            // rotate the raw vertex by the object's yaw (about Y) then pitch (about X),
            // then write the rotated values back into x/y/z for the offset step below.
            glm::mat4 rot(1.0f);
            rot = glm::rotate(rot, glm::radians(pitch), glm::vec3(-1.0f, 0.0f, 0.0f));
            rot = glm::rotate(rot, glm::radians(yaw),   glm::vec3(0.0f, 1.0f, 0.0f));
            glm::vec3 r = glm::vec3(rot * glm::vec4(x, y, z, 1.0f));

            outVerts.push_back(r.x + xoff);
            outVerts.push_back(r.y + yoff);
            outVerts.push_back(r.z + zoff);
        }
        else if (tag == "f") {
            std::string vert;
            std::vector<unsigned int> face;
            while (ss >> vert) {
                unsigned int idx = std::stoi(vert.substr(0, vert.find('/')));
                face.push_back(idx - 1);
            }
            // triangulate as a fan (handles triangles, quads, n-gons)
            for (size_t i = 1; i + 1 < face.size(); i++) {
                outIndices.push_back(face[0] + num_verts);
                outIndices.push_back(face[i] + num_verts);
                outIndices.push_back(face[i+1] + num_verts);
            }
        }
        // ignore vt, vn, #, o, g, s, mtllib, usemtl for now
    }
    return true;
}


int main()
{
    // --- window + context (same as Milestone 1) ---
    if (!glfwInit()) { std::fprintf(stderr, "failed to init GLFW\n"); return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SW, SH, "OpenGL", nullptr, nullptr);
    if (!window) { std::fprintf(stderr, "Failed to create window\n"); glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    { std::fprintf(stderr, "Failed to init GLAD\n"); glfwTerminate(); return -1; }

    glEnable(GL_DEPTH_TEST);

    // --- build the shader program ---
    unsigned int vs = compileShader(GL_VERTEX_SHADER, vertexShaderSrc);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSrc);

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vs);
    glAttachShader(shaderProgram, fs);
    glLinkProgram(shaderProgram);

    int linked = 0;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        char log[512];
        glGetProgramInfoLog(shaderProgram, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Program link error:\n%s\n", log);
    }

    int modelLoc      = glGetUniformLocation(shaderProgram, "model");
    int viewLoc       = glGetUniformLocation(shaderProgram, "view");
    int projectionLoc = glGetUniformLocation(shaderProgram, "projection");

    // The individual shaders are now baked into the program; we can delete them.
    glDeleteShader(vs);
    glDeleteShader(fs);

    // --- the triangle's vertex data ---
    // Coordinates are in Normalized Device Coordinates (NDC): the visible screen
    // runs from -1 to +1 on both x and y, regardless of window size. z is depth.
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    loadOBJ("models/gun.obj", vertices, indices, 0.0f, 0.0f, 0.0f, 90.0f, 45.0f);
    loadOBJ("models/gun.obj", vertices, indices, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f);

    // VAO: records the "how to read the buffer" config so we can re-bind it later
    // with one call. In the Core profile you MUST have a VAO bound to draw.
    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO); // start recording into this VAO

    unsigned int EBO;
    glGenBuffers(1, &EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
             indices.size() * sizeof(unsigned int),
             indices.data(), GL_STATIC_DRAW);

    // VBO: upload the vertex bytes to GPU memory.
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER,
             vertices.size() * sizeof(float),
             vertices.data(), GL_STATIC_DRAW); // change GL_STATIC_DRAW to GL_DYNAMIC_DRAW if tri will warp in 3d space

    // Tell GL how to interpret the buffer for attribute slot 0 (our aPos):
    //   index 0, 3 floats per vertex, not normalized,
    //   stride = 3 floats between vertices, offset 0 at the start.
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0); // stop recording (optional tidy-up)

    // --- render loop ---
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        float speed = 2.5f * deltaTime; // deltaTime keeps speed steady regardless of FPS
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) cameraPos += speed * cameraFront;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) cameraPos -= speed * cameraFront;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * speed;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * speed;
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)        cameraPos += speed * cameraUp;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)   cameraPos -= speed * cameraUp;

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);
        
        glClearColor(0.10f, 0.12f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        int fbw, fbh;
        glfwGetFramebufferSize(window, &fbw, &fbh);
        
        glm::mat4 projection = glm::perspective(
            glm::radians(45.0f),                 // vertical field of view
            (float)fbw / (float)fbh,             // aspect ratio
            0.1f, 100.0f);                       // near & far clip planes
        
        glm::mat4 view = glm::lookAt(
            cameraPos,         // camera position (Step 5 fills these in)
            cameraPos + cameraFront,         // look at the origin (where the triangle is)
            cameraUp);        // "up" direction

        glm::mat4 model = glm::mat4(1.0f);       // identity: triangle stays at the origin

        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(viewLoc,       1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(modelLoc,      1, GL_FALSE, glm::value_ptr(model));

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, (GLsizei)indices.size(), GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // --- cleanup ---
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);

    glfwTerminate();
    return 0;
}   