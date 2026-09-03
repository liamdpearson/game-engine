#include "collisions/collisions.h"
#include "graphics/graphics.h"
#include "lighting/lighting.h"
#include "load/load.h"
#include "ui/ui.h"

#include <iostream>
#include <algorithm>


std::vector<int> keys_pressed;
std::vector<int> mouse_buttons_pressed;

std::vector<int> keys_released;
std::vector<int> mouse_buttons_released;

float lastX, lastY;
float xPos, yPos;
float xoff, yoff;
bool firstMouse = true;

void mouseMoveCallback(GLFWwindow*, double xpos, double ypos)
{
    if (firstMouse) {
        lastX = (float)xpos; lastY = (float)ypos;
        xPos = (float)xpos; yPos = (float)ypos;
        firstMouse = false;
    } else {
        lastX = xPos; lastY = yPos;
        xPos = xpos; yPos = ypos;

        xoff += xPos - lastX;
        yoff += lastY - yPos;
    }
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS) keys_pressed.push_back(key);
    if (action == GLFW_RELEASE) keys_released.push_back(key);
}

bool keyPressed(int key)
{
    return std::find(keys_pressed.begin(), keys_pressed.end(), key) != keys_pressed.end();
}

bool keyReleased(int key)
{
    return std::find(keys_released.begin(), keys_released.end(), key) != keys_released.end();
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    if (action == GLFW_PRESS) mouse_buttons_pressed.push_back(button);
    if (action == GLFW_RELEASE) mouse_buttons_released.push_back(button);
}

bool mouseButtonPressed(int button)
{
    return std::find(mouse_buttons_pressed.begin(), mouse_buttons_pressed.end(), button) != mouse_buttons_pressed.end();
}

bool mouseButtonReleased(int button)
{
    return std::find(mouse_buttons_released.begin(), mouse_buttons_released.end(), button) != mouse_buttons_released.end();
}

