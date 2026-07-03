cmake_minimum_required(VERSION 3.16)
project(mss)

set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR})

find_package(DART REQUIRED COMPONENTS collision-bullet utils CONFIG)
find_package(tinyxml CONFIG REQUIRED)

file(GLOB srcs "*.h" "*.cpp")

add_library(mss STATIC ${srcs})
target_include_directories(mss PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mss
  PUBLIC
    dart
    dart-collision-bullet
    dart-utils
    unofficial-tinyxml::unofficial-tinyxml)
