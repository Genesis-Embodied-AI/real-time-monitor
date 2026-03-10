find_package(Threads REQUIRED)

add_executable(rtm_integration_test integration_test.cc)
target_link_libraries(rtm_integration_test PRIVATE rtm Threads::Threads)

add_test(NAME integration_test COMMAND rtm_integration_test)
