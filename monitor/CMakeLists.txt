find_package(imgui REQUIRED)

if (LINUX)
    set(OS_LIB_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/backend/imgui_impl_glfw.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/backend/imgui_impl_opengl3.cpp
    )

    set(OS_MAIN_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/main.cc)

    find_package(glfw3 REQUIRED)
    find_package(OpenGL REQUIRED)
    set(OS_SPECIFIC_LIBRARIES glfw OpenGL::GL)
endif()

if (APPLE)
    set(OS_LIB_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/backend/imgui_impl_metal.mm
        ${CMAKE_CURRENT_SOURCE_DIR}/backend/imgui_impl_osx.mm
    )

    set(OS_MAIN_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/main.mm)

    find_library(METAL_FRAMEWORK Metal REQUIRED)
    find_library(METALKIT_FRAMEWORK MetalKit REQUIRED)
    find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
    find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
    find_library(COREGRAPHICS_FRAMEWORK CoreGraphics REQUIRED)
    find_library(GAMECONTROLLER_FRAMEWORK GameController REQUIRED)
    set(OS_SPECIFIC_LIBRARIES
        ${METAL_FRAMEWORK}
        ${METALKIT_FRAMEWORK}
        ${COCOA_FRAMEWORK}
        ${QUARTZCORE_FRAMEWORK}
        ${COREGRAPHICS_FRAMEWORK}
        ${GAMECONTROLLER_FRAMEWORK}
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
target_link_libraries(implot PUBLIC imgui::imgui ${OS_SPECIFIC_LIBRARIES})
target_compile_options(implot PRIVATE -Wno-conversion -Wno-cast-qual)

set(MONITOR_SRCS
    ${OS_MAIN_SOURCE}
    ${CMAKE_CURRENT_SOURCE_DIR}/main_window.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/plot.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/serie.cc
    )
add_executable(monitor ${MONITOR_SRCS})
target_link_libraries(monitor PRIVATE implot rtm)
