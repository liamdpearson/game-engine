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

    // build shader programs
    buildShaderProgram();
    buildUIProgram();

    // set callbacks
    glfwSetKeyCallback(window, keyCallback);
    glfwSetCursorPosCallback(window, mouseMoveCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);


    // define objects

    StaticMesh test = makeStaticObj(Transform{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f},
            "assets/testing_zone/testing_zone.obj", "assets/testing_zone/testing_zone.png", true, true);

    rootObjs.push_back(&test);

    Capsule player = Capsule{Transform{0.0f, 5.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f},
                           1.8f, 0.3f};

    rootObjs.push_back(&player);

    Camera camera{90.0f, glm::vec3(0.0f, player.height - player.radius, 0.0f),
                  glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f)};

    player.addChild(&camera);

    AnimatedMesh glock = makeAnimatedObj(
        Transform{0.1f, -0.125f, -0.2f, 180.0f, 0.0f, 0.0f, 0.01f, 0.01f, 0.01},
        "assets/glock/glocknhands.fbx", "assets/glock/glocknhands.png", true
    );

    int ammo = 17;
    glock.SetAnimation(0);
    camera.addChild(&glock);

    // Mesh gun = makeObj(Transform{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f},
    //         "assets/gun/gun.obj", "assets/gun/gun.png", false);

    // knight.addChild(&gun, knight.findBoneIndex("wrist_l"));

    
    // define lights

    Light light1{glm::vec3(0.0f, 10.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f),
                 40.0f, 40.0f, 1.0f};
    lights.push_back(&light1);

    Light light2{glm::vec3(5.0f, 7.0f, 0.0f), glm::vec3(1.0f, 0.0f, 1.0f),
                 20.0f, 10.0f, 1.0f};
    lights.push_back(&light2);


    // define ui

    UIImage crosshair{UITransform{SW/2, SH/2, 0.0f, 1.0f, 1.0f}, "assets/ui/crosshair.png"};

    uiRoots.push_back(&crosshair);

    // Font font = bakeFont("assets/fonts/terminal.ttf", 32.0f);

    // UIText textTest = UIText{UITransform{SW/2, SH/2, 0.0f, 1.0f, 1.0f}, "Hello World",
    //                          32.0f, &font, glm::vec3{1.0f}};
    
    // uiRoots.push_back(&textTest);


    bakeSceneLighting();
    collectSceneColliders();

    for (Object*& obj : rootObjs) obj->Upload();
    for (UIElement*& ui : uiRoots) ui->UploadUI();
    


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

        float acceleration = 30.0f * deltaTime;

        glm::vec3 moveDir = glm::vec3(0.0f);

        glm::vec3 forward = glm::vec3(
            -sin(glm::radians(player.transform.yaw)),
            0.0f,
            -cos(glm::radians(player.transform.yaw))
        );

        glm::vec3 right = glm::vec3(
            -sin(glm::radians(player.transform.yaw + 90.0f)),
            0.0f,
            -cos(glm::radians(player.transform.yaw + 90.0f))
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
        }
        if (keyPressed(GLFW_KEY_SPACE) && player.grounded) {
            player.velocity.y += 5.0f;
        }
    
        if (glm::length(moveDir) > 0.0f)
            moveDir = glm::normalize(moveDir);
        
        player.velocity.y += -9.81f * deltaTime;
        if (player.velocity.y < -50.0f) player.velocity.y = -50.0f;

        float damping = powf(0.005f, deltaTime);
        player.velocity.x *= damping;
        player.velocity.z *= damping;
        player.velocity += moveDir * acceleration;

        glm::vec3 step = player.velocity * deltaTime;
        int substeps = glm::max(1, (int)glm::ceil(glm::length(step) / (player.radius * 0.5f)));

        bool groundedAny = false;

        for (int i = 0; i < substeps; ++i)
        {
            player.transform.x += step.x / substeps;
            player.transform.y += step.y / substeps;
            player.transform.z += step.z / substeps;

            resolveCapsuleCollision(player, colliders);

            if (player.grounded) groundedAny = true;
        }

        player.grounded = groundedAny;

        if (mouseButtonPressed(GLFW_MOUSE_BUTTON_1) && glock.currentAnim != 6) {
            if (ammo > 1) {
                glock.SetAnimation(2, 0.05f, 0);
                ammo--;
            }
            else if (ammo == 1) {
                glock.SetAnimation(3, 0.05f, 1);
                ammo--;
            }
            else {
                glock.SetAnimation(4, 0.05f, 1);
            }
        }

        if (keyPressed(GLFW_KEY_R)) {
            ammo = 17;
        }

        // reset global input indicators
        xoff = 0.0f; yoff = 0.0f;
        keys_pressed = {}; keys_released = {};
        mouse_buttons_pressed = {}; mouse_buttons_released = {};

        for (Object*& obj : rootObjs) obj->ComputePose();
        
        for (Object*& obj : rootObjs) obj->Compose();

        // render here
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaderProgram);
        configureCamera(camera);

        for (Object*& obj : rootObjs) obj->Draw();
        glBindVertexArray(0);

        beginUI();
        for (UIElement*& ui : uiRoots) ui->ComposeUI();
        for (UIElement*& ui : uiRoots) ui->DrawUI();
        endUI();
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}