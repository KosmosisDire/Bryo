#define NOMINMAX  // Prevent Windows min/max macros
#include "test_runner.hpp"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <algorithm>

namespace Mycelium::Testing {

UnifiedTestRunner::UnifiedTestRunner(const TestSuiteConfig& config) 
    : config_(config) {
    
    // Configure execution engine based on test config
    ExecutionConfig exec_config;
    exec_config.log_level = config_.log_level;
    exec_config.save_ir_to_file = true;
    exec_config.ir_output_directory = config_.output_directory;
    exec_config.capture_console_output = true;
    exec_config.enable_timing = true;
    exec_config.capture_debug_info = (config_.log_level >= LogLevel::DEBUG);
    
    engine_.set_config(exec_config);
    
    // Ensure output directory exists
    std::filesystem::create_directories(config_.output_directory);
}

void UnifiedTestRunner::set_config(const TestSuiteConfig& config) {
    config_ = config;
    
    // Reconfigure execution engine
    ExecutionConfig exec_config = engine_.get_config();
    exec_config.log_level = config_.log_level;
    exec_config.ir_output_directory = config_.output_directory;
    exec_config.capture_debug_info = (config_.log_level >= LogLevel::DEBUG);
    
    engine_.set_config(exec_config);
}

std::vector<std::string> UnifiedTestRunner::find_test_files() {
    std::vector<std::string> test_files;
    
    for (const auto& pattern : config_.test_patterns) {
        std::filesystem::path pattern_path(pattern);
        std::filesystem::path directory = pattern_path.parent_path();
        std::string filename_pattern = pattern_path.filename().string();
        
        if (std::filesystem::exists(directory)) {
            for (const auto& entry : std::filesystem::directory_iterator(directory)) {
                if (entry.is_regular_file() && entry.path().extension() == ".sp") {
                    test_files.push_back(entry.path().string());
                }
            }
        }
    }
    
    // Sort for consistent ordering
    std::sort(test_files.begin(), test_files.end());
    return test_files;
}

void UnifiedTestRunner::print_test_progress(const ExecutionResult& result) {
    if (!config_.verbose_output) return;
    
    std::cout << "Running test: " << result.script_name;
    if (!result.script_path.empty()) {
        std::cout << " (" << result.script_path << ")";
    }
    std::cout << std::endl;
}

ExecutionResult UnifiedTestRunner::run_single_test(const std::string& file_path) {
    ExecutionResult result = engine_.execute_file(file_path);
    print_test_progress(result);
    return result;
}

TestSuiteResult UnifiedTestRunner::run_test_suite() {
    TestSuiteResult suite("Sharpie Test Suite");
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::vector<std::string> test_files = find_test_files();
    
    if (config_.verbose_output) {
        std::cout << "Found " << test_files.size() << " test file(s)" << std::endl;
        std::cout << std::endl;
    }
    
    // Execute all tests using the execution engine
    suite.test_results = engine_.execute_multiple_files(test_files);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    suite.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    return suite;
}

void UnifiedTestRunner::print_test_suite_results(const TestSuiteResult& suite) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "Test Suite Results: " << suite.suite_name << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    for (const auto& result : suite.test_results) {
        ScriptExecutionEngine::print_execution_result(result, config_.verbose_output);
    }
    
    std::cout << std::string(60, '-') << std::endl;
    std::cout << "Total: " << suite.test_results.size() << " tests, ";
    std::cout << "\033[32m" << suite.get_passed_count() << " passed\033[0m, ";
    std::cout << "\033[31m" << suite.get_failed_count() << " failed\033[0m" << std::endl;
    std::cout << "Total time: " << suite.total_time.count() << "ms" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    if (config_.verbose_output && suite.get_failed_count() > 0) {
        std::cout << "\nFailed Test Details:" << std::endl;
        print_failed_test_details(suite.get_failed_tests());
    }
}

