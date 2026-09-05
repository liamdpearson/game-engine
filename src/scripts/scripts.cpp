#include "scripts.h"

#include "../input/input.h"
#include <fstream>

sol::state lua;

std::vector<ScriptInstance> scripts;

void attachScript(Object* obj, std::string path)
{
    ScriptInstance si;
    si.obj = obj;
    si.path = path;
    scripts.push_back(si);
}

void initScripting()
{
    lua.open_libraries(sol::lib::base, sol::lib::math);

    // tell lua about the data types it can use
    lua.new_usertype<glm::vec3>("Vec3",
        sol::constructors<glm::vec3(float), glm::vec3(float, float, float)>(),
        "x", &glm::vec3::x,
        "y", &glm::vec3::y,
        "z", &glm::vec3::z,
        "normalize", [](glm::vec3& v) { // mutates v
            float len = glm::length(v);
            if (len > 1e-6f) v /= len;
        },
        "normalized", [](const glm::vec3& v) { // returns new v
            float len = glm::length(v);
            return len > 1e-6f ? v / len : glm::vec3(0.0f);
        },
        "length", [](const glm::vec3& v) { return glm::length(v); },
        "dot",    [](const glm::vec3& a, const glm::vec3& b) { return glm::dot(a, b); },
        "cross",  [](const glm::vec3& a, const glm::vec3& b) { return glm::cross(a, b); },
        sol::meta_function::addition,    [](const glm::vec3& a, const glm::vec3 b) { return a + b; },
        sol::meta_function::subtraction, [](const glm::vec3& a, const glm::vec3 b) { return a - b; },
        sol::meta_function::multiplication, sol::overload(
            [](const glm::vec3& v, float s) { return v * s; },
            [](float s, const glm::vec3& v) { return v * s; }
        ),
        sol::meta_function::unary_minus, [](const glm::vec3& v) { return -v; },
        sol::meta_function::to_string,   [](const glm::vec3& v) {
            return "(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " + std::to_string(v.z) + ")";
        }
    );
    lua.new_usertype<Transform>("Transform",
        "x", &Transform::x,
        "y", &Transform::y,
        "z", &Transform::z,
        "yaw", &Transform::yaw,
        "pitch", &Transform::pitch,
        "roll", &Transform::roll,
        "scaleX", &Transform::scaleX,
        "scaleY", &Transform::scaleY,
        "scaleZ", &Transform::scaleZ,
        "matrix", &Transform::matrix
    );
    lua.new_usertype<Rig>("Rig",
        "currentAnim", &Rig::currentAnim,
        "animSpeed", &Rig::animSpeed,
        "setAnim", sol::overload(
            [](Rig& r, int i)                 { r.setAnim(i); },
            [](Rig& r, int i, float b)        { r.setAnim(i, b); },
            [](Rig& r, int i, float b, int n) { r.setAnim(i, b, n); }
        )
    );
    lua.new_usertype<Object>("Object",
        "name", sol::property(&Object::getName, &Object::setName),
        "tag", sol::property(&Object::getTag, &Object::setTag),
        "draw", &Object::draw,
        "transform", &Object::transform
    );
    lua.new_usertype<Mesh>("Mesh",
        sol::base_classes, sol::bases<Object>()
    );
    lua.new_usertype<StaticMesh>("StaticMesh",
        sol::base_classes, sol::bases<Object, Mesh>()
    );
    lua.new_usertype<AnimatedMesh>("AnimatedMesh",
        "rig", &AnimatedMesh::rig,
        sol::base_classes, sol::bases<Object, Mesh>()
    );
    lua.new_usertype<AnimatedObj>("AnimatedObj",
        "rig", &AnimatedObj::rig,
        sol::base_classes, sol::bases<Object>()
    );
    lua.new_usertype<Camera>("Camera",
        sol::base_classes, sol::bases<Object>()
    );
    lua.new_usertype<Capsule>("Capsule",
        "height", &Capsule::height,
        "radius", &Capsule::radius,
        "velocity", &Capsule::velocity,
        "grounded", &Capsule::grounded,
        sol::base_classes, sol::bases<Object>()
    );
    lua.create_named_table("input",
        "keyPressed",   &keyPressed,
        "keyHeld",      &keyHeld,
        "keyReleased",  &keyReleased,
        "mousePressed",  &mouseButtonPressed,
        "mouseHeld",     &mouseButtonHeld,
        "mouseReleased", &mouseButtonReleased,
        "mouseDX", &mouseDX,
        "mouseDY", &mouseDY
    );
    lua.create_named_table("key",
        "W", GLFW_KEY_W,
        "A", GLFW_KEY_A,
        "S", GLFW_KEY_S,
        "D", GLFW_KEY_D,
        "R", GLFW_KEY_R,
        "SPACE",  GLFW_KEY_SPACE,
        "LSHIFT", GLFW_KEY_LEFT_SHIFT,
        "ESCAPE", GLFW_KEY_ESCAPE
    );
    lua.create_named_table("mouse",
        "LEFT",  GLFW_MOUSE_BUTTON_LEFT,
        "RIGHT", GLFW_MOUSE_BUTTON_RIGHT
    );

    for (ScriptInstance& si : scripts)
    {
        std::ifstream file(si.path);
        if (!file) {
            std::cout << "Error opening lua file" << '\n';
            continue;
        }

        std::string source{
            std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>()
        };

        sol::environment env(lua, sol::create, lua.globals());
        si.env = env;

        if (Capsule* capsule = dynamic_cast<Capsule*>(si.obj))
            si.env["self"] = capsule;
        else if (Camera* camera = dynamic_cast<Camera*>(si.obj))
            si.env["self"] = camera;
        else if (AnimatedObj* animatedObj = dynamic_cast<AnimatedObj*>(si.obj))
            si.env["self"] = animatedObj;
        else if (AnimatedMesh* animatedMesh = dynamic_cast<AnimatedMesh*>(si.obj))
            si.env["self"] = animatedMesh;
        else if (StaticMesh* staticMesh = dynamic_cast<StaticMesh*>(si.obj))
            si.env["self"] = staticMesh;
        else if (Mesh* mesh = dynamic_cast<Mesh*>(si.obj))
            si.env["self"] = mesh;
        else
            si.env["self"] = si.obj;

        sol::load_result loaded = lua.load(source, si.path);
        if (!loaded.valid()) {
            sol::error err = loaded;
            std::cout << "Script load error: " << err.what() << '\n';
            continue;
        }
        sol::protected_function body = loaded;
        si.env.set_on(body);

        auto result = body();

        if (!result.valid()) {
            sol::error err = result;
            std::cout << "Script body error: " << err.what() << '\n';
            continue;
        }

        si.startFn = si.env["start"];
        if (!si.startFn.valid())
            std::cout << "Start function invalid" << '\n';
        si.updateFn = si.env["update"];
        if (!si.updateFn.valid())
            std::cout << "Update function invalid" << '\n';
    }
}

void ScriptInstance::Start() const
{
    if (!startFn.valid()) return;

    auto result = startFn();

    if (!result.valid()) {
        sol::error err = result;
        std::cout << "Start function error: " << err.what() << '\n';
    }
}

void ScriptInstance::Update(float deltaTime) const
{
    if (!updateFn.valid()) return;
 
    auto result = updateFn(deltaTime);

    if (!result.valid()) {
        sol::error err = result;
        std::cout << "Update function error: " << err.what() << '\n';
    }
}