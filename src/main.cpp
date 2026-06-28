#include "graphics.h"


int main()
{
    if (!glfwInit()) { std::fprintf(stderr, "failed to init GLFW\n"); return -1; }

    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    SW = mode->width;
    SH = mode->height;
    lastX = SW/2, lastY = SH/2;

    GLFWwindow* window;
    initWindow(window);
    
    // --- build the shader program ---
    unsigned int vs = compileShader(GL_VERTEX_SHADER, vertexShaderSrc);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSrc);
    unsigned int shaderProgram = glCreateProgram();
    int modelLoc, viewLoc, projectionLoc;

    buildShaderProgram(vs, fs, shaderProgram, modelLoc, viewLoc, projectionLoc);



    // --- init objects --- //
    std::vector<Object> objects;
    objects.push_back(makeObject("models/gun/gun.obj", "models/gun/gun.png",
                                std::vector<float>{0, 0, 0,  90, 45, 1.0f}));
    objects.push_back(makeObject("models/skull/skull.obj", "models/skull/skull.png",
                                 std::vector<float>{1, 1, 1,  0, 0, 0.01f}));

    for (Object& obj : objects)
        uploadObject(obj);


        
    // --- render loop --- //
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

        glActiveTexture(GL_TEXTURE0);
        for (Object& obj : objects)
        {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(obj.transform[0], obj.transform[1], obj.transform[2])); // pos
            model = glm::rotate(model, glm::radians(obj.transform[4]), glm::vec3(-1,0,0));  // pitch
            model = glm::rotate(model, glm::radians(obj.transform[3]), glm::vec3(0,1,0));  // yaw
            model = glm::scale(model, glm::vec3(obj.transform[5]));                        // scale
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

            glBindTexture(GL_TEXTURE_2D, obj.texture);
            glBindVertexArray(obj.VAO);
            glDrawElements(GL_TRIANGLES, obj.indexCount, GL_UNSIGNED_INT, 0);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}   