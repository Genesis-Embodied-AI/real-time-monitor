cmake_minimum_required(VERSION 3.28)
project(test_package CXX)

find_package(real-time-monitor REQUIRED)

add_executable(test_package test_package.cpp)
target_link_libraries(test_package PRIVATE real-time-monitor::real-time-monitor)

if(NOT TARGET real-time-monitor::rtm_recorder)
    message(FATAL_ERROR "Expected imported target real-time-monitor::rtm_recorder")
endif()

add_custom_target(show_recorder ALL
    COMMAND ${CMAKE_COMMAND} -E echo
            "rtm_recorder: $<TARGET_FILE:real-time-monitor::rtm_recorder>")
