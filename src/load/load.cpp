#include "load.h"

#include "../graphics/graphics.h"
#include <glm/glm.hpp>                  // vec3, mat4, basic types
#include <xatlas/xatlas.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <iostream>
#include <map>


// loops through mesh tris and adds up area
static float getMeshArea(const std::vector<float>& verts,
                         const std::vector<unsigned int>& idx)
{
    float area;

    for (size_t i = 0; i + 2 < idx.size(); i += 3)
    {
        glm::vec3 pos[3];
        for (int c = 0; c < 3; c++)
        {
            size_t v = (size_t)idx[i + c] * VERTEX_FLOATS;
            pos[c] = glm::vec3(verts[v], verts[v + 1], verts[v + 2]);
        }
        area += 0.5f * glm::length(glm::cross(pos[2] - pos[0], pos[1] - pos[0]));
    }
    std::cout << "area: " << area << '\n';
    return area;
}

// uses xatlas to generate a second non overlapping 
// uv mapping for a static meshes lightmap.
static bool GenUV2(xatlas::Atlas*& atlas, std::vector<float>& verts,
                   std::vector<unsigned int>& idx, float resolution)
{
    atlas = xatlas::Create();

    xatlas::MeshDecl meshDecl;
    meshDecl.vertexCount = static_cast<uint32_t>(verts.size() / VERTEX_FLOATS);
    meshDecl.vertexPositionData = verts.data();
    meshDecl.vertexPositionStride = sizeof(float) * VERTEX_FLOATS;

    meshDecl.indexCount = static_cast<uint32_t>(idx.size());
    meshDecl.indexData = idx.data();
    meshDecl.indexFormat = xatlas::IndexFormat::UInt32;


    xatlas::AddMeshError error = xatlas::AddMesh(atlas, meshDecl);
    if (error != xatlas::AddMeshError::Success)
    {
        std::cout << "Error adding mesh: " << xatlas::StringForEnum(error) << '\n';
        xatlas::Destroy(atlas);
        return false;
    }

    xatlas::ChartOptions chartOptions;
    xatlas::PackOptions packOptions;
    packOptions.resolution = resolution;
    packOptions.padding = 4;

    xatlas::Generate(atlas, chartOptions, packOptions);

    xatlas::Mesh& outputMesh = atlas->meshes[0];

    std::vector<float> finalVerts;
    std::vector<unsigned int> finalIdx;

    for (uint32_t i = 0; i < outputMesh.vertexCount; i++)
    {
        const xatlas::Vertex& xatlasVert = outputMesh.vertexArray[i];

        const uint32_t ogVertIdx = xatlasVert.xref * VERTEX_FLOATS;
        //pos
        finalVerts.push_back(verts[ogVertIdx]);
        finalVerts.push_back(verts[ogVertIdx + 1]);
        finalVerts.push_back(verts[ogVertIdx + 2]);
        // uv1
        finalVerts.push_back(verts[ogVertIdx + 3]);
        finalVerts.push_back(verts[ogVertIdx + 4]);
        // norm
        finalVerts.push_back(verts[ogVertIdx + 5]);
        finalVerts.push_back(verts[ogVertIdx + 6]);
        finalVerts.push_back(verts[ogVertIdx + 7]);
        // uv2
        finalVerts.push_back(xatlasVert.uv[0] / static_cast<float>(atlas->width));
        finalVerts.push_back(xatlasVert.uv[1] / static_cast<float>(atlas->height));
    }
    
    for (uint32_t i = 0; i < outputMesh.indexCount; i++) {
        finalIdx.push_back(outputMesh.indexArray[i]);
    }

    verts = finalVerts;
    idx = finalIdx;

    std::cout << "Succesfully created second uv set. New vert count: "
              << finalVerts.size() / VERTEX_FLOATS << '\n';
    return true;
}

