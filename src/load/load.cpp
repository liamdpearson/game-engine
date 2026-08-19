#include "load.h"

#include "../graphics/graphics.h"
#include <glm/glm.hpp>                  // vec3, mat4, basic types
#include <fstream>
#include <sstream>
#include <vector>
#include <iostream>
#include <map>


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
                    unsigned int newIdx = (unsigned int)(outVerts.size() / 8);
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

Mesh makeObj(const Transform& transform, const char* objPath,
             const char* texPath)
{   
    Mesh obj;

    std::vector<float> verts;
    std::vector<unsigned int> idx;
    loadObj(objPath, verts, idx);
    std::cout << verts.size() << ' ' << idx.size();

    obj.transform = transform;
    obj.setTexture(loadTexture(texPath));
    obj.setIndexCount((GLsizei)idx.size());
    obj.setIndices(idx);
    obj.setVertices(verts);

    return obj;
}