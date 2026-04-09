cmake_minimum_required(VERSION 3.22)

project(rcore_c_port C ASM)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/RCoreTarget.cmake)

add_subdirectory(lib/console)
add_subdirectory(lib/linker)
add_subdirectory(lib/kernel-context-local)

set(RCORE_CHAPTER "" CACHE STRING "chapter to build, for example ch1")
set(RCORE_ARTIFACT_DIR "${CMAKE_BINARY_DIR}/artifacts" CACHE PATH "artifact output directory")

option(RCORE_FEATURE_USER "Enable user-space support" OFF)
option(RCORE_FEATURE_KERNEL "Enable kernel dispatch support" ON)
option(RCORE_FEATURE_FOREIGN "Enable foreign portal support" OFF)
option(RCORE_FEATURE_PROC "Enable process relation support" OFF)
option(RCORE_FEATURE_THREAD "Enable thread relation support" OFF)
option(RCORE_FEATURE_COOP "Enable cooperative scheduling mode" OFF)

file(MAKE_DIRECTORY "${RCORE_ARTIFACT_DIR}")

if(RCORE_CHAPTER STREQUAL "")
  message(FATAL_ERROR "RCORE_CHAPTER must be set, for example -DRCORE_CHAPTER=ch1")
endif()

if(RCORE_CHAPTER STREQUAL "ch1")
  add_subdirectory(chapters/ch1)
elseif(RCORE_CHAPTER STREQUAL "ch2")
  add_subdirectory(chapters/ch2)
elseif(RCORE_CHAPTER STREQUAL "ch3")
  add_subdirectory(chapters/ch3)
elseif(RCORE_CHAPTER STREQUAL "ch4")
  add_subdirectory(chapters/ch4)
else()
  message(FATAL_ERROR "C port chapter '${RCORE_CHAPTER}' has not been implemented yet")
endif()
