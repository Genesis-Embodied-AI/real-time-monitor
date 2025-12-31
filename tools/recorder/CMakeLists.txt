add_executable(recorder main.cc)
target_link_libraries(recorder PRIVATE rtm)

target_compile_options(recorder PRIVATE -fsanitize=address)
target_link_options(recorder PRIVATE -fsanitize=address)
