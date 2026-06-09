cmake_minimum_required(VERSION 3.24)

project(GabbroOcean LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glfw
    URL https://github.com/glfw/glfw/archive/refs/tags/3.4.zip
)

FetchContent_Declare(
    glm
    URL https://github.com/g-truc/glm/archive/refs/tags/1.0.1.zip
)

FetchContent_MakeAvailable(glfw glm)

add_library(glad STATIC
    src/glad.c
)

target_include_directories(glad PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_executable(GabbroOcean
    src/Camera.cpp
    src/Camera.h
    src/FftOcean.cpp
    src/FftOcean.h
    src/Mesh.cpp
    src/Mesh.h
    src/Ocean.cpp
    src/Ocean.h
    src/Shader.cpp
    src/Shader.h
    src/main.cpp
)

target_include_directories(GabbroOcean PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(GabbroOcean PRIVATE
    GABBRO_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(GabbroOcean PRIVATE
    glad
    glfw
    glm::glm
    opengl32
)

if(MSVC)
    target_compile_options(GabbroOcean PRIVATE /W4 /permissive-)
else()
    target_compile_options(GabbroOcean PRIVATE -Wall -Wextra -Wpedantic)
endif()
