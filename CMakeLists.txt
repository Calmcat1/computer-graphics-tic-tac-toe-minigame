cmake_minimum_required(VERSION 3.14...3.30)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

project(TicTacToeProject CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Fetch FreeGLUT 3.6.0 (Static)
FetchContent_Declare(
    freeglut
    GIT_REPOSITORY https://github.com/FreeGLUTProject/freeglut.git
    GIT_TAG        v3.6.0
)
set(FREEGLUT_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(FREEGLUT_BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(FREEGLUT_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(freeglut)

# Define the executable target and its source file
add_executable(TicTacToe_OpenGL main.cpp)

# Configure include paths, libraries, and definitions for that exact target
target_include_directories(TicTacToe_OpenGL PRIVATE "${freeglut_SOURCE_DIR}/include")
target_link_libraries(TicTacToe_OpenGL PRIVATE freeglut_static opengl32 glu32)
target_compile_definitions(TicTacToe_OpenGL PRIVATE FREEGLUT_STATIC)