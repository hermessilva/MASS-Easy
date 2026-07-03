cmake_minimum_required(VERSION 3.16)
project(mss)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING
      "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
endif(NOT CMAKE_BUILD_TYPE)

# DART 6.15 requires C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT MSVC)
  add_compile_options(-fPIC)
  add_compile_options(-Wdeprecated-declarations)
  add_compile_options(-DHAVE_CSTDDEF)
else()
  # MSVC: keep min/max macros disabled, enable proper preprocessor / M_PI
  add_compile_definitions(NOMINMAX _USE_MATH_DEFINES _CRT_SECURE_NO_WARNINGS)
  add_compile_options(/bigobj /permissive-)
endif()

add_definitions(-DMASS_ROOT_DIR="${CMAKE_HOME_DIRECTORY}")
set(CMAKE_MODULE_PATH "${CMAKE_HOME_DIRECTORY}/cmake")

add_subdirectory( core )
add_subdirectory( render )
add_subdirectory( python )
add_subdirectory( Arena )
