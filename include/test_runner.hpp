#pragma once

#define NOMINMAX  // Prevent Windows min/max macros

#include <string>
#include <vector>
#include <filesystem>
#include "script_execution_engine.hpp"

namespace Mycelium::Testing {

    using namespace Mycelium::Execution;

    struct TestSuiteConfig {
        std::vector<std::string> test_patterns = {"tests/*.sp"};
        std::string output_directory = "tests/build";
        LogLevel log_level = LogLevel::ERR;
        bool verbose_output = false;
        bool save_individual_results = true;
        bool save_summary = true;
        
        TestSuiteConfig() = default;
        
        static TestSuiteConfig silent() {
            TestSuiteConfig config;
            config.log_level = LogLevel::NONE;
            config.verbose_output = false;
            config.save_individual_results = false;
            config.save_summary = false;
            return config;
        }
        
        static TestSuiteConfig verbose() {
            TestSuiteConfig config;
            config.log_level = LogLevel::DEBUG;
            config.verbose_output = true;
            return config;
        }
    };

    struct TestSuiteResult {
        std::string suite_name;
        std::vector<ExecutionResult> test_results;
        std::chrono::milliseconds total_time{0};
        
        TestSuiteResult(const std::string& name) : suite_name(name) {}
        
        int get_passed_count() const {
            return std::count_if(test_results.begin(), test_results.end(), 
                [](const ExecutionResult& r) { return r.succeeded; });
        }
        
        int get_failed_count() const {
            return test_results.size() - get_passed_count();
        }
        
        bool all_passed() const {
            return get_failed_count() == 0;
        }
        
        std::vector<ExecutionResult> get_failed_tests() const {
            std::vector<ExecutionResult> failed;
            for (const auto& result : test_results) {
                if (!result.succeeded) failed.push_back(result);
            }
            return failed;
        }
    };

    class UnifiedTestRunner {
    private:
        TestSuiteConfig config_;
        ScriptExecutionEngine engine_;
        
        std::vector<std::string> find_test_files();
        void print_test_progress(const ExecutionResult& result);
        
    public:
        explicit UnifiedTestRunner(const TestSuiteConfig& config = TestSuiteConfig{});
        
        // Main test execution
        TestSuiteResult run_test_suite();
        ExecutionResult run_single_test(const std::string& file_path);
        
        // Output methods
        void print_test_suite_results(const TestSuiteResult& suite);
        void save_test_suite_results(const TestSuiteResult& suite);
        
        // Configuration
        void set_config(const TestSuiteConfig& config);
        const TestSuiteConfig& get_config() const { return config_; }
        
        // Utility methods
        static std::string format_test_summary(const TestSuiteResult& suite);
        static void print_failed_test_details(const std::vector<ExecutionResult>& failed_tests);
    };

} // namespace Mycelium::Testing