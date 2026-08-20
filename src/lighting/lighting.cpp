#include "lighting.h"

#include "../graphics/graphics.h"

#include <glm/glm.hpp>

#include <vector>
#include <iostream>
#include <map>


void Object::CollectOccluders(const glm::mat4 parentWorld, std::vector<Tri>& out)
{
    glm::mat4 world = parentWorld * transform.matrix();
    for (Object*& child : children) child->CollectOccluders(world, out);
}

void StaticMesh::CollectOccluders(const glm::mat4 parentWorld, std::vector<Tri>& out)
{
    glm::mat4 world = parentWorld * transform.matrix();
    const std::vector<unsigned int>& indices = this->getIndices();
    const std::vector<float>& vertices = this->getVertices();

    for (size_t i = 0; i + 2 < indices.size(); i += 3)
    {
        glm::vec3 tri[3];
        for (int c = 0; c < 3; c++)
        {
            size_t v = (size_t)indices[i + c] * VERTEX_FLOATS;
            glm::vec3 local(vertices[v], vertices[v + 1], vertices[v + 2]);
            tri[c] = glm::vec3(world * glm::vec4(local, 1.0f));
        }

        Tri t{tri[0], tri[1], tri[2]};
        out.push_back(t);
       
    }
    // pass parent world because not every object is a static mesh so
    // Object::CollectOccluders has to * transform.matrix() aswell.
    Object::CollectOccluders(parentWorld, out);
}

void Object::BakeLighting(const glm::mat4 parentWorld, const std::vector<Tri>& occluders)
{
    glm::mat4 world = parentWorld * transform.matrix();
    for (Object*& child : children) child->BakeLighting(world, occluders);
}

void StaticMesh::BakeLighting(const glm::mat4 parentWorld, const std::vector<Tri>& occluders)
{
    glm::mat4 world = parentWorld * transform.matrix();
    const std::vector<unsigned int>& indices = this->getIndices();
    const std::vector<float>& vertices = this->getVertices();

    // ADD CODE HERE THAT CREATES LIGHT MAP
    
    // pass parent world because not every object is a static mesh so
    // Object::CollectOccluders has to * transform.matrix() aswell.
    Object::BakeLighting(parentWorld, occluders);
}

bool bakeSceneLighting(const std::vector<Light>& lights,
                       std::vector<Object*>& rootObjs)
{
    double start = glfwGetTime();

    std::vector<Tri> occluders;
    for (Object*& obj : rootObjs)
        obj->CollectOccluders(glm::mat4(1.0f), occluders);
    
    std::cout << "bake: occluder Tris: " << occluders.size() << '\n';

    // for (Object*& obj : rootObjs)
    //     obj->BakeLighting(glm::mat4(1.0f), occluders);

    double end = glfwGetTime();
    std::cout << "bake: took " << (end - start) << "s\n";

    return true;
}
