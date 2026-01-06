add_executable(rtm_recorder main.cc)
target_link_libraries(rtm_recorder PRIVATE rtm)

if (SKBUILD)
    install(TARGETS rtm_recorder DESTINATION ${SKBUILD_SCRIPTS_DIR})
endif()
