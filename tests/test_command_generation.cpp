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

TestResult test_function_call_return_type() {
    AstAllocator allocator;
    TestASTBuilder builder(allocator);
    SymbolTable symbol_table;
    
    // Create two functions: one returning i32, one returning void
    auto return_42 = builder.create_return_statement(builder.create_int_literal(42));
    auto func1_body = builder.create_block_statement({return_42});
    auto func1 = builder.create_simple_function("get_number", "i32", func1_body);
    
    auto func2_body = builder.create_block_statement({});
    auto func2 = builder.create_simple_function("do_nothing", "void", func2_body);
    
    // Create a main function that calls both
    auto call1 = builder.create_call_expression("get_number");
    auto call2 = builder.create_call_expression("do_nothing");
    auto return_call1 = builder.create_return_statement(call1);
    auto expr_stmt = allocator.alloc<ExpressionStatementNode>();
    expr_stmt->expression = call2;
    
    auto main_body = builder.create_block_statement({expr_stmt, return_call1});
    auto main_func = builder.create_simple_function("main", "i32", main_body);
    
    auto unit = builder.create_compilation_unit({func1, func2, main_func});
    
    // Build symbol table
    build_symbol_table(symbol_table, unit);
    
    // Generate commands
    CodeGenerator generator(symbol_table);
    auto commands = generator.generate_code(unit);
    
    ASSERT_NOT_EMPTY(commands, "Should generate commands for function calls");
    
    // Check that we have Call commands
    int call_count = 0;
    for (const auto& cmd : commands) {
        if (cmd.op == Op::Call) {
            call_count++;
            // Verify that the call has the correct return type
            // The first call should be to do_nothing (void)
            // The second call should be to get_number (i32)
            if (call_count == 1) {
                ASSERT_EQ(cmd.result.type.kind, IRType::Kind::Void, 
                         "First call (do_nothing) should have void return type");
            } else if (call_count == 2) {
                ASSERT_EQ(cmd.result.type.kind, IRType::Kind::I32, 
                         "Second call (get_number) should have i32 return type");
            }
        }
    }
    
    ASSERT_EQ(call_count, 2, "Should generate exactly 2 Call commands");
    
    return TestResult(true, "Function call return type test successful");
}

void run_command_generation_tests() {
    TestSuite suite("Command Generation Tests");
    
    suite.add_test("Literal Generation", test_literal_generation);
    suite.add_test("Binary Expression Generation", test_binary_expression_generation);
    suite.add_test("Void Function Generation", test_void_function_generation);
    suite.add_test("Function Call Return Type", test_function_call_return_type);
    
    suite.run_all();
}