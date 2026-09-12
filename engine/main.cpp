#include "collisions/collisions.h"
#include "graphics/graphics.h"
#include "input/input.h"
#include "lighting/lighting.h"
#include "load/load.h"
#include "scenes/scenes.h"
#include "scripts/scripts.h"
#include "ui/ui.h"

#include <iostream>
#include <algorithm>
#include <thread>
#include <chrono>


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
    loadScene(startupScene.c_str());
    initScripting();
    target_frame_duration = 0.00333;

    bakeSceneLighting();
    collectSceneColliders();

    for (std::unique_ptr<Object>& obj : rootObjs) obj->Upload();
    for (std::unique_ptr<UIElement>& ui : uiRoots) ui->UploadUI();

    startScripts();
    
    while(!glfwWindowShouldClose(window))
    {
        // wait for frame cap
        currentFrameTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = currentFrameTime - lastFrameTime;

        double time_left = target_frame_duration - elapsed.count();

        while (time_left > 0.0)
        {
            currentFrameTime = std::chrono::high_resolution_clock::now();
            elapsed = currentFrameTime - lastFrameTime;
            time_left = target_frame_duration - elapsed.count();
        }

        // set dt
        lastFrameTime = currentFrameTime;
        deltaTime = (float)(elapsed.count());



        // call update fn in scripts
        updateScripts();

        if (pendingScene != "") {
            swapScene(pendingScene.c_str());
            pendingScene = "";
        }

        if (keyHeld(GLFW_KEY_LEFT_ALT) && keyPressed(GLFW_KEY_F4))
            glfwSetWindowShouldClose(window, true);

        endFrameInput();

        for (std::unique_ptr<Object>& obj : rootObjs) obj->ComputePose();
        for (std::unique_ptr<Object>& obj : rootObjs) obj->Compose();

        // render here
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaderProgram);
        if (currentCam) configureCamera(currentCam);

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