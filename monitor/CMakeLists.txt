find_package(imgui REQUIRED)
find_package(argparse REQUIRED)
find_package(glfw3 REQUIRED)

if (LINUX)
    set(OS_LIB_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/backend/imgui_impl_glfw.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/backend/imgui_impl_opengl3.cpp
    )

    set(OS_MAIN_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/main.cc)

    find_package(OpenGL REQUIRED)
    set(OS_SPECIFIC_LIBRARIES OpenGL::GL)
endif()

if (APPLE)
    set(OS_LIB_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/backend/imgui_impl_glfw.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/backend/imgui_impl_metal.mm
    )

    set(OS_MAIN_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/main.mm)

    find_library(METAL_FRAMEWORK Metal REQUIRED)
    find_library(METALKIT_FRAMEWORK MetalKit REQUIRED)
    find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
    set(OS_SPECIFIC_LIBRARIES
        ${METAL_FRAMEWORK}
        ${METALKIT_FRAMEWORK}
        ${QUARTZCORE_FRAMEWORK}
    )
endif()

set(IMPLOT_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/implot/implot.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/implot/implot_items.cpp
    )
add_library(implot STATIC ${IMPLOT_SRCS} ${OS_LIB_SOURCES})
target_include_directories(implot PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/implot
    ${CMAKE_CURRENT_SOURCE_DIR}/backend)
target_link_libraries(implot PUBLIC imgui::imgui glfw ${OS_SPECIFIC_LIBRARIES})
target_compile_options(implot PRIVATE -Wno-conversion -Wno-cast-qual)

set(MONITOR_SRCS
    ${OS_MAIN_SOURCE}
    ${CMAKE_CURRENT_SOURCE_DIR}/main_window.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/plot.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/serie.cc
    )
add_executable(rtm_gui ${MONITOR_SRCS})
target_link_libraries(rtm_gui PRIVATE implot rtm argparse::argparse glfw)

if (SKBUILD)
    install(TARGETS rtm_gui DESTINATION ${SKBUILD_SCRIPTS_DIR})
endif()
