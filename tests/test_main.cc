#include "test_helpers.h"

bool test_file_sink();
bool test_local_socket();
bool test_tcp();
bool test_empty_data();
bool test_truncated_data();
bool test_corrupted_data();

bool test_blackbox_no_trigger();
bool test_blackbox_trigger();
bool test_blackbox_multiple_triggers();
bool test_blackbox_retrigger_extends();
bool test_blackbox_backward_compat();
bool test_blackbox_file_header();


int main()
{
    TestCase tests[] =
    {
        {"file_sink",                  test_file_sink},
        {"local_socket",               test_local_socket},
        {"tcp",                        test_tcp},
        {"empty_data",                 test_empty_data},
        {"truncated_data",             test_truncated_data},
        {"corrupted_data",             test_corrupted_data},
        {"blackbox_no_trigger",        test_blackbox_no_trigger},
        {"blackbox_trigger",           test_blackbox_trigger},
        {"blackbox_multiple_triggers", test_blackbox_multiple_triggers},
        {"blackbox_retrigger_extends", test_blackbox_retrigger_extends},
        {"blackbox_backward_compat",   test_blackbox_backward_compat},
        {"blackbox_file_header",       test_blackbox_file_header},
    };

    return run_tests(tests, std::size(tests));
}
