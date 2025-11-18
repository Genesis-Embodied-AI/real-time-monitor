set(LIB_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/error.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/io.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/probe.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/serializer.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/time_wrapper.cc)

add_library(rtm ${LIB_SRCS})
target_include_directories(rtm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
                               PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/rtm)
set_target_properties(rtm PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
