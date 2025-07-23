#include "ast/ast.hpp"
#include "ast/ast_rtti.hpp"
#include "test/test_framework.hpp"
#include "common/logger.hpp"
#include <iostream>

using namespace Mycelium::Testing;
using namespace Mycelium::Scripting::Common;
using namespace Mycelium;

// Test function declarations
void run_lexer_tests();
void run_parser_tests();
void run_parse_result_tests();
void run_pratt_parser_tests();
void run_recursive_parser_tests();
void run_command_generation_tests();
void run_ir_generation_tests();
void run_jit_execution_tests();
void run_integration_tests();

int main() {
    // Initialize logger for test output
    Logger& logger = Logger::get_instance();
    logger.initialize();
    logger.set_console_level(LogLevel::DEBUG);
    logger.set_enabled_categories(LogCategory::TEST | LogCategory::GENERAL | LogCategory::PARSER | LogCategory::AST);
    logger.set_test_mode(true);
    
    logger.test_suite_start("🔬 Mycelium Compiler Test Suite 🔬");
    
    // Initialize RTTI (required for AST operations)
    Mycelium::Scripting::Lang::AstTypeInfo::initialize();
    LOG_INFO("RTTI Initialized. Total types: " + std::to_string(Mycelium::Scripting::Lang::AstNode::sTypeInfo.fullDerivedCount + 1), LogCategory::TEST);
    
    // Clear any previous test results
    TestTracker::instance().clear();
    
    // Run all test suites
    LOG_INFO("🧪 Running Lexer Tests...", LogCategory::TEST);
    run_lexer_tests();
    
    LOG_INFO("🧪 Running Parser Tests...", LogCategory::TEST);
    run_parser_tests();
    
    LOG_INFO("🧪 Running Command Generation Tests...", LogCategory::TEST);
    run_command_generation_tests();
    
    LOG_INFO("🧪 Running IR Generation Tests...", LogCategory::TEST);
    run_ir_generation_tests();
    
    LOG_INFO("🧪 Running JIT Execution Tests...", LogCategory::TEST);
    run_jit_execution_tests();
    
    LOG_INFO("🧪 Running Integration Tests...", LogCategory::TEST);
    run_integration_tests();
    
    // Print comprehensive final summary
    TestTracker::instance().print_final_summary();
    
    // Return appropriate exit code
    return TestTracker::instance().all_passed() ? 0 : 1;
}