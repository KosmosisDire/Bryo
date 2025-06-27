#pragma once
#include "test/test_framework.hpp"

namespace Mycelium::Testing {

// Convenience macro to create a test suite and run it in one go
#define CREATE_TEST_SUITE(suite_name) \
    TestSuite suite(#suite_name)

#define ADD_TEST(test_function) \
    suite.add_test(#test_function, test_function)

#define RUN_SUITE() \
    suite.run_all()

// Example usage in test files:
/*
void run_my_new_tests() {
    CREATE_TEST_SUITE(My New Feature Tests);
    
    ADD_TEST(test_feature_one);
    ADD_TEST(test_feature_two);
    ADD_TEST(test_edge_cases);
    
    RUN_SUITE();
}

TestResult test_feature_one() {
    // Your test code here
    ASSERT_TRUE(some_condition, "Feature one should work");
    return TestResult(true);
}
*/

} // namespace Mycelium::Testing