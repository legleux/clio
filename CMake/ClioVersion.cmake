execute_process(COMMAND git rev-parse --short HEAD
                OUTPUT_VARIABLE GIT_REV
                ERROR_QUIET)

if(NOT (BRANCH MATCHES "master" OR BRANCH MATCHES "release/*" OR BRANCH MATCHES "main")) # for develop and any other branch name YYYYMMDDHMS-<branch>-<git-ref>
    execute_process(COMMAND date +%Y%m%d%H%M%S OUTPUT_VARIABLE DATE OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

if(CMAKE_BUILD_TYPE MATCHES Debug)
  set(DEBUG "+DEBUG")
  set_target_properties(${exe_name} PROPERTIES OUTPUT_NAME  ${exe_name}-debug)
endif()

if ("${GIT_REV}" STREQUAL "")
    set(VERSION "${DATE}-development")
    return()
else()
    execute_process(
        COMMAND git describe --exact-match --tags
        OUTPUT_VARIABLE TAG ERROR_QUIET)
    execute_process(
        COMMAND git rev-parse --abbrev-ref HEAD
        OUTPUT_VARIABLE BRANCH)

    string(STRIP "${GIT_REV}" GIT_REV)
    string(STRIP "${TAG}" TAG)
    string(STRIP "${BRANCH}" BRANCH)
endif()

if(NOT (BRANCH MATCHES "master" OR BRANCH MATCHES "release/*" OR BRANCH MATCHES "main"))
    set(VERSION "${DATE}-${BRANCH}-${GIT_REV}${DEBUG}")
else()
    set(VERSION "${TAG}${DEBUG}")
endif()

set(clio_version ${VERSION})

configure_file(CMake/Build.cpp.in ${CMAKE_SOURCE_DIR}/src/main/impl/Build.cpp)
