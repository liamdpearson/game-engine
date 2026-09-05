#include "load.h"

#include "../graphics/graphics.h"
#include "../lighting/lighting.h"
#include "../scripts/scripts.h"
#include "../ui/ui.h"
#include <glm/glm.hpp>                  // vec3, mat4, basic types
#include <xatlas/xatlas.h>
#include <nlohmann/json.hpp>
#include <ufbx/ufbx.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <iostream>
#include <cstring>
#include <map>

using json = nlohmann::json;


// loops through mesh tris and adds up area
static float getMeshArea(const std::vector<float>& verts,
                         const std::vector<unsigned int>& idx)
{
    float area = 0.0f;

    for (size_t i = 0; i + 2 < idx.size(); i += 3)
    {
        glm::vec3 pos[3];
        for (int c = 0; c < 3; ++c)
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

    for (uint32_t i = 0; i < outputMesh.vertexCount; ++i)
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
        // anim data
        finalVerts.push_back(verts[ogVertIdx + 10]);
        finalVerts.push_back(verts[ogVertIdx + 11]);
        finalVerts.push_back(verts[ogVertIdx + 12]);
        finalVerts.push_back(verts[ogVertIdx + 13]);
        finalVerts.push_back(verts[ogVertIdx + 14]);
    }
    
    for (uint32_t i = 0; i < outputMesh.indexCount; ++i) {
        finalIdx.push_back(outputMesh.indexArray[i]);
    }

    verts = finalVerts;
    idx = finalIdx;

    std::cout << "Succesfully created second uv set. New vert count: "
              << finalVerts.size() / VERTEX_FLOATS << '\n';
    return true;
}

// loads .fbx file unanimated
static void loadFbxUnanimated(const char* path, std::vector<float>& outVerts,
                    std::vector<unsigned int>& outIndices)
{
    ufbx_load_opts opts = {}; // default options
    opts.target_axes = ufbx_axes_right_handed_y_up;
    opts.target_unit_meters = 1.0f;

    ufbx_error error;
    ufbx_scene* scene = ufbx_load_file(path, &opts, &error);
    if (!scene)
    {
        std::cout << "Failed to load scene. Path: " << path 
                  << " Description: " << error.description.data << '\n';
        ufbx_free_scene(scene);
        return;
    }

    for (size_t i = 0; i < scene->nodes.count; ++i)
    {
        ufbx_node* node = scene->nodes.data[i];
        ufbx_mesh* mesh = node->mesh;
        if (!mesh) continue;

        ufbx_matrix geomToWorld = node->geometry_to_world;
        ufbx_matrix geomToWorldNormals = ufbx_matrix_for_normals(&geomToWorld);
        std::vector<unsigned int> tri(mesh->max_face_triangles * 3);

        for (size_t fi = 0; fi < mesh->faces.count; ++fi)
        {
            ufbx_face face = mesh->faces[fi];
            size_t numTris = ufbx_triangulate_face(tri.data(), tri.size(), mesh, face);

            for (size_t i = 0; i < numTris * 3; ++i)
            {
                unsigned int corner = tri[i];
                ufbx_vec3 p = ufbx_get_vertex_vec3(&mesh->vertex_position, corner);
                p = ufbx_transform_position(&geomToWorld, p);
                ufbx_vec2 uv = mesh->vertex_uv.exists
                    ? ufbx_get_vertex_vec2(&mesh->vertex_uv, corner)
                    : ufbx_vec2{0.0f, 0.0f};
                ufbx_vec3 n = mesh->vertex_normal.exists
                    ? ufbx_get_vertex_vec3(&mesh->vertex_normal, corner)
                    : ufbx_vec3{0.0f, 0.0f, 1.0f};
                n = ufbx_transform_direction(&geomToWorldNormals, n);

                outIndices.push_back((unsigned int)(outVerts.size() / VERTEX_FLOATS));
                outVerts.push_back((float)p.x);
                outVerts.push_back((float)p.y);
                outVerts.push_back((float)p.z);
                outVerts.push_back((float)uv.x);
                outVerts.push_back((float)uv.y);
                outVerts.push_back((float)n.x);
                outVerts.push_back((float)n.y);
                outVerts.push_back((float)n.z);
                outVerts.push_back(0.0f); // light map uv
                outVerts.push_back(0.0f);
                outVerts.push_back(0.0f); // empty anim data
                outVerts.push_back(0.0f);
                outVerts.push_back(0.0f);
                outVerts.push_back(0.0f);
                outVerts.push_back(0.0f);
            }
        }
    }
}

