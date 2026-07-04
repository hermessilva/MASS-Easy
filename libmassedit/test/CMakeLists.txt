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

    add_executable(test_query
        test_query.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Index.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Query.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../MassModel.cpp)
    target_include_directories(test_query PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        "${_vcpkg_inc}")

    add_executable(test_complete
        test_complete.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Index.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Kinematics.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Complete.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../MassModel.cpp)
    target_include_directories(test_complete PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        "${_vcpkg_inc}")

    add_executable(test_groom
        test_groom.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Groom.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Kinematics.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Index.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../MassModel.cpp)
    target_include_directories(test_groom PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/.. "${_vcpkg_inc}")

    # Atlas needs tinyxml2 (linked). Only build if the vcpkg lib is present.
    set(_vcpkg_root "${CMAKE_CURRENT_SOURCE_DIR}/../../Deps/vcpkg/installed/x64-windows")
    if(EXISTS "${_vcpkg_root}/lib/tinyxml2.lib")
        add_executable(test_atlas
            test_atlas.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/../Index.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/../Atlas.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/../MassModel.cpp)
        target_include_directories(test_atlas PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/.. "${_vcpkg_inc}")
        target_link_libraries(test_atlas PRIVATE "${_vcpkg_root}/lib/tinyxml2.lib")
        add_custom_command(TARGET test_atlas POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_vcpkg_root}/bin/tinyxml2.dll" "$<TARGET_FILE_DIR:test_atlas>")

        add_executable(test_mcp
            test_mcp.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/../Mcp.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/../Query.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/../Batch.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/../Kinematics.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/../Complete.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/../Atlas.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/../Index.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/../MassModel.cpp)
        target_include_directories(test_mcp PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/.. "${_vcpkg_inc}")
        target_link_libraries(test_mcp PRIVATE "${_vcpkg_root}/lib/tinyxml2.lib")
        add_custom_command(TARGET test_mcp POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_vcpkg_root}/bin/tinyxml2.dll" "$<TARGET_FILE_DIR:test_mcp>")
    endif()
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
    add_test(NAME query COMMAND test_query
             "${CMAKE_CURRENT_SOURCE_DIR}/../../data/human.mass")
    add_test(NAME complete COMMAND test_complete)
    add_test(NAME groom COMMAND test_groom)
    if(TARGET test_atlas)
        add_test(NAME atlas COMMAND test_atlas)
    endif()
    if(TARGET test_mcp)
        add_test(NAME mcp COMMAND test_mcp
                 "${CMAKE_CURRENT_SOURCE_DIR}/../../data/human.mass")
    endif()
endif()
