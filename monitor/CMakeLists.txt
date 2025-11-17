find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(imgui REQUIRED)

set(IMPLOT_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/implot/implot.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/implot/implot_items.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/backend/imgui_impl_glfw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/backend/imgui_impl_opengl3.cpp)
add_library(implot STATIC ${IMPLOT_SRCS})
target_include_directories(implot PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/implot
    ${CMAKE_CURRENT_SOURCE_DIR}/backend)
target_link_libraries(implot PUBLIC imgui::imgui)
target_compile_options(implot PRIVATE -Wno-conversion -Wno-cast-qual)

set(MONITOR_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/plot.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/serie.cc
    )
add_executable(monitor ${MONITOR_SRCS})
target_link_libraries(monitor PRIVATE implot glfw OpenGL::GL rtm)
