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

# Python bindings
if (SKBUILD)
    set(LIB_SRCS
        ${CMAKE_SOURCE_DIR}/py_bindings/src/rtm_py.cc
        ${CMAKE_SOURCE_DIR}/lib/py_bindings/src/lib_py.cc)


    find_package(Python
        REQUIRED COMPONENTS Interpreter Development.Module
        OPTIONAL_COMPONENTS Development.SABIModule)
    execute_process(
        COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir
        OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE nanobind_ROOT)
        find_package(nanobind CONFIG REQUIRED)

    nanobind_add_module(real_time_monitor ${LIB_SRCS} NB_STATIC STABLE_ABI)
    target_link_libraries(real_time_monitor PRIVATE rtm)
    target_include_directories(real_time_monitor PRIVATE ${CMAKE_SOURCE_DIR}/lib/py_bindings/include)
    target_compile_options(real_time_monitor PRIVATE -Wno-shadow -Wno-cast-qual -Wno-pedantic)

    install(TARGETS real_time_monitor DESTINATION .)
endif()
