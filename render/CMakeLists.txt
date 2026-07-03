cmake_minimum_required(VERSION 3.16)
project(render)

find_package(DART REQUIRED COMPONENTS gui collision-bullet utils CONFIG)
find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)
find_package(assimp CONFIG REQUIRED)

set(PYBIND11_FINDPYTHON ON)
find_package(pybind11 CONFIG REQUIRED)

file(GLOB srcs "*.h" "*.cpp")
add_executable(render ${srcs})

target_link_libraries(render
  PRIVATE
    mss
    dart
    dart-gui
    dart-collision-bullet
    dart-utils
    OpenGL::GL
    OpenGL::GLU
    GLUT::GLUT
    assimp::assimp
    pybind11::embed)
