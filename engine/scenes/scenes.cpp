#include "scenes.h"

#include <nlohmann/json.hpp>
#include "../load/load.h"
#include "../ui/ui.h"
#include "../scripts/scripts.h"
#include "../lighting/lighting.h"
#include "../graphics/graphics.h"
#include "../collisions/collisions.h"
#include <fstream>
#include <vector>
#include <string>
#include <iostream>

using json = nlohmann::json;

static std::unique_ptr<Object> buildObject(const json& node, Object* parent)
{
    const std::string name = node.value("name", "Unnamed Object");
    const std::string type = node.value("type", "object");
    const std::string tag  = node.value("tag", "");
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

        if (node.value("isCurrent", false))
        {
            if (Camera* cam = dynamic_cast<Camera*>(obj.get()))
                currentCam = cam;
        }
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

    for (const std::string& path : node.value("scripts", json::array()))
        attachScript(obj.get(), path);

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

        ambient = scene.value("ambient", 0.0f);
        lightmapResScalar = scene.value("lightmapResScalar", 0.01f);

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
        // global scripts operate the same way as regular scripts just with no self object
        for (const std::string& path : scene.value("globalscripts", json::array()))
            attachScript(nullptr, path);

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

void swapScene(const char* path)
{
    uiRoots.clear();
    resetScripting();
    rootObjs.clear();
    currentCam = nullptr;
    ambient = 0.0f;
    lightmapResScalar = 0.01;

    loadScene(path);
    loadScripts();

    // this clears occluders and light grid
    bakeSceneLighting();

    // this clears colliders
    collectSceneColliders();

    for (std::unique_ptr<Object>& obj : rootObjs) obj->Upload();
    for (std::unique_ptr<UIElement>& ui : uiRoots) ui->UploadUI();

    startScripts();
}