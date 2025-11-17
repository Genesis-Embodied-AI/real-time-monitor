add_executable(generator main.cc)
target_link_libraries(generator PRIVATE rtm)

target_compile_options(generator PRIVATE -fsanitize=address)
target_link_options(generator PRIVATE -fsanitize=address)
