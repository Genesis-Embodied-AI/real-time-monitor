set (CMAKE_CXX_STANDARD 23)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS OFF)

set(WARNINGS_FLAGS -Wall -Wextra -pedantic -Wconversion -Wunused -Wshadow -Wnull-dereference)
set(WARNINGS_FLAGS ${WARNINGS_FLAGS} -Wunused-parameter -Wunused-variable -Wuninitialized -Wno-maybe-uninitialized)
set(WARNINGS_FLAGS ${WARNINGS_FLAGS} -Wreturn-type -Wbool-compare -Wlogical-op -Wsequence-point -Wmisleading-indentation)
set(WARNINGS_FLAGS ${WARNINGS_FLAGS} -Wmissing-noreturn -Wduplicated-cond -Wcast-qual -Wcast-align)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(${WARNINGS_FLAGS})
else()
    message(FATAL_ERROR "Unsupported compiler: ${CMAKE_CXX_COMPILER_ID}. Only GCC and Clang are supported: please add your definitions here.")
endif()

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
