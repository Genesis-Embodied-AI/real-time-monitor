find_package(Threads REQUIRED)

add_executable(rtm_test
    test_main.cc
    integration_test.cc
    blackbox_test.cc
)
target_link_libraries(rtm_test PRIVATE rtm Threads::Threads)

add_test(NAME rtm_test COMMAND rtm_test)
set_tests_properties(rtm_test PROPERTIES TIMEOUT 120)
