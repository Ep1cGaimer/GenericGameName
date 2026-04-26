cmake_minimum_required(VERSION 3.15)
project(HorrorGame VERSION 1.0)

# Use C++ 17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# --- FETCH DEPENDENCIES ---
include(FetchContent)

# 1. Fetch GLFW (Window & Input)
FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.8
)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# 2. Fetch GLM (Mathematics)
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG master
)
FetchContent_MakeAvailable(glm)

# 3. Fetch Assimp (3D Models)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
# Build Assimp as a static library
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG v5.3.1
)
FetchContent_MakeAvailable(assimp)

# --- SETUP OUR EXECUTABLE ---
add_executable(HorrorGame 
    src/main.cpp 
    src/gl.c
    src/Maze.cpp
    src/MazeRenderer.cpp
    src/Collision.cpp
    src/PostProcessor.cpp
    src/TextureLoader.cpp
    src/Model.cpp
    src/AssetManager.cpp
    src/ParticleSystem.cpp
    src/Overlay.cpp
)

# Include directories
target_include_directories(HorrorGame PRIVATE include src)

# Link GLFW, GLM, Assimp, and winmm (for Windows Audio) to our game
target_link_libraries(HorrorGame PRIVATE glfw glm assimp winmm)
