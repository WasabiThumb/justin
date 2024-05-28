find_package(Git)
if (GIT_FOUND)
    execute_process(
            COMMAND git rev-parse --abbrev-ref HEAD
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE BRANCH
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
            COMMAND git rev-parse --short HEAD
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_COMMIT
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if (GIT_COMMIT STREQUAL "")
        set(GIT_COMMIT "unknown")
    endif()
else()
    get_filename_component(BRANCH ${CMAKE_CURRENT_SOURCE_DIR} NAME)
    set(GIT_COMMIT "unknown")
endif()
if (BRANCH STREQUAL "master" AND CMAKE_BUILD_TYPE STREQUAL "Release")
    set(FULL_VERSION "${BASE_VERSION}")
else()
    set(FULL_VERSION "${BASE_VERSION}-git-${GIT_COMMIT}")
endif()
