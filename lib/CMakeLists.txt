set(LIB_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/data.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/error.cc

    ${CMAKE_CURRENT_SOURCE_DIR}/src/io/io.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/io/posix/file.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/io/posix/socket.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/io/posix/local_socket.cc

    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/probe.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/serializer.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/os/time.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/os/posix/time.cc
    )

add_library(rtm ${LIB_SRCS})
target_include_directories(rtm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
                               PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/rtm)
set_target_properties(rtm PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
