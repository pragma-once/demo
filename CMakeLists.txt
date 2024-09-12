cmake_minimum_required(VERSION 3.12)

project(Game)

set(CMAKE_CXX_STANDARD 20)

if (("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang") OR ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"))

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

endif()

add_subdirectory(glad)
add_subdirectory(glm)
add_subdirectory(sdl2)
add_subdirectory(demo)
