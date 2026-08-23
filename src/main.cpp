#include "main.h"

#include "graphics/graphics.h"
#include "load/load.h"
#include "lighting/lighting.h"
#include "collisions/collisions.h"

#include <iostream>


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

    // build shader program
    buildShaderProgram();

    // set callbacks
    glfwSetKeyCallback(window, keyCallback);
    glfwSetCursorPosCallback(window, mouseMoveCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);

    

    // define objects

    StaticMesh test = makeStaticObj(Transform{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.5f, 0.5f},
            "assets/testing_zone/testing_zone.obj", "assets/testing_zone/testing_zone.png", true, true);

    rootObjs.push_back(&test);

    Player player = Player{Transform{0.0f, 5.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f},
                           1.8f, 0.3f};

    rootObjs.push_back(&player);

    Camera camera{90.0f, glm::vec3(0.0f, player.height - player.radius, 0.0f),
                  glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f)};

    player.children.push_back(&camera);

    Mesh gun = makeObj(Transform{0.0f, 0.0f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f},
            "assets/gun/gun.obj", "assets/gun/gun.png", false);

    camera.children.push_back(&gun);
    
    // define lights
    Light light = Light{glm::vec3(0.0f, 10.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f),
                        20.0f, 20.0f, 3.0f};
    lights.push_back(&light);

    bakeSceneLighting();
    collectSceneColliders();

    for (Object*& obj : rootObjs) obj->Upload();
    


    while(!glfwWindowShouldClose(window))
    {
        // calculate delta time
        currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // update here
        player.transform.yaw -= xoff * 0.05;
        camera.transform.pitch -= yoff * 0.05;
        camera.transform.pitch = std::clamp(camera.transform.pitch, -89.0f, 89.0f);

        if (keyPressed(GLFW_KEY_ESCAPE)) { 
            glfwSetWindowShouldClose(window, true);
        }

        glm::vec3 moveDir = glm::vec3(0.0f);

        glm::vec3 right = glm::vec3(
            -sin(glm::radians(player.transform.yaw + 90.0f)),
            0.0f,
            -cos(glm::radians(player.transform.yaw + 90.0f))
        );
        
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
            moveDir += camera.getFront();
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
            moveDir -= camera.getFront();
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
            moveDir += right;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
            moveDir -= right;
        }
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            moveDir += camera.getUp();
        }
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
            moveDir -= camera.getUp();
        }
    
        if (glm::length(moveDir) > 0.0f)
            moveDir = glm::normalize(moveDir);
        
        player.transform.x += moveDir.x * deltaTime;
        player.transform.y += moveDir.y * deltaTime;
        player.transform.z += moveDir.z * deltaTime;

        resolvePlayerCollision(player, colliders);

        

        // reset global input indicators
        xoff = 0.0f; yoff = 0.0f;
        keys_pressed = {}; keys_released = {};
        mouse_buttons_pressed = {}; mouse_buttons_released = {};

        for (Object*& obj : rootObjs) {
            obj->setWorld(obj->transform.matrix());
            obj->Compose();
        }

        // render here
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaderProgram);
        configureCamera(camera);

        for (Object*& obj : rootObjs) obj->Draw();
        glBindVertexArray(0);
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}