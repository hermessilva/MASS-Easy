cmake_minimum_required(VERSION 3.16)
project(arena)

find_package(glfw3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(imguizmo CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(tinyxml CONFIG REQUIRED)
find_package(DART REQUIRED COMPONENTS collision-bullet utils CONFIG)
find_package(boost_asio CONFIG REQUIRED)

file(GLOB editor_srcs "src/*.cpp" "src/*.h")

add_executable(arena ${editor_srcs})
target_compile_features(arena PRIVATE cxx_std_17)

target_include_directories(arena PRIVATE ${CMAKE_SOURCE_DIR}/core)

target_link_libraries(arena PRIVATE
  glfw
  glad::glad
  imgui::imgui
  imguizmo::imguizmo
  nlohmann_json::nlohmann_json
  unofficial-tinyxml::unofficial-tinyxml
  mss
  dart
  dart-collision-bullet
  dart-utils
  Boost::asio)

if(WIN32)
  target_link_libraries(arena PRIVATE opengl32 comdlg32 shell32 ws2_32 mswsock)
endif()

set_target_properties(arena PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/editor)
