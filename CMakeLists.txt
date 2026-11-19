cmake_minimum_required(VERSION 3.16)

project(ntmesh LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -D wins, then the environment variable, then a sibling checkout.
if(NOT NTV_ROOT AND DEFINED ENV{NTV_ROOT} AND NOT "$ENV{NTV_ROOT}" STREQUAL "")
  set(NTV_ROOT "$ENV{NTV_ROOT}")
endif()
if(NOT NTV_ROOT AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../ntv/ntv/src/nvec3.h")
  get_filename_component(NTV_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../ntv/ntv" ABSOLUTE)
endif()
if(NOT NTV_ROOT)
  message(FATAL_ERROR "NTV_ROOT is not set. Pass -DNTV_ROOT= or export NTV_ROOT. NTV_ROOT is the ntv repo that contains src/nvec3.h.")
endif()
if(NOT IS_ABSOLUTE "${NTV_ROOT}")
  get_filename_component(NTV_ROOT "${NTV_ROOT}" ABSOLUTE)
endif()
if(NOT EXISTS "${NTV_ROOT}/src/nvec3.h")
  message(FATAL_ERROR "NTV_ROOT does not contain src/nvec3.h: ${NTV_ROOT}")
endif()

add_executable(ntmesh_tests test/main.C test/test_ntmesh.C)
target_include_directories(ntmesh_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/test
  ${NTV_ROOT}/src
)
target_compile_options(ntmesh_tests PRIVATE -Wall -Wextra -Wpedantic)

message(STATUS "NTV_ROOT: ${NTV_ROOT}")
