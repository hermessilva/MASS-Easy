cmake_minimum_required(VERSION 3.20)
project(libmassedit_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# libmassedit sources under test (no DART/JSON deps for the Index unit test)
add_executable(test_index
    test_index.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../Index.cpp)

target_include_directories(test_index PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..)   # libmassedit headers (Index.h + MassModel.h)

# Integration test over real data/human.mass — needs nlohmann/json (header-only).
# Point at the repo's vcpkg include if present; skip the target otherwise.
set(_vcpkg_inc "${CMAKE_CURRENT_SOURCE_DIR}/../../Deps/vcpkg/installed/x64-windows/include")
if(EXISTS "${_vcpkg_inc}/nlohmann/json.hpp")
    add_executable(test_model
        test_model.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Index.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../MassModel.cpp)
    target_include_directories(test_model PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        "${_vcpkg_inc}")
endif()

add_executable(test_kinematics
    test_kinematics.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../Index.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../Kinematics.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../DofMap.cpp)
target_include_directories(test_kinematics PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

add_executable(test_batch
    test_batch.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../Index.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../Kinematics.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../Batch.cpp)
target_include_directories(test_batch PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

enable_testing()
add_test(NAME index COMMAND test_index)
add_test(NAME kinematics COMMAND test_kinematics)
add_test(NAME batch COMMAND test_batch)
if(TARGET test_model)
    add_test(NAME model COMMAND test_model
             "${CMAKE_CURRENT_SOURCE_DIR}/../../data/human.mass")
endif()
