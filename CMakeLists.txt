cmake_minimum_required(VERSION 3.20)

set(BASE_VERSION "0.0.1")
include("${CMAKE_SOURCE_DIR}/cmake/version.cmake")

project(justin VERSION ${BASE_VERSION} LANGUAGES C)

add_compile_definitions(JUSTIN_VERSION="${FULL_VERSION}")

set(CMAKE_C_STANDARD 99)
if (CMAKE_BUILD_TYPE STREQUAL  "Release")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
endif()

file(GLOB_RECURSE JUSTIN_SOURCES RELATIVE ${CMAKE_SOURCE_DIR} "src/*.c")
add_executable(justin main.c ${JUSTIN_SOURCES})
target_link_libraries(justin git2 curl alpm json-c)
target_compile_options(justin PRIVATE -Wall -fmacro-prefix-map=${CMAKE_SOURCE_DIR}/= -msse4.2)
