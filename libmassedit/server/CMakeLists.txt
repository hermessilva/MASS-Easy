cmake_minimum_required(VERSION 3.20)
project(mass_mcp_server CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(_lib  "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(_vcpkg "${CMAKE_CURRENT_SOURCE_DIR}/../../Deps/vcpkg/installed/x64-windows")

add_executable(mass-mcp
    main.cpp
    ${_lib}/MassModel.cpp ${_lib}/Index.cpp ${_lib}/Kinematics.cpp
    ${_lib}/DofMap.cpp ${_lib}/Batch.cpp ${_lib}/Complete.cpp
    ${_lib}/Query.cpp ${_lib}/Atlas.cpp ${_lib}/Groom.cpp ${_lib}/Mcp.cpp)

target_include_directories(mass-mcp PRIVATE ${_lib} "${_vcpkg}/include")
target_compile_definitions(mass-mcp PRIVATE _WIN32_WINNT=0x0601 BOOST_ASIO_NO_DEPRECATED)
target_link_libraries(mass-mcp PRIVATE "${_vcpkg}/lib/tinyxml2.lib" ws2_32)

add_custom_command(TARGET mass-mcp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_vcpkg}/bin/tinyxml2.dll" "$<TARGET_FILE_DIR:mass-mcp>")
