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

set(MIXED_BUNNY "${CMAKE_CURRENT_SOURCE_DIR}/samplegeo/mixed_manifold_bunny.geo")
set(LIGHT_BUNNY "${CMAKE_CURRENT_SOURCE_DIR}/samplegeo/light_bunny.geo")
if(NOT EXISTS "${MIXED_BUNNY}")
  message(FATAL_ERROR "missing samplegeo/mixed_manifold_bunny.geo")
endif()
if(NOT EXISTS "${LIGHT_BUNNY}")
  message(FATAL_ERROR "missing samplegeo/light_bunny.geo")
endif()

function(nt_mesh_target name)
  add_executable(${name} ${ARGN})
  target_include_directories(${name} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/test
    ${NTV_ROOT}/src
  )
  target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic)
endfunction()

nt_mesh_target(ntmesh_tests test/main.C test/test_ntmesh.C)
nt_mesh_target(bunny_grid samples/grid/src/main.C)
target_compile_definitions(bunny_grid PRIVATE
  BUNNY_GEO="${MIXED_BUNNY}"
  OUT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/samples/grid"
)
nt_mesh_target(tet_edges samples/tets/src/main.C)
target_compile_definitions(tet_edges PRIVATE
  LIGHT_BUNNY="${LIGHT_BUNNY}"
  OUT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/samples/tets"
)

message(STATUS "NTV_ROOT: ${NTV_ROOT}")