// loads .obj file
static void loadObj(const char* path, std::vector<float>& outVerts,
                    std::vector<unsigned int>& outIndices)
{
    std::ifstream file(path);
    if (!file) { std::cout << "Failed to open " << path << '\n'; return; }

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
                    outVerts.push_back(0.0f); // second uv set for lightmaps
                    outVerts.push_back(0.0f);
                    outVerts.push_back(0.0f); // empty anim data
                    outVerts.push_back(0.0f);
                    outVerts.push_back(0.0f);
                    outVerts.push_back(0.0f);
                    outVerts.push_back(0.0f);

                    uniqueMap[key] = newIdx;
                    face.push_back(newIdx);
                }
            }
            // triangulate as a face
            for (size_t i = 1; i + 1 < face.size(); ++i) {
                outIndices.push_back(face[0]);
                outIndices.push_back(face[i]);
                outIndices.push_back(face[i+1]);
            }
        }
    }
}

std::unique_ptr<StaticMesh> makeStaticMesh(const Transform& transform, const char* objPath,
                          const char* texPath, bool pixelated, bool collides)
{   
    auto obj = std::make_unique<StaticMesh>();

    std::vector<float> verts;
    std::vector<unsigned int> idx;

    const char* file = strrchr(objPath, '.');
    if (file) {
        if (strcmp(file, ".obj") == 0) {
            loadObj(objPath, verts, idx);
        }
        else if (strcmp(file, ".fbx") == 0) {
            loadFbxUnanimated(objPath, verts, idx);
        }
        else {
            std::cout << "File not recognized: " << objPath << '\n';
        }
    } else {
        std::cout << "No file type: " << objPath << '\n';
    }
    
    xatlas::Atlas* atlas = nullptr;
    float ogArea = getMeshArea(verts, idx);
    float scaledArea = (transform.scaleX * transform.scaleY +
                        transform.scaleX * transform.scaleZ +
                        transform.scaleY * transform.scaleZ) * ogArea;
    float resolution = std::sqrt(scaledArea * 64 * 64) * 0.1f;
    if (!GenUV2(atlas, verts, idx, resolution)) atlas = nullptr;
    

    obj->transform = transform;
    obj->setVertices(verts);
    obj->setIndices(idx);
    obj->setTexture(loadTexture(texPath, pixelated));
    obj->setIndexCount((GLsizei)idx.size());

    obj->setAtlas(atlas);
    obj->setCollides(collides);
    
    return obj;
}

std::unique_ptr<Mesh> makeMesh(const Transform& transform, const char* objPath,
              const char* texPath, bool pixelated)
{
    auto obj = std::make_unique<Mesh>();

    std::vector<float> verts;
    std::vector<unsigned int> idx;

    const char* file = strrchr(objPath, '.');
    if (file) {
        if (strcmp(file, ".obj") == 0) {
            loadObj(objPath, verts, idx);
        }
        else if (strcmp(file, ".fbx") == 0) {
            loadFbxUnanimated(objPath, verts, idx);
        }
        else {
            std::cout << "File not recognized: " << objPath << '\n';
        }
    } else {
        std::cout << "No file type: " << objPath << '\n';
    }

    obj->transform = transform;
    obj->setVertices(verts);
    obj->setIndices(idx);
    obj->setTexture(loadTexture(texPath, pixelated));
    obj->setIndexCount((GLsizei)idx.size());

    return obj;
}

