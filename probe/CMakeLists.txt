set(PROBE_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/io.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/probe.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/serializer.cc)

add_library(rtm_probe ${PROBE_SRCS})
target_include_directories(rtm_probe PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_target_properties(rtm_probe PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

add_subdirectory(generator)
