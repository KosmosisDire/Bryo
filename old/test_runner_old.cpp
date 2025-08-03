#include "ast/ast.hpp"
#include "ast/ast_rtti.hpp"
#include "test/test_framework.hpp"
#include "common/logger.hpp"
#include <iostream>

// Current architecture includes
#include "semantic/type.hpp"
#include "semantic/symbol_table.hpp"
#include "semantic/error_collector.hpp"
#include "codegen/ir_command.hpp"
#include "codegen/ir_builder.hpp"

using namespace Myre;

// Test function declarations
// void run_lexer_tests();
// void run_parser_tests();
// void run_parse_result_tests();
// void run_pratt_parser_tests();
// void run_recursive_parser_tests();
// void run_command_generation_tests();
// void run_ir_generation_tests();
// void run_jit_execution_tests();
// void run_integration_tests();

// New optimal architecture test
int run_optimal_architecture_test();

// Implementation of optimal architecture test
int run_optimal_architecture_test() {
    using namespace Myre;
    
    std::cout << "\n🚀 Running Optimal Architecture Test\n";
    std::cout << "====================================\n";
    
    try {
        // Quick smoke test of core components
        std::cout << "Testing Type System... ";
        
        // Test type creation
        auto i32_type = TypeFactory::i32();
        auto bool_type = TypeFactory::bool_type();
        
        if (!i32_type || !bool_type) {
            std::cout << "❌ FAILED - Type creation failed\n";
            return 1;
        }
        
        // Test struct creation
        std::vector<FieldInfo> fields = {
            FieldInfo("health", i32_type, 0),
            FieldInfo("alive", bool_type, 4)
        };
        
        std::vector<MethodInfo> methods = {
            MethodInfo("getHealth", i32_type),
            MethodInfo("isAlive", bool_type)
        };
        
        auto player_type = TypeFactory::create_struct("Player", fields, methods);
        if (!player_type || player_type->fields().size() != 2) {
            std::cout << "❌ FAILED - Struct creation failed\n";
            return 1;
        }
        
        std::cout << "✅ PASSED\n";
        
        // Test Symbol Registry
        std::cout << "Testing Symbol Registry... ";
        
        SymbolRegistry registry;
        auto new_registry = registry.add_struct_type(player_type);
        
        auto type_symbol = new_registry.lookup("Player");
        if (!type_symbol) {
            std::cout << "❌ FAILED - Type lookup failed\n";
            return 1;
        }
        
        auto method_symbol = new_registry.lookup_member_function("Player", "getHealth");
        if (!method_symbol) {
            std::cout << "❌ FAILED - Method lookup failed\n";
            return 1;
        }
        
        std::cout << "✅ PASSED\n";
        
        // Test Command Stream
        std::cout << "Testing Command Stream... ";
        
        CommandStream stream;
        auto val1 = stream.next_value(i32_type);
        auto val2 = stream.next_value(i32_type);
        auto result = stream.next_value(i32_type);
        
        stream.add_command(CommandFactory::constant_i32(val1, 42));
        stream.add_command(CommandFactory::constant_i32(val2, 24));
        stream.add_command(CommandFactory::add(result, val1, val2));
        
        if (stream.size() != 3) {
            std::cout << "❌ FAILED - Command stream size incorrect\n";
            return 1;
        }
        
        stream.finalize();
        if (!stream.is_finalized()) {
            std::cout << "❌ FAILED - Stream finalization failed\n";
            return 1;
        }
        
        std::cout << "✅ PASSED\n";
        
        // Test IR Builder
        std::cout << "Testing IR Builder... ";
        
        IRBuilder builder("TestModule");
        auto ir_result = builder.build_ir(stream);
        
        if (!ir_result.is_success()) {
            std::cout << "❌ FAILED - IR generation failed: " << ir_result.error().message() << "\n";
            return 1;
        }
        
        std::string ir = ir_result.value().llvm_ir();
        if (ir.empty() || ir.find("add i32") == std::string::npos) {
            std::cout << "❌ FAILED - Generated IR is invalid\n";
            return 1;
        }
        
        std::cout << "✅ PASSED\n";
        
        std::cout << "\n🎉 All Optimal Architecture Tests PASSED!\n";
        std::cout << "\nGenerated LLVM IR Sample:\n";
        std::cout << "-------------------------\n";
        std::cout << ir.substr(0, 300) << "...\n";
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cout << "❌ FAILED - Exception: " << e.what() << "\n";
        return 1;
    }
}

int main() {
    // Initialize logger for test output
    Logger& logger = Logger::get_instance();
    logger.initialize();
    logger.set_console_level(LogLevel::DEBUG);
    logger.set_enabled_categories(LogCategory::TEST | LogCategory::GENERAL | LogCategory::PARSER | LogCategory::AST);
    logger.set_test_mode(true);
    
    logger.test_suite_start("🔬 Mycelium Compiler Test Suite 🔬");
    
    // Initialize RTTI (required for AST operations)
    Myre::AstTypeInfo::initialize();
    LOG_INFO("RTTI Initialized. Total types: " + std::to_string(Myre::AstNode::sTypeInfo.fullDerivedCount + 1), LogCategory::TEST);
    
    // Clear any previous test results
    TestTracker::instance().clear();
    
    // Run all test suites (COMMENTED OUT - TESTING NEW ARCHITECTURE)
    // LOG_INFO("🧪 Running Lexer Tests...", LogCategory::TEST);
    // run_lexer_tests();
    
    // LOG_INFO("🧪 Running Parser Tests...", LogCategory::TEST);
    // run_parser_tests();
    
    // LOG_INFO("🧪 Running Command Generation Tests...", LogCategory::TEST);
    // run_command_generation_tests();
    
    // LOG_INFO("🧪 Running IR Generation Tests...", LogCategory::TEST);
    // run_ir_generation_tests();
    
    // LOG_INFO("🧪 Running JIT Execution Tests...", LogCategory::TEST);
    // run_jit_execution_tests();
    
    // LOG_INFO("🧪 Running Integration Tests...", LogCategory::TEST);
    // run_integration_tests();
    
    // NEW OPTIMAL ARCHITECTURE TEST
    LOG_INFO("🧪 Running Optimal Architecture Tests...", LogCategory::TEST);
    int test_result = run_optimal_architecture_test();
    
    if (test_result == 0) {
        LOG_INFO("✅ Optimal Architecture Tests PASSED", LogCategory::TEST);
    } else {
        LOG_INFO("❌ Optimal Architecture Tests FAILED", LogCategory::TEST);
    }
    
    // Print comprehensive final summary
    // TestTracker::instance().print_final_summary();
    
    // Return appropriate exit code
    return test_result;
}