static glm::mat4 ufbxToGlm(const ufbx_matrix& m)
{
    glm::mat4 r(1.0f);
    r[0] = glm::vec4((float)m.cols[0].x, (float)m.cols[0].y, (float)m.cols[0].z, 0.0f);
    r[1] = glm::vec4((float)m.cols[1].x, (float)m.cols[1].y, (float)m.cols[1].z, 0.0f);
    r[2] = glm::vec4((float)m.cols[2].x, (float)m.cols[2].y, (float)m.cols[2].z, 0.0f);
    r[3] = glm::vec4((float)m.cols[3].x, (float)m.cols[3].y, (float)m.cols[3].z, 1.0f);
    return r;
}

static uint32_t pack(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    return (a << 24) | (b << 16) | (c << 8) | d;
}

static float uintBits(uint32_t bits) {
    float f;
    std::memcpy(&f, &bits, sizeof(f));
    return f;
}

// loads .fbx file and looks for animated meshes
static void loadFbxAnimatedMesh(const char* path, std::vector<float>& outVerts,
                                std::vector<unsigned int>& outIndices,
                                Skeleton& outSkel,
                                std::vector<Animation>& outAnims)
{
    ufbx_load_opts opts = {}; // default options
    opts.target_axes = ufbx_axes_right_handed_y_up;
    opts.target_unit_meters = 1.0f;

    ufbx_error error;
    ufbx_scene* scene = ufbx_load_file(path, &opts, &error);
    if (!scene)
    {
        std::cout << "Failed to load scene. Path: " << path 
                  << " Description: " << error.description.data << '\n';
        ufbx_free_scene(scene);
        return;
    }

    // map of bone nodes to their indices
    std::map<ufbx_node*, int> boneMap;
    std::vector<ufbx_node*> boneNodes;

    for (size_t i = 0; i < scene->nodes.count; ++i)
    {
        ufbx_node* node = scene->nodes.data[i];
        ufbx_mesh* mesh = node->mesh;
        if (!mesh) continue;

        ufbx_matrix geomToWorld = node->geometry_to_world;
        ufbx_matrix geomToWorldNormals = ufbx_matrix_for_normals(&geomToWorld);
        std::vector<unsigned int> tri(mesh->max_face_triangles * 3);

        ufbx_skin_deformer* skin = 
            mesh->skin_deformers.count > 0 ? mesh->skin_deformers.data[0] : nullptr;
        std::vector<int> clusterToBone;
        if (skin)
        {
            // loops through bones and sets up skeleton
            clusterToBone.resize(skin->clusters.count);
            for (size_t c = 0; c < skin->clusters.count; ++c)
            {
                ufbx_skin_cluster* cl = skin->clusters.data[c];
                ufbx_node* boneNode = cl->bone_node;
                auto it = boneMap.find(boneNode);
                int bi;
                if (it != boneMap.end()) {
                    bi = it->second;
                } else {
                    bi = (int)outSkel.inverseBind.size();
                    boneMap[boneNode] = bi;
                    
                    // InvBind = GeomToBone * WorldToGeom (gets positions in bone space)
                    outSkel.inverseBind.push_back(ufbxToGlm(cl->geometry_to_bone) * 
                                                  glm::inverse(ufbxToGlm(geomToWorld)));
                    outSkel.parentWorld.push_back(boneNode->parent
                                                  ? ufbxToGlm(boneNode->parent->node_to_world)
                                                  : glm::mat4(1.0f));
                    outSkel.names.push_back(boneNode ? std::string(boneNode->name.data) : "");
                    outSkel.parent.push_back(-1);
                    boneNodes.push_back(boneNode);
                }
                clusterToBone[c] = bi;
            }
        }

        // loops through mesh vertices and sets their perma values
        for (size_t fi = 0; fi < mesh->faces.count; ++fi)
        {
            ufbx_face face = mesh->faces[fi];
            size_t numTris = ufbx_triangulate_face(tri.data(), tri.size(), mesh, face);

            for (size_t i = 0; i < numTris * 3; ++i)
            {
                unsigned int corner = tri[i];
                ufbx_vec3 p = ufbx_get_vertex_vec3(&mesh->vertex_position, corner);
                p = ufbx_transform_position(&geomToWorld, p);
                ufbx_vec2 uv = mesh->vertex_uv.exists
                    ? ufbx_get_vertex_vec2(&mesh->vertex_uv, corner)
                    : ufbx_vec2{0.0f, 0.0f};
                ufbx_vec3 n = mesh->vertex_normal.exists
                    ? ufbx_get_vertex_vec3(&mesh->vertex_normal, corner)
                    : ufbx_vec3{0.0f, 0.0f, 1.0f};
                n = ufbx_transform_direction(&geomToWorldNormals, n);

                uint32_t bi[4] = {0,0,0,0};
                float bw[4] = {0,0,0,0};
                if (skin)
                {
                    uint32_t vert = mesh->vertex_indices.data[corner];
                    ufbx_skin_vertex sv = skin->vertices.data[vert];

                    uint32_t count = sv.num_weights < 4 ? sv.num_weights : 4;
                    float sum = 0.0f;
                    for (uint32_t w = 0; w < count; ++w)
                    {
                        ufbx_skin_weight sw = skin->weights.data[sv.weight_begin + w];
                        bi[w] = (uint32_t)clusterToBone[sw.cluster_index];
                        bw[w] = (float)sw.weight;
                        sum += (float)sw.weight;
                    }
                    if (sum != 1.0f && sum > 0.0f) for (int w = 0; w < 4; ++w) bw[w] /= sum;
                }

                outIndices.push_back((unsigned int)(outVerts.size() / VERTEX_FLOATS));
                outVerts.push_back((float)p.x);
                outVerts.push_back((float)p.y);
                outVerts.push_back((float)p.z);
                outVerts.push_back((float)uv.x);
                outVerts.push_back((float)uv.y);
                outVerts.push_back((float)n.x);
                outVerts.push_back((float)n.y);
                outVerts.push_back((float)n.z);
                outVerts.push_back(0.0f);
                outVerts.push_back(0.0f);
                outVerts.push_back(uintBits(pack(bi[0], bi[1], bi[2], bi[3])));
                outVerts.push_back(bw[0]);
                outVerts.push_back(bw[1]);
                outVerts.push_back(bw[2]);
                outVerts.push_back(bw[3]);
            }
        }
    }

    // loops through bone map and sets each bones parent
    // now that all bones have been processed once already.
    for (auto& bone : boneMap)
    {
        ufbx_node* boneNode = bone.first;
        int bi = bone.second;
        if (boneNode && boneNode->parent)
        {
            auto pit = boneMap.find(boneNode->parent);
            if (pit != boneMap.end()) outSkel.parent[bi] = pit->second;
        }
    }

    // gets pos, rot, and scale of each bone during each frame of each animation.
    for (size_t si = 0; si < scene->anim_stacks.count && !boneNodes.empty(); ++si)
    {
        ufbx_anim_stack* stack = scene->anim_stacks.data[si];
        ufbx_anim* anim = stack->anim;

        float fps = 30.0f;
        double t0 = stack->time_begin;
        double t1 = stack->time_end;
        double dur = t1 - t0;
        int frames = (int)std::ceil(dur * fps) + 1;
        if (frames < 1) frames = 1;

        Animation out;
        out.name = stack->name.data ? std::string(stack->name.data) : "";
        out.fps = fps;
        out.duration = (float)dur;
        out.frameCount = frames;
        out.tracks.resize(boneNodes.size());

        for (int f = 0; f < frames; ++f)
        {
            double t = t0 + (frames > 1 ? dur * (double)f / (double)(frames - 1) : 0.0);
            for (size_t b = 0; b < boneNodes.size(); ++b)
            {
                // get nodes local pos quat and scale for bone each frame
                ufbx_transform lt = ufbx_evaluate_transform(anim, boneNodes[b], t);
                out.tracks[b].pos.push_back(
                    glm::vec3((float)lt.translation.x, (float)lt.translation.y, (float)lt.translation.z)
                );
                out.tracks[b].rot.push_back( // glm quat is (w, x, y, z)
                    glm::quat((float)lt.rotation.w, (float)lt.rotation.x,
                              (float)lt.rotation.y, (float)lt.rotation.z)
                );
                out.tracks[b].scale.push_back(
                    glm::vec3((float)lt.scale.x, (float)lt.scale.y, (float)lt.scale.z)
                );
            }
        }
        std::cout << "Baked clip " << out.name << " frames: " 
                  << out.frameCount << " duration: " << out.duration << std::endl;
        outAnims.push_back(out);
    }
    ufbx_free_scene(scene);
}

