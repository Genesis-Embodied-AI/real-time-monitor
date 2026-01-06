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
