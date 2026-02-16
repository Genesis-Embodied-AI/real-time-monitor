find_package(argparse REQUIRED)

add_executable(rtm_generator main.cc)
target_link_libraries(rtm_generator PRIVATE rtm argparse::argparse)
