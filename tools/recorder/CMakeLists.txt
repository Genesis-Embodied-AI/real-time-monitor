find_package(argparse REQUIRED)

add_executable(rtm_recorder main.cc)
target_link_libraries(rtm_recorder PRIVATE rtm argparse::argparse)

if (SKBUILD)
    install(TARGETS rtm_recorder DESTINATION ${SKBUILD_SCRIPTS_DIR})
endif()