// loads .fbx file and looks for non mesh armatures
static void loadFbxAnimatedObj(const char* path,
                              Skeleton& outSkel,
                              std::vector<Animation>& outAnims)
{
    ufbx_load_opts opts = {}; // default options
    opts.target_axes = ufbx_axes_right_handed_y_up;
    opts.target_unit_meters = 1.0f;

    ufbx_error error;
    ufbx_scene* scene = ufbx_load_file(path, &opts, &error);
    if (!scene)
    {
        std::cout << "Failed to load scene. Path: " << path 
                  << " Description: " << error.description.data << '\n';
        ufbx_free_scene(scene);
        return;
    }

    // map of bone nodes to their indices
    std::map<ufbx_node*, int> boneMap;
    std::vector<ufbx_node*> boneNodes;

    for (size_t i = 0; i < scene->nodes.count; ++i)
    {
        ufbx_node* node = scene->nodes.data[i];
        ufbx_bone* bone = node->bone;
        if (!bone) continue;

        auto it = boneMap.find(node);
        int bi;
        if (it != boneMap.end()) {
            bi = it->second;
        } else {
            bi = (int)outSkel.parent.size();
            boneMap[node] = bi;

            outSkel.parentWorld.push_back(node->parent
                                          ? ufbxToGlm(node->parent->node_to_world)
                                          : glm::mat4(1.0f));
            outSkel.names.push_back(node ? std::string(node->name.data) : "");
            outSkel.parent.push_back(-1);
            boneNodes.push_back(node);
        }
    }

    // loops through bone map and sets each bones parent
    // now that all bones have been processed once already.
    for (auto& bone : boneMap)
    {
        ufbx_node* boneNode = bone.first;
        int bi = bone.second;
        if (boneNode && boneNode->parent)
        {
            auto pit = boneMap.find(boneNode->parent);
            if (pit != boneMap.end()) outSkel.parent[bi] = pit->second;
        }
    }

    // gets pos, rot, and scale of each bone during each frame of each animation.
    for (size_t si = 0; si < scene->anim_stacks.count && !boneNodes.empty(); ++si)
    {
        ufbx_anim_stack* stack = scene->anim_stacks.data[si];
        ufbx_anim* anim = stack->anim;

        float fps = 30.0f;
        double t0 = stack->time_begin;
        double t1 = stack->time_end;
        double dur = t1 - t0;
        int frames = (int)std::ceil(dur * fps) + 1;
        if (frames < 1) frames = 1;

        Animation out;
        out.name = stack->name.data ? std::string(stack->name.data) : "";
        out.fps = fps;
        out.duration = (float)dur;
        out.frameCount = frames;
        out.tracks.resize(boneNodes.size());

        for (int f = 0; f < frames; ++f)
        {
            double t = t0 + (frames > 1 ? dur * (double)f / (double)(frames - 1) : 0.0);
            for (size_t b = 0; b < boneNodes.size(); ++b)
            {
                // get nodes local pos quat and scale for bone each frame
                ufbx_transform lt = ufbx_evaluate_transform(anim, boneNodes[b], t);
                out.tracks[b].pos.push_back(
                    glm::vec3((float)lt.translation.x, (float)lt.translation.y, (float)lt.translation.z)
                );
                out.tracks[b].rot.push_back( // glm quat is (w, x, y, z)
                    glm::quat((float)lt.rotation.w, (float)lt.rotation.x,
                              (float)lt.rotation.y, (float)lt.rotation.z)
                );
                out.tracks[b].scale.push_back(
                    glm::vec3((float)lt.scale.x, (float)lt.scale.y, (float)lt.scale.z)
                );
            }
        }
        std::cout << "Baked clip " << out.name << " frames: " 
                  << out.frameCount << " duration: " << out.duration << std::endl;
        outAnims.push_back(out);
    }
    ufbx_free_scene(scene);
}

