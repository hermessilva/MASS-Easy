# libmassedit — shared model-editing library used by both Arena and the (future)
# in-process MCP bridge. Pure C++17 over the mass::Model struct; no DART.
# nlohmann/json (header-only) is used by MassModel .mass IO and comes from vcpkg.
cmake_minimum_required(VERSION 3.20)

add_library(massedit STATIC
    MassModel.cpp
    Index.cpp
    Kinematics.cpp
    DofMap.cpp
    Batch.cpp
    Complete.cpp
    Query.cpp
    Atlas.cpp
    Groom.cpp
    Mcp.cpp)

target_include_directories(massedit PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# nlohmann/json: prefer a real find_package (vcpkg), fall back to include dir.
find_package(nlohmann_json CONFIG QUIET)
if(nlohmann_json_FOUND)
    target_link_libraries(massedit PUBLIC nlohmann_json::nlohmann_json)
endif()

# tinyxml2 for the OpenSim .osim atlas parser.
find_package(tinyxml2 CONFIG QUIET)
if(tinyxml2_FOUND)
    target_link_libraries(massedit PUBLIC tinyxml2::tinyxml2)
endif()

set_target_properties(massedit PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)

# Unit/integration tests (standalone: cmake -S libmassedit/test -B build && ctest).
# Not added here to keep the main build lean; see libmassedit/test/CMakeLists.txt.
