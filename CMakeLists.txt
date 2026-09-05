cmake_minimum_required(VERSION 3.16)
project(game_engine LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# --- third party: built without our warning flags ---
add_library(third_party STATIC
    third_party/glad/src/glad.c
    third_party/xatlas/xatlas.cpp
    third_party/ufbx/ufbx.c
)
target_include_directories(third_party SYSTEM PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party
    ${CMAKE_SOURCE_DIR}/third_party/glad/include
    ${CMAKE_SOURCE_DIR}/third_party/glfw/include
)

# --- lua ---
add_library(lua STATIC third_party/lua/onelua.c)
target_compile_definitions(lua PRIVATE MAKE_LIB)
target_include_directories(lua PUBLIC ${CMAKE_SOURCE_DIR}/third_party/lua)


# --- prebuilt GLFW ---
add_library(glfw3 STATIC IMPORTED)
set_target_properties(glfw3 PROPERTIES
    IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/third_party/glfw/lib/libglfw3.a
)

# --- engine ---
add_library(engine STATIC
    src/collisions/collisions.cpp
    src/graphics/graphics.cpp
    src/input/input.cpp
    src/lighting/lighting.cpp
    src/load/load.cpp
    src/scripts/scripts.cpp
    src/ui/ui.cpp
    
)
target_include_directories(engine PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_compile_options(engine PRIVATE -Wall -Wextra)
target_link_libraries(engine PUBLIC third_party glfw3 lua opengl32 gdi32)

# --- game ---
add_executable(app src/main.cpp)
target_compile_options(app PRIVATE -Wall -Wextra)
target_link_libraries(app PRIVATE engine)
set_target_properties(app PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_NAME $<IF:$<CONFIG:Debug>,debug,$<IF:$<CONFIG:RelWithDebInfo>,reldbg,release>>
)