std::unique_ptr<AnimatedMesh> makeAnimatedMesh(const Transform& transform, const char* objPath,
                              const char* texPath, bool pixelated)
{
    auto obj = std::make_unique<AnimatedMesh>();

    std::vector<float> verts;
    std::vector<unsigned int> idx;
    Skeleton skel;
    std::vector<Animation> anims;

    const char* file = strrchr(objPath, '.');
    if (file) {
        if (strcmp(file, ".fbx") == 0) {
            loadFbxAnimatedMesh(objPath, verts, idx, skel, anims);
        }
        else {
            std::cout << "File type not recognized: " << objPath << '\n';
        }
    } else {
        std::cout << "No file type: " << objPath << '\n';
    }
    
    Rig rig;

    obj->transform = transform;
    obj->setVertices(verts);
    obj->setIndices(idx);
    obj->setTexture(loadTexture(texPath, pixelated));
    obj->setIndexCount((GLsizei)idx.size());

    rig.skeleton = skel;
    rig.animations = anims;

    obj->rig = rig;

    return obj;
}

std::unique_ptr<AnimatedObj> makeAnimatedObj(const Transform& transform, const char* objPath)
{
    auto obj = std::make_unique<AnimatedObj>();

    Skeleton skel;
    std::vector<Animation> anims;

    const char* file = strrchr(objPath, '.');
    if (file) {
        if (strcmp(file, ".fbx") == 0) {
            loadFbxAnimatedObj(objPath, skel, anims);
        }
        else {
            std::cout << "File type not recognized: " << objPath << '\n';
        }
    } else {
        std::cout << "No file type: " << objPath << '\n';
    }
    
    Rig rig;
    obj->transform = transform;
    rig.skeleton = skel;
    rig.animations = anims;

    obj->rig = rig;

    return obj;
}

