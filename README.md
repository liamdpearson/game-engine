# OG3D game engine
simple game engine meant to make 3d games that look like they are from the 90s


## Current features:
- Object hierarchy with types: Object, Mesh, Camera, Capsule, StaticMesh, AnimatedMesh, and AnimatedObj
- UI hierarchy with types: UIElement, UIImage, and UIText
- Ability to have multiple cameras and draw from whichever you want
- Basic collisions for Capsules
- Light maps for static objects
- Light grid for dynamic objects containing light value and dominant light direction
- Mesh support from obj and fbx files
- Armature and animation support from fbx files
- Bone parenting support for children of rigged objects
- Scripting with Lua


To DO:
- Add sound with miniaudio
- Add decals
- Add some sort of particle system
- Add editor with ImGui

## How to run yourself(Windows)
### Prerequisites: MinGW/GCC and CMake 3.16 or newer
Make sure gcc, g++, and mingw32-make are on your path.
1. Clone the repo
2. cmake -S . -B build -G "MinGW Makefiles"
3. cmake --build build