void UnifiedTestRunner::save_test_suite_results(const TestSuiteResult& suite) {
    if (!config_.save_summary && !config_.save_individual_results) return;
    
    // Save summary
    if (config_.save_summary) {
        std::string summary_file = config_.output_directory + "/test_summary.txt";
        std::ofstream file(summary_file);
        if (file.is_open()) {
            file << "Test Suite Summary: " << suite.suite_name << std::endl;
            file << "Generated: " << std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count() << std::endl;
            file << std::string(60, '=') << std::endl;
            
            for (const auto& result : suite.test_results) {
                file << result.script_name << ": " << (result.succeeded ? "PASSED" : "FAILED");
                file << " (" << result.timing.total_time.count() << "ms)";
                if (result.output.exit_code.has_value()) {
                    file << " [exit: " << result.output.exit_code.value() << "]";
                }
                file << std::endl;
                
                if (!result.succeeded && !result.errors.empty()) {
                    for (const auto& error : result.errors) {
                        file << "  Error: " << error.message;
                        if (!error.location.empty()) {
                            file << " at " << error.location;
                        }
                        file << std::endl;
                    }
                }
            }
            
            file << std::string(60, '-') << std::endl;
            file << "Summary: " << suite.test_results.size() << " tests, ";
            file << suite.get_passed_count() << " passed, ";
            file << suite.get_failed_count() << " failed" << std::endl;
            file << "Total time: " << suite.total_time.count() << "ms" << std::endl;
            
            file.close();
            
            if (config_.verbose_output) {
                std::cout << "Test summary saved to: " << summary_file << std::endl;
            }
        }
    }
    
    // Save individual results
    if (config_.save_individual_results) {
        for (const auto& result : suite.test_results) {
            std::string result_file = config_.output_directory + "/" + result.script_name + "_result.txt";
            ScriptExecutionEngine::save_execution_result(result, result_file);
        }
        
        if (config_.verbose_output) {
            std::cout << "Individual test results saved to: " << config_.output_directory << std::endl;
        }
    }
}

std::string UnifiedTestRunner::format_test_summary(const TestSuiteResult& suite) {
    std::ostringstream summary;
    
    summary << "Test Suite: " << suite.suite_name << std::endl;
    summary << "Tests: " << suite.test_results.size() << std::endl;
    summary << "Passed: " << suite.get_passed_count() << std::endl;
    summary << "Failed: " << suite.get_failed_count() << std::endl;
    summary << "Total Time: " << suite.total_time.count() << "ms" << std::endl;
    summary << "Success Rate: " << std::fixed << std::setprecision(1) 
            << (suite.test_results.empty() ? 0.0 : 
                (static_cast<double>(suite.get_passed_count()) / suite.test_results.size() * 100.0))
            << "%" << std::endl;
    
    return summary.str();
}

void UnifiedTestRunner::print_failed_test_details(const std::vector<ExecutionResult>& failed_tests) {
    for (const auto& test : failed_tests) {
        std::cout << "\n\033[31m" << test.script_name << " FAILED\033[0m:" << std::endl;
        
        for (const auto& error : test.errors) {
            std::cout << "  • " << error.message;
            if (!error.location.empty()) {
                std::cout << " at " << error.location;
            }
            std::cout << std::endl;
        }
        
        if (!test.warnings.empty()) {
            std::cout << "  Warnings:" << std::endl;
            for (const auto& warning : test.warnings) {
                std::cout << "    - " << warning << std::endl;
            }
        }
        
        std::cout << "  Completed phase: ";
        switch (test.completed_phase) {
            case ExecutionPhase::INPUT_READING: std::cout << "Input Reading"; break;
            case ExecutionPhase::PARSING: std::cout << "Parsing"; break;
            case ExecutionPhase::SEMANTIC_ANALYSIS: std::cout << "Semantic Analysis"; break;
            case ExecutionPhase::COMPILATION: std::cout << "Compilation"; break;
            case ExecutionPhase::JIT_EXECUTION: std::cout << "JIT Execution"; break;
            case ExecutionPhase::COMPLETED: std::cout << "Completed"; break;
        }
        std::cout << std::endl;
    }
}

} // namespace Mycelium::Testing