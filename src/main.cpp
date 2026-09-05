#include "collisions/collisions.h"
#include "graphics/graphics.h"
#include "input/input.h"
#include "lighting/lighting.h"
#include "load/load.h"
#include "scripts/scripts.h"
#include "ui/ui.h"

#include <iostream>
#include <algorithm>


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
    initScripting();

    

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

    UIElement* f = findUIElement("fps", uiRoots);
    UIText* fps = dynamic_cast<UIText*>(f);

    if (!player || !camnhands || !glock || !camera || !crosshair || !fps) {
        std::cout << "Scene lookup failed:"
                  << " player=" << player
                  << " camnhands=" << camnhands
                  << " glock=" << glock
                  << " camera=" << camera
                  << " crosshair=" << crosshair
                  << " fps=" << fps << '\n';
        return -1;
    }

    camnhands->rig.setAnim(0);
    glock->rig.setAnim(0);

    bool walking = false;
    bool ads = false;
    int ammo = 17;

    int frames = 0;

    bakeSceneLighting();
    collectSceneColliders();

    for (std::unique_ptr<Object>& obj : rootObjs) obj->Upload();
    for (std::unique_ptr<UIElement>& ui : uiRoots) ui->UploadUI();

    for (const ScriptInstance& si : scripts) si.Start();
    
    while(!glfwWindowShouldClose(window))
    {
        frames++;
        // calculate delta time
        currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // call update fn in scripts
        for (const ScriptInstance& si : scripts) si.Update(deltaTime);

        // update here
        player->transform.yaw -= mouseDX() * 0.05;
        camnhands->transform.pitch -= mouseDY() * 0.05;
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
        
        if (keyHeld(GLFW_KEY_W)) {
            moveDir += forward;
        }
        if (keyHeld(GLFW_KEY_S)) {
            moveDir -= forward;
        }
        if (keyHeld(GLFW_KEY_A)) {
            moveDir += right;
        }
        if (keyHeld(GLFW_KEY_D)) {
            moveDir -= right;
        }
        if (keyHeld(GLFW_KEY_LEFT_SHIFT)) {
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
                glock->rig.setAnim(2, 0.01f, 0);
                ammo--;
            }
            else if (ammo == 1) {
                glock->rig.setAnim(3, 0.01f, 1);
                ammo--;
            }
            else {
                glock->rig.setAnim(4, 0.05f, 1);
            }
        }

        if (mouseButtonPressed(GLFW_MOUSE_BUTTON_2)) {
            ads = true;  crosshair->draw = false;
        }
        if (mouseButtonReleased(GLFW_MOUSE_BUTTON_2)) {
            ads = false; crosshair->draw = true;
        }

        if (keyPressed(GLFW_KEY_R) && glock->rig.currentAnim != 5) {
            glock->rig.setAnim(5, 0.05f, 0);
            ammo = 17;
        }

        int shouldBe = walking + ads * 2;
        if (camnhands->rig.currentAnim != shouldBe)
            camnhands->rig.setAnim(shouldBe, 0.1f);

        endFrameInput();

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