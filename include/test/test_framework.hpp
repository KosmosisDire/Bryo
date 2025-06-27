#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include "common/logger.hpp"

namespace Mycelium::Testing {

// ANSI color codes
namespace Colors {
    const std::string RESET = "\033[0m";
    const std::string RED = "\033[31m";
    const std::string GREEN = "\033[32m";
    const std::string YELLOW = "\033[33m";
    const std::string BLUE = "\033[34m";
    const std::string MAGENTA = "\033[35m";
    const std::string CYAN = "\033[36m";
    const std::string WHITE = "\033[37m";
    const std::string BOLD = "\033[1m";
    const std::string DIM = "\033[2m";
}

class TestResult {
public:
    bool passed;
    std::string message;
    std::string test_name;
    std::string suite_name;
    
    TestResult(bool p, const std::string& m = "") : passed(p), message(m) {}
};

// Global test tracking
class TestTracker {
private:
    static TestTracker* instance_;
    std::vector<TestResult> all_results_;
    
public:
    static TestTracker& instance() {
        if (!instance_) {
            instance_ = new TestTracker();
        }
        return *instance_;
    }
    
    void add_result(const TestResult& result) {
        all_results_.push_back(result);
    }
    
    void print_final_summary() {
        int total_tests = all_results_.size();
        int passed_tests = 0;
        int failed_tests = 0;
        
        std::vector<TestResult> failures;
        
        for (const auto& result : all_results_) {
            if (result.passed) {
                passed_tests++;
            } else {
                failed_tests++;
                failures.push_back(result);
            }
        }

        
        LOG_BLANK();
        LOG_HEADER("FINAL TEST SUMMARY", LogCategory::TEST);
        
        if (failed_tests == 0) {
            LOG_INFO("🎉 ALL TESTS PASSED! 🎉", LogCategory::TEST);
        } else {
            LOG_ERROR("❌ SOME TESTS FAILED", LogCategory::TEST);
        }

        LOG_BLANK();
        LOG_INFO("Overall Results:", LogCategory::TEST);
        LOG_INFO("  Total Tests: " + std::to_string(total_tests), LogCategory::TEST);
        LOG_INFO("  Passed: " + std::to_string(passed_tests), LogCategory::TEST);
        LOG_INFO("  Failed: " + std::to_string(failed_tests), LogCategory::TEST);
        
        if (!failures.empty()) {
            LOG_BLANK();
            LOG_ERROR("FAILED TESTS:", LogCategory::TEST);
            for (const auto& failure : failures) {
                LOG_ERROR("  ❌ " + failure.suite_name + " → " + failure.test_name, LogCategory::TEST);
                if (!failure.message.empty()) {
                    LOG_ERROR("     " + failure.message, LogCategory::TEST);
                }
            }
        }

    }
    
    void clear() {
        all_results_.clear();
    }
    
    bool all_passed() const {
        for (const auto& result : all_results_) {
            if (!result.passed) return false;
        }
        return true;
    }
};

// Static instance definition
inline TestTracker* TestTracker::instance_ = nullptr;

class TestSuite {
private:
    std::string suite_name_;
    std::vector<std::function<TestResult()>> tests_;
    std::vector<std::string> test_names_;
    
public:
    TestSuite(const std::string& name) : suite_name_(name) {}
    
    void add_test(const std::string& name, std::function<TestResult()> test) {
        test_names_.push_back(name);
        tests_.push_back(test);
    }
    
    bool run_all() {

        LOG_TEST_SUITE_START(suite_name_);

        int passed = 0;
        int total = tests_.size();
        
        for (size_t i = 0; i < tests_.size(); ++i) {
            try {
                TestResult result = tests_[i]();
                result.test_name = test_names_[i];
                result.suite_name = suite_name_;

                LOG_TEST_RESULT(test_names_[i], result.passed, result.message);

                if (result.passed) {
                    passed++;
                }
                
                // Add to global tracker
                TestTracker::instance().add_result(result);
                
            } catch (const std::exception& e) {
                std::string error_msg = std::string("Exception: ") + e.what();
                LOG_TEST_RESULT(test_names_[i], false, error_msg);

                // Create error result
                TestResult error_result(false, error_msg);
                error_result.test_name = test_names_[i];
                error_result.suite_name = suite_name_;
                TestTracker::instance().add_result(error_result);
            }
        }

        LOG_TEST_SUITE_END(suite_name_, passed, total);

        return passed == total;
    }
};

// Helper macros for assertions
#define ASSERT_TRUE(condition, message) \
    do { \
        if (!(condition)) { \
            return TestResult(false, message); \
        } \
    } while(0)

#define ASSERT_FALSE(condition, message) \
    do { \
        if (condition) { \
            return TestResult(false, message); \
        } \
    } while(0)

#define ASSERT_EQ(expected, actual, message) \
    do { \
        if ((expected) != (actual)) { \
            return TestResult(false, std::string(message) + " - Expected: " + std::to_string(expected) + ", Got: " + std::to_string(actual)); \
        } \
    } while(0)

#define ASSERT_STR_EQ(expected, actual, message) \
    do { \
        if ((expected) != (actual)) { \
            return TestResult(false, std::string(message) + " - Expected: '" + std::string(expected) + "', Got: '" + std::string(actual) + "'"); \
        } \
    } while(0)

#define ASSERT_NOT_EMPTY(container, message) \
    do { \
        if ((container).empty()) { \
            return TestResult(false, message); \
        } \
    } while(0)

} // namespace Mycelium::Testing