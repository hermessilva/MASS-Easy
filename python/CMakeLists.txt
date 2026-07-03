cmake_minimum_required(VERSION 3.16)
project(pymss)

set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR})

find_package(OpenMP)
find_package(DART REQUIRED COMPONENTS collision-bullet utils CONFIG)

set(PYBIND11_FINDPYTHON ON)
find_package(pybind11 CONFIG REQUIRED)

file(GLOB srcs "*.h" "*.cpp")

pybind11_add_module(pymss ${srcs})
target_link_libraries(pymss
  PRIVATE
    mss
    dart
    dart-collision-bullet
    dart-utils)

if(OpenMP_CXX_FOUND)
  target_link_libraries(pymss PRIVATE OpenMP::OpenMP_CXX)
endif()
