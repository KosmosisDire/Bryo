#pragma once

#include <string>
#include <vector>

namespace Fern {

struct TestResult {
    std::string test_name;
    bool passed;
    bool crashed;
    bool compile_failed;
    float return_value;
    std::string error_message;

    TestResult(const std::string& name)
        : test_name(name), passed(false), crashed(false),
          compile_failed(false), return_value(0.0f) {}
};

class TestRunner {
public:
    TestRunner();

    // Run all tests in the specified directory
    std::vector<TestResult> run_all_tests(const std::string& test_dir);

    // Print summary of test results
    void print_summary(const std::vector<TestResult>& results);

private:
    TestResult run_single_test(const std::string& test_file);
};

} // namespace Fern
