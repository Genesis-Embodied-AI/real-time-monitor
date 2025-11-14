cmake_minimum_required(VERSION 3.28)

project(real-time-monitor)

# To find conan packages
list(APPEND CMAKE_FIND_ROOT_PATH ${CMAKE_BINARY_DIR})

# Custom CMake modules and macros
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(flags)

# Enable ctest
enable_testing()

add_subdirectory(probe)
add_subdirectory(monitor)
