#include "graphics.h"

glm::vec3 gameCamPos   = glm::vec3(0.0f, 0.0f, 3.0f);
glm::vec3 gameCamFront = glm::vec3(0.0f, 0.0f, -1.0f);

glm::vec3 editCamPos   = glm::vec3(0.0f, 0.0f, 3.0f);
glm::vec3 editCamFront = glm::vec3(0.0f, 0.0f, -1.0f);

std::vector<Object*> parents;
bool paused = false;
bool editing = true;


void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_P && action == GLFW_PRESS) {
        paused = !paused;
    }
    else if (key == GLFW_KEY_E && action == GLFW_PRESS) {
        editing = !editing;
        if (!editing)
        {
            editCamPos = cameraPos;
            editCamFront = cameraFront;

            cameraPos = gameCamPos;
            cameraFront = gameCamFront;
        } else {
            cameraPos = editCamPos;
            cameraFront = editCamFront;
        }
    }
}


void update()
{

}

void removeChild(std::vector<Object*>& children, const Object* child)
{
    children.erase(std::remove(children.begin(), children.end(), child), children.end());
}


int main()
{
    initWindow();
    buildShaderProgram();
    glfwSetKeyCallback(window, key_callback);

    // --- init objects --- //

    Object gun = makeObject("assets/gun/gun.obj", "assets/gun/gun.png",
                                Transform{0, 0, 0,  0, 45, 1.0f});

    Object skull = makeObject("assets/skull/skull.obj", "assets/skull/skull.png",
                                 Transform{-1, 0, 0,  0, 0, 0.01f});

    gun.children.push_back(&skull);
    //removeChild(gun.children, &skull);
    
    
    parents.push_back(&gun);

    // A Doom-style billboard sprite. yaw/pitch are ignored for sprites; only
    // position (first 3) and scale (last) are used. Use a 1x1 RGBA PNG with a
    // transparent background so the cutout looks right.

    for (Object* obj : parents) obj->Upload();
        
    // --- render loop --- //
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        float speed = 2.5f * deltaTime; // deltaTime keeps speed steady regardless of FPS
        

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);
        
        clearBG(0.10f, 0.12f, 0.15f, 1.0f); // r, g, b, a

        for (Object* obj : parents)
        {
            obj->world_pos = obj->transform;
            obj->Draw();
        }

        gun.transform.yaw += 0.1;
        gun.transform.pitch += 0.1;

        if (editing)
        {
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) cameraPos += speed * cameraFront;
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) cameraPos -= speed * cameraFront;
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
                cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * speed;
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
                cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * speed;
            if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)        cameraPos += speed * cameraUp;
            if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)   cameraPos -= speed * cameraUp;
        } else {
            // apply game movement here
            // apply camera filters here
            if (!paused) update();
        }
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}   