static std::unique_ptr<Object> buildObject(const json& node, Object* parent)
{
    const std::string name = node.value("name", "Unnamed Object");
    const std::string type = node.value("type", "object");
    const std::string tag  = node.value("tag", "");
    const std::string scriptpath  = node.value("scriptpath", "");
    const std::string pbn = node.value("parentbonename", "");
    const json& t = node.at("transform");

    Transform transform{t.at(0).get<float>(), t.at(1).get<float>(), t.at(2).get<float>(),
                        t.at(3).get<float>(), t.at(4).get<float>(), t.at(5).get<float>(),
                        t.at(6).get<float>(), t.at(7).get<float>(), t.at(8).get<float>()};
    int pbi = -1;
    if (parent && parent->GetRig() && pbn != "")
        pbi = parent->GetRig()->findBoneIndex(pbn);
    
    std::unique_ptr<Object> obj;

    if (type == "object")
    {
        obj = std::make_unique<Object>(transform);
    }
    else if (type == "mesh")
    {
        obj = makeMesh(
            transform,
            node.at("meshpath").get<std::string>().c_str(),
            node.at("texpath").get<std::string>().c_str(),
            node.value("pixelated", false)
        );
    }
    else if (type == "staticmesh")
    {
        obj = makeStaticMesh(
            transform,
            node.at("meshpath").get<std::string>().c_str(),
            node.at("texpath").get<std::string>().c_str(),
            node.value("pixelated", false),
            node.value("collides", false)
        );
    }
    else if (type == "animatedmesh")
    {
        obj = makeAnimatedMesh(
            transform,
            node.at("meshpath").get<std::string>().c_str(),
            node.at("texpath").get<std::string>().c_str(),
            node.value("pixelated", false)
        );
    }
    else if (type == "animatedobj")
    {
        obj = makeAnimatedObj(
            transform,
            node.at("armaturepath").get<std::string>().c_str()
        );
    }
    else if (type == "camera")
    {
        obj = std::make_unique<Camera>(
            transform,
            node.value("fov", 90.0f)
        );
    }
    else if (type == "capsule")
    {
        obj = std::make_unique<Capsule>(
            transform,
            node.value("height", 1.8f),
            node.value("radius", 0.3f)
        );
    }
    else
    {
        std::cout << "buildObject: Error unrecognized object type " << type << '\n';
        return nullptr;
    }
    
    obj->setBoneIndex(pbi); obj->setName(name); obj->setTag(tag);
    if (parent) obj->parent = parent;

    if (scriptpath != "" ) attachScript(obj.get(), scriptpath);

    for (const json& child : node.value("children", json::array()))
    {
        auto c = buildObject(child, obj.get());
        if (c) obj->addChild(std::move(c));
    }

    return obj;
}

