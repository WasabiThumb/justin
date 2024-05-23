cmake_minimum_required(VERSION 3.20)

find_package(Git)
if (GIT_FOUND)
    execute_process(
            COMMAND git rev-parse --abbrev-ref HEAD
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE BRANCH
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
else()
    get_filename_component(BRANCH ${CMAKE_CURRENT_SOURCE_DIR} NAME)
endif()
message("${BRANCH}")

project(justin VERSION 0.0.1 LANGUAGES C)

add_compile_definitions(JUSTIN_VERSION="${PROJECT_VERSION}")
set(CMAKE_C_STANDARD 99)
if (CMAKE_BUILD_TYPE STREQUAL  "Release")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
endif()

file(GLOB_RECURSE JUSTIN_SOURCES RELATIVE ${CMAKE_SOURCE_DIR} "src/*.c")
add_executable(justin main.c ${JUSTIN_SOURCES})
target_link_libraries(justin git2 curl)
target_compile_options(justin PRIVATE -Wall)
