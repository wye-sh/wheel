################################################################################
# Author:    Alexander Hållenius                                               #
# Copyright: 2025                                                              #
# File:      /CMakeLists.txt                                                   #
################################################################################

set(BUILD_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build)
set(BINARY_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
set(LIBRARY_DIRECTORY ${BINARY_DIRECTORY})
set(INCLUDE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include)

################################################################################
##  project  ###################################################################
################################################################################

cmake_minimum_required(VERSION 3.27)
project(WHEEL VERSION 2.0.2 LANGUAGES CXX C)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${LIBRARY_DIRECTORY})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${LIBRARY_DIRECTORY})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BINARY_DIRECTORY})

################################################################################
##  dependencies  ##############################################################
################################################################################

if(NOT TARGET aport)
  include(FetchContent)
  FetchContent_Declare(
    aport
    GIT_REPOSITORY https://github.com/wye-sh/aport
    GIT_TAG v1.1.3)
  FetchContent_MakeAvailable(aport)
endif()

################################################################################
##  target  ####################################################################
################################################################################

add_library(wheel INTERFACE)
target_include_directories(wheel INTERFACE ${INCLUDE_DIRECTORY})
target_link_libraries(wheel INTERFACE aport)

################################################################################
##  options  ###################################################################
################################################################################

option(RTTI_POINTER_COMPARISON_MODE "When retrieving from tree, will use radix mode." ON)
if (RTTI_POINTER_COMPARISON_MODE)
  target_compile_definitions(wheel
    INTERFACE
      RTTI_POINTER_COMPARISON_MODE)
endif()

################################################################################
##  testing  ###################################################################
################################################################################

if(${CMAKE_PROJECT_NAME} STREQUAL ${PROJECT_NAME})
  # We are running from this project, meaning it is debug
  add_subdirectory(test)
endif()
