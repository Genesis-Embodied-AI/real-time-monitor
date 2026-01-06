cmake_minimum_required(VERSION 3.28)

project(real_time_monitor)

if (SKBUILD AND NOT CMAKE_CI_BUILD)
    execute_process(
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/setup_build.sh" "${CMAKE_BINARY_DIR}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()

# To find conan packages
list(APPEND CMAKE_FIND_ROOT_PATH ${CMAKE_BINARY_DIR})

# Custom CMake modules and macros
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(flags)

# Enable ctest
enable_testing()

option(BUILD_TOOLS      "Build tools" ON)
option(BUILD_MONITOR    "Build monitor" ON)

add_subdirectory(lib)

if (BUILD_MONITOR)
    add_subdirectory(monitor)
endif()

if (BUILD_TOOLS)
    add_subdirectory(tools)
endif()

add_subdirectory(py_bindings)