static Light buildLight(const json& node)
{
    Light light;
    light.name = node.value("name", "Unnamed Light");
    light.tag  = node.value("tag", "");

    const json& p = node.at("pos");
    const json& c = node.at("color");

    light.pos   = glm::vec3{p.at(0).get<float>(), p.at(1).get<float>(), p.at(2).get<float>()};
    light.color = glm::vec3{c.at(0).get<float>(), c.at(1).get<float>(), c.at(2).get<float>()};

    light.intensity = node.at("intensity").get<float>();
    light.radius    = node.at("radius").get<float>();
    light.falloff   = node.at("falloff").get<float>();

    return light;
}

static std::unique_ptr<UIElement> buildUIElement(const json& node, UIElement* parent)
{
    const std::string name = node.value("name", "Unnamed Object");
    const std::string type = node.value("type", "object");
    const std::string tag  = node.value("tag", "");
    const json& t = node.at("uitransform");

    UITransform transform{t.at(0).get<float>() * SW, t.at(1).get<float>() * SH,
                          t.at(2).get<float>(), t.at(3).get<float>(), t.at(4).get<float>()};

    std::unique_ptr<UIElement> ui;
    
    if (type == "uielement")
    {
        ui = std::make_unique<UIElement>(transform);
    }
    else if (type == "uiimage")
    {
        ui = std::make_unique<UIImage>(
            transform, node.at("imagepath").get<std::string>().c_str()
        );
    }
    else if (type == "uitext")
    {
        const json& c = node.at("color");

        ui = std::make_unique<UIText>(
            transform, node.at("text").get<std::string>().c_str(),
            node.value("size", 32.0f), node.at("fontindex").get<int>(),
            glm::vec3{c.at(0).get<float>(), c.at(1).get<float>(), c.at(2).get<float>()},
            (unsigned char)node.at("anchorX").get<std::string>()[0],
            (unsigned char)node.at("anchorY").get<std::string>()[0]
        );
    }
    else
    {
        std::cout << "buildUI: Error unrecognized ui type " << type << '\n';
        return nullptr;
    }

    ui->setName(name); ui->setTag(tag);
    if (parent) ui->parent = parent;

    for (const json& child : node.value("children", json::array()))
    {
        auto c = buildUIElement(child, ui.get());
        if (c) ui->addChild(std::move(c));
    }

    return ui;
}

void loadScene(const char* path)
{
    std::ifstream file(path);
    if (!file)
    {
        std::cout << "loadScene: Could not find scene: " << path << '\n';
        return;
    }

    try
    {
        json scene;
        file >> scene;

        std::vector<std::unique_ptr<Object>> tempObjs;
        std::vector<Light> tempLights;
        std::vector<Font> tempFonts;
        std::vector<std::unique_ptr<UIElement>> tempUI;

        for (const json& node : scene.value("objects", json::array()))
        {
            auto obj = buildObject(node, nullptr);
            if (obj) tempObjs.push_back(std::move(obj));
        }
        for (const json& node : scene.value("lights", json::array()))
        {
            Light l = buildLight(node);
            tempLights.push_back(l);
        }
        for (const json& node : scene.value("fonts", json::array()))
        {
            Font f = bakeFont(
                node.at("ttfpath").get<std::string>().c_str(),
                node.value("pixelheight", 32.0f)
            );
            tempFonts.push_back(f);
        }
        for (const json& node : scene.value("ui", json::array()))
        {
            auto ui = buildUIElement(node, nullptr);
            if (ui) tempUI.push_back(std::move(ui));
        }

        rootObjs = std::move(tempObjs);
        lights = tempLights;
        for (Font& f : fonts) glDeleteTextures(1, &f.atlas);
        fonts = tempFonts;
        uiRoots = std::move(tempUI);
    }
    catch(const json::exception& e)
    {
        std::cout << "loadScene: " << path << ": " << e.what() << '\n';
    }
}