Object* findObject(const std::string& name,
                   const std::vector<std::unique_ptr<Object>>& objs)
{
    for (const std::unique_ptr<Object>& obj : objs)
    {
        if (obj->getName() == name) return obj.get();
        Object* found = findObject(name, obj->children);
        if (found) return found;
    }
    return nullptr;
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

int main()
{
    if (!glfwInit()) { std::cout << "Failed to init GLFW\n"; return -1; }

    GLFWmonitor* moniter = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(moniter);
    SW = mode->width; SH = mode->height;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(SW, SH, "Game Engine", NULL, NULL);
    if (!window)
    {
        std::cout << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }
    glfwSetWindowPos(window, 0, 0);
    glfwMakeContextCurrent(window);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    if (glfwRawMouseMotionSupported())
        glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD\n";
        return -1;
    }

    glViewport(0, 0, SW, SH);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    glEnable(GL_DEPTH_TEST);

    // build shader programs
    buildShaderProgram();
    buildUIProgram();

    // set callbacks
    glfwSetKeyCallback(window, keyCallback);
    glfwSetCursorPosCallback(window, mouseMoveCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);

    // load scene
    loadScene("assets/scenes/scene1.json");

    // set scene variables
    Object* p = findObject("player", rootObjs);
    Capsule* player = dynamic_cast<Capsule*>(p);

    Object* c = findObject("camnhands", rootObjs);
    AnimatedObj* camnhands = dynamic_cast<AnimatedObj*>(c);
    
    Object* g = findObject("glock", rootObjs);
    AnimatedMesh* glock = dynamic_cast<AnimatedMesh*>(g);
    
    Object* cam = findObject("camera", rootObjs);
    Camera* camera = dynamic_cast<Camera*>(cam);

    UIElement* ch = findUIElement("crosshair", uiRoots);
    UIImage* crosshair = dynamic_cast<UIImage*>(ch);

    if (!player || !camnhands || !glock || !camera || !crosshair) {
        std::cout << "Scene lookup failed:"
                  << " player=" << player
                  << " camnhands=" << camnhands
                  << " glock=" << glock
                  << " camera=" << camera
                  << " crosshair=" << crosshair << '\n';
        return -1;
    }

    camnhands->rig.SetAnimation(0);
    glock->rig.SetAnimation(0);

    bool walking = false;
    bool ads = false;
    int ammo = 17;

    bakeSceneLighting();
    collectSceneColliders();

    for (std::unique_ptr<Object>& obj : rootObjs) obj->Upload();
    for (std::unique_ptr<UIElement>& ui : uiRoots) ui->UploadUI();
    

    while(!glfwWindowShouldClose(window))
    {
        // calculate delta time
        currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // update here
        player->transform.yaw -= xoff * 0.05;
        camnhands->transform.pitch -= yoff * 0.05;
        camnhands->transform.pitch = std::clamp(camnhands->transform.pitch, -90.0f, 90.0f);

        if (keyPressed(GLFW_KEY_ESCAPE)) { 
            glfwSetWindowShouldClose(window, true);
        }

        float acceleration = 30.0f * deltaTime;

        glm::vec3 moveDir = glm::vec3(0.0f);

        glm::vec3 forward = glm::vec3(
            -sin(glm::radians(player->transform.yaw)),
            0.0f,
            -cos(glm::radians(player->transform.yaw))
        );

        glm::vec3 right = glm::vec3(
            -sin(glm::radians(player->transform.yaw + 90.0f)),
            0.0f,
            -cos(glm::radians(player->transform.yaw + 90.0f))
        );
        
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
            moveDir += forward;
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
            moveDir -= forward;
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
            moveDir += right;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
            moveDir -= right;
        }
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
            acceleration *= 1.5f;
            camnhands->rig.animSpeed = 1.5f;
        } else {
            camnhands->rig.animSpeed = 1.0f;
        }
        if (keyPressed(GLFW_KEY_SPACE) && player->grounded) {
            player->velocity.y += 5.0f;
        }
        
        float moveDirLen = glm::length(moveDir);
        if (moveDirLen > 0.0f) {
            moveDir = glm::normalize(moveDir);
            walking = true;
        } else {
            walking = false;
        }
        
        player->velocity.y += -9.81f * deltaTime;
        if (player->velocity.y < -50.0f) player->velocity.y = -50.0f;

        float damping = powf(0.00005f, deltaTime);
        player->velocity.x *= damping;
        player->velocity.z *= damping;
        player->velocity += moveDir * acceleration;

        glm::vec3 step = player->velocity * deltaTime;
        int substeps = glm::max(1, (int)glm::ceil(glm::length(step) / (player->radius * 0.5f)));

        bool groundedAny = false;

        for (int i = 0; i < substeps; ++i)
        {
            player->transform.x += step.x / substeps;
            player->transform.y += step.y / substeps;
            player->transform.z += step.z / substeps;

            resolveCapsuleCollision(player, colliders);

            if (player->grounded) groundedAny = true;
        }
        player->grounded = groundedAny;


        if (mouseButtonPressed(GLFW_MOUSE_BUTTON_1) && glock->rig.currentAnim != 5) {
            if (ammo > 1) {
                glock->rig.SetAnimation(2, 0.01f, 0);
                ammo--;
            }
            else if (ammo == 1) {
                glock->rig.SetAnimation(3, 0.01f, 1);
                ammo--;
            }
            else {
                glock->rig.SetAnimation(4, 0.05f, 1);
            }
        }

        if (mouseButtonPressed(GLFW_MOUSE_BUTTON_2)) {
            ads = true;  crosshair->draw = false;
        }
        if (mouseButtonReleased(GLFW_MOUSE_BUTTON_2)) {
            ads = false; crosshair->draw = true;
        }

        if (keyPressed(GLFW_KEY_R) && glock->rig.currentAnim != 5) {
            glock->rig.SetAnimation(5, 0.05f, 0);
            ammo = 17;
        }

        int shouldBe = walking + ads * 2;
        if (camnhands->rig.currentAnim != shouldBe)
            camnhands->rig.SetAnimation(shouldBe, 0.1f);

        // reset global input indicators
        xoff = 0.0f; yoff = 0.0f;
        keys_pressed = {}; keys_released = {};
        mouse_buttons_pressed = {}; mouse_buttons_released = {};

        for (std::unique_ptr<Object>& obj : rootObjs) obj->ComputePose();
        for (std::unique_ptr<Object>& obj : rootObjs) obj->Compose();

        // render here
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaderProgram);
        configureCamera(camera);

        for (std::unique_ptr<Object>& obj : rootObjs) obj->Draw();
        glBindVertexArray(0);

        beginUI();
        for (std::unique_ptr<UIElement>& ui : uiRoots) ui->ComposeUI();
        for (std::unique_ptr<UIElement>& ui : uiRoots) ui->DrawUI();
        endUI();
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}