// loads obj file doesnt look at mtl yet
static bool loadObj(const char* path, std::vector<float>& outVerts,
                    std::vector<unsigned int>& outIndices)
{
    std::ifstream file(path);
    if (!file) { std::cout << "Failed to open " << path << '\n'; return false; }

    std::vector<glm::vec3> pos;
    std::vector<glm::vec2> uv;
    std::vector<glm::vec3> norm;

    std::map<std::tuple<int, int, int>, unsigned int> uniqueMap;

    std::string line;
    while(std::getline(file, line))
    {
        std::istringstream ss(line);
        std::string tag;
        ss >> tag;

        if (tag == "v") {
            float x, y, z;
            ss >> x >> y >> z;
            glm::vec3 temp(x, y, z);
            pos.push_back(temp);
        }
        else if (tag == "vt") {
            float u, v;
            ss >> u >> v;
            glm::vec2 temp(u, v);
            uv.push_back(temp);
        }
        else if (tag == "vn") {
            float x, y, z;
            ss >> x >> y >> z;
            glm::vec3 temp(x, y, z);
            norm.push_back(temp);
        }
        else if (tag == "f") {
            std::string vert;
            std::vector<unsigned int> face;
            while (ss >> vert) {
                std::istringstream vs(vert);
                std::string p, t, n;
                std::getline(vs, p, '/');
                std::getline(vs, t, '/');
                std::getline(vs, n, '/');
                int posIdx = std::stoi(p) - 1;
                int uvIdx = t.empty() ? -1 : std::stoi(t) - 1;
                int normIdx = n.empty() ? -1 : std::stoi(n) - 1;

                std::tuple<int, int, int> key(posIdx, uvIdx, normIdx);
                auto found = uniqueMap.find(key);
                if (found != uniqueMap.end()) {
                    face.push_back(found->second);
                } else {
                    unsigned int newIdx = (unsigned int)(outVerts.size() / VERTEX_FLOATS);
                    // pos
                    outVerts.push_back(pos[posIdx].x);
                    outVerts.push_back(pos[posIdx].y);
                    outVerts.push_back(pos[posIdx].z);
                    // uv
                    if (uvIdx >= 0) {
                        outVerts.push_back(uv[uvIdx].x);
                        outVerts.push_back(uv[uvIdx].y);
                    } else {
                        outVerts.push_back(0.0f);
                        outVerts.push_back(0.0f);
                    }
                    // norm
                    if (normIdx >= 0) {
                        outVerts.push_back(norm[normIdx].x);
                        outVerts.push_back(norm[normIdx].y);
                        outVerts.push_back(norm[normIdx].z);
                    } else {
                        outVerts.push_back(0.0f);
                        outVerts.push_back(0.0f);
                        outVerts.push_back(0.0f);
                    }
                    // extra two to be filled with second uv set for lightmaps
                    outVerts.push_back(0.0f);
                    outVerts.push_back(0.0f);

                    uniqueMap[key] = newIdx;
                    face.push_back(newIdx);
                }
            }
            // triangulate as a face
            for (size_t i = 1; i + 1 < face.size(); i++) {
                outIndices.push_back(face[0]);
                outIndices.push_back(face[i]);
                outIndices.push_back(face[i+1]);
            }
        }
    }
    return true;
}

StaticMesh makeStaticObj(const Transform& transform, const char* objPath,
                         const char* texPath, bool pixelated)
{   
    StaticMesh obj;

    std::vector<float> verts;
    std::vector<unsigned int> idx;
    loadObj(objPath, verts, idx);

    xatlas::Atlas* atlas;
    float ogArea = getMeshArea(verts, idx);
    float scaledArea = (transform.scaleX * transform.scaleY +
                        transform.scaleX * transform.scaleZ +
                        transform.scaleY * transform.scaleZ) * ogArea;
    float resolution = std::sqrt(scaledArea * 64 * 64);
    GenUV2(atlas, verts, idx, resolution);
    

    obj.transform = transform;
    obj.setVertices(verts);
    obj.setIndices(idx);
    obj.setTexture(loadTexture(texPath, pixelated));
    obj.setIndexCount((GLsizei)idx.size());
    obj.lightMode = 1;

    obj.setAtlas(atlas);
    
    return obj;
}

Mesh makeObj(const Transform& transform, const char* objPath,
             const char* texPath, bool pixelated)
{
    Mesh obj;

    std::vector<float> verts;
    std::vector<unsigned int> idx;
    loadObj(objPath, verts, idx);

    obj.transform = transform;
    obj.setVertices(verts);
    obj.setIndices(idx);
    obj.setTexture(loadTexture(texPath, pixelated));
    obj.setIndexCount((GLsizei)idx.size());

    return obj;
}