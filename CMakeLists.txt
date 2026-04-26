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

# --- SETUP OUR EXECUTABLE ---
add_executable(HorrorGame 
    src/main.cpp 
    src/gl.c
    src/Maze.cpp
    src/MazeRenderer.cpp
    src/Collision.cpp
    src/PostProcessor.cpp
)

# Include directories
target_include_directories(HorrorGame PRIVATE include src)

# Link GLFW and GLM to our game
target_link_libraries(HorrorGame PRIVATE glfw glm)
