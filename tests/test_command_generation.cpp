#include "test/test_framework.hpp"
#include "test/test_helpers.hpp"
#include "codegen/codegen.hpp"
#include "semantic/symbol_table.hpp"
#include "ast/ast_rtti.hpp"

using namespace Mycelium::Testing;
using namespace Mycelium::Scripting::Lang;

TestResult test_literal_generation() {
    // Initialize RTTI
    AstTypeInfo::initialize();
    
    AstAllocator allocator;
    TestASTBuilder builder(allocator);
    SymbolTable symbol_table;
    
    // Create a simple integer literal
    auto literal = builder.create_int_literal(42);
    
    // Create a simple function with return statement
    auto return_stmt = builder.create_return_statement(literal);
    auto block = builder.create_block_statement({return_stmt});
    auto func = builder.create_simple_function("test", "i32", block);
    auto unit = builder.create_compilation_unit({func});
    
    // Generate commands
    CodeGenerator generator(symbol_table);
    auto commands = generator.generate_code(unit);
    
    // Verify we got some commands
    ASSERT_NOT_EMPTY(commands, "Should generate commands for literal");
    
    // Check for specific command types
    bool found_const = false;
    bool found_ret = false;
    bool found_func_begin = false;
    bool found_func_end = false;
    
    for (const auto& cmd : commands) {
        if (cmd.op == Op::Const) found_const = true;
        if (cmd.op == Op::Ret) found_ret = true;
        if (cmd.op == Op::FunctionBegin) found_func_begin = true;
        if (cmd.op == Op::FunctionEnd) found_func_end = true;
    }
    
    ASSERT_TRUE(found_const, "Should generate Const command for literal");
    ASSERT_TRUE(found_ret, "Should generate Ret command for return statement");
    ASSERT_TRUE(found_func_begin, "Should generate FunctionBegin command");
    ASSERT_TRUE(found_func_end, "Should generate FunctionEnd command");
    
    return TestResult(true);
}

TestResult test_binary_expression_generation() {
    AstAllocator allocator;
    TestASTBuilder builder(allocator);
    SymbolTable symbol_table;
    
    // Create 5 + 3
    auto left = builder.create_int_literal(5);
    auto right = builder.create_int_literal(3);
    auto binary = builder.create_binary_expression(left, BinaryOperatorKind::Add, right);
    
    auto return_stmt = builder.create_return_statement(binary);
    auto block = builder.create_block_statement({return_stmt});
    auto func = builder.create_simple_function("add_test", "i32", block);
    auto unit = builder.create_compilation_unit({func});
    
    // Generate commands
    CodeGenerator generator(symbol_table);
    auto commands = generator.generate_code(unit);
    
    ASSERT_NOT_EMPTY(commands, "Should generate commands for binary expression");
    
    // Check for Add command
    bool found_add = false;
    for (const auto& cmd : commands) {
        if (cmd.op == Op::Add) found_add = true;
    }
    
    ASSERT_TRUE(found_add, "Should generate Add command for binary expression");
    
    return TestResult(true);
}

TestResult test_void_function_generation() {
    AstAllocator allocator;
    TestASTBuilder builder(allocator);
    SymbolTable symbol_table;
    
    // Create empty void function
    auto block = builder.create_block_statement({});
    auto func = builder.create_simple_function("void_test", "void", block);
    auto unit = builder.create_compilation_unit({func});
    
    // Generate commands
    CodeGenerator generator(symbol_table);
    auto commands = generator.generate_code(unit);
    
    ASSERT_NOT_EMPTY(commands, "Should generate commands for void function");
    
    // Check for RetVoid command
    bool found_ret_void = false;
    for (const auto& cmd : commands) {
        if (cmd.op == Op::RetVoid) found_ret_void = true;
    }
    
    ASSERT_TRUE(found_ret_void, "Should generate RetVoid command for void function");
    
    return TestResult(true);
}

void run_command_generation_tests() {
    TestSuite suite("Command Generation Tests");
    
    suite.add_test("Literal Generation", test_literal_generation);
    suite.add_test("Binary Expression Generation", test_binary_expression_generation);
    suite.add_test("Void Function Generation", test_void_function_generation);
    
    suite.run_all();
}