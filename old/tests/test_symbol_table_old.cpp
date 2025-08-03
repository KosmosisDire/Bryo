#include <iostream>
#include <cassert>
#include "common/test_framework.hpp"
#include "semantic/symbol_table.hpp"
#include "parser/parser.hpp"
#include "parser/lexer.hpp"

using namespace Myre;

// Helper function to create a parser from source code
std::unique_ptr<Parser> create_parser_from_source(const std::string& source) {
    auto lexer = std::make_unique<Lexer>(source);
    auto stream = lexer->tokenize();
    return std::make_unique<Parser>(*stream);
}

// Test basic type inference for literals
TestResult test_basic_literal_type_inference() {
    std::string source = R"(
        fn test_function(): i32 {
            var int_var = 42;
            var bool_var = true;
            var string_var = "hello";
            var float_var = 3.14;
            return 0;
        }
    )";
    
    auto parser = create_parser_from_source(source);
    auto parse_result = parser->parse();
    
    ASSERT_TRUE(parse_result.is_success(), "Should parse successfully");
    
    SymbolTable symbol_table;
    build_symbol_table(symbol_table, parse_result.get_node());
    
    // Check that variables were resolved correctly
    auto int_symbol = symbol_table.lookup_symbol("int_var");
    ASSERT_TRUE(int_symbol != nullptr, "int_var should exist");
    ASSERT_EQ(int_symbol->type_name, "i32", "int_var should have type i32");
    ASSERT_EQ(int_symbol->resolution_state, TypeResolutionState::RESOLVED, "int_var should be resolved");
    
    auto bool_symbol = symbol_table.lookup_symbol("bool_var");
    ASSERT_TRUE(bool_symbol != nullptr, "bool_var should exist");
    ASSERT_EQ(bool_symbol->type_name, "bool", "bool_var should have type bool");
    ASSERT_EQ(bool_symbol->resolution_state, TypeResolutionState::RESOLVED, "bool_var should be resolved");
    
    auto string_symbol = symbol_table.lookup_symbol("string_var");
    ASSERT_TRUE(string_symbol != nullptr, "string_var should exist");
    ASSERT_EQ(string_symbol->type_name, "string", "string_var should have type string");
    ASSERT_EQ(string_symbol->resolution_state, TypeResolutionState::RESOLVED, "string_var should be resolved");
    
    auto float_symbol = symbol_table.lookup_symbol("float_var");
    ASSERT_TRUE(float_symbol != nullptr, "float_var should exist");
    ASSERT_EQ(float_symbol->type_name, "f32", "float_var should have type f32");
    ASSERT_EQ(float_symbol->resolution_state, TypeResolutionState::RESOLVED, "float_var should be resolved");
    
    return TestResult(true, "Basic literal type inference test passed");
}

// Test type inference from explicit variables
TestResult test_variable_dependency_type_inference() {
    std::string source = R"(
        fn test_function(): i32 {
            i32 explicit_var = 100;
            var inferred_var = explicit_var;
            return 0;
        }
    )";
    
    auto parser = create_parser_from_source(source);
    auto parse_result = parser->parse();
    
    ASSERT_TRUE(parse_result.is_success(), "Should parse successfully");
    
    SymbolTable symbol_table;
    build_symbol_table(symbol_table, parse_result.get_node());
    
    // Check that explicit_var has correct type
    auto explicit_symbol = symbol_table.lookup_symbol("explicit_var");
    ASSERT_TRUE(explicit_symbol != nullptr, "explicit_var should exist");
    ASSERT_EQ(explicit_symbol->type_name, "i32", "explicit_var should have type i32");
    ASSERT_EQ(explicit_symbol->resolution_state, TypeResolutionState::RESOLVED, "explicit_var should be resolved");
    
    // Check that inferred_var gets the same type
    auto inferred_symbol = symbol_table.lookup_symbol("inferred_var");
    ASSERT_TRUE(inferred_symbol != nullptr, "inferred_var should exist");
    ASSERT_EQ(inferred_symbol->type_name, "i32", "inferred_var should have type i32");
    ASSERT_EQ(inferred_symbol->resolution_state, TypeResolutionState::RESOLVED, "inferred_var should be resolved");
    
    return TestResult(true, "Variable dependency type inference test passed");
}

// Test chain dependency type inference
TestResult test_chain_dependency_type_inference() {
    std::string source = R"(
        fn test_function(): i32 {
            var first_var = 42;
            var second_var = first_var;
            var third_var = second_var;
            return 0;
        }
    )";
    
    auto parser = create_parser_from_source(source);
    auto parse_result = parser->parse();
    
    ASSERT_TRUE(parse_result.is_success(), "Should parse successfully");
    
    SymbolTable symbol_table;
    build_symbol_table(symbol_table, parse_result.get_node());
    
    // Check all variables have the correct type
    auto first_symbol = symbol_table.lookup_symbol("first_var");
    ASSERT_TRUE(first_symbol != nullptr, "first_var should exist");
    ASSERT_EQ(first_symbol->type_name, "i32", "first_var should have type i32");
    ASSERT_EQ(first_symbol->resolution_state, TypeResolutionState::RESOLVED, "first_var should be resolved");
    
    auto second_symbol = symbol_table.lookup_symbol("second_var");
    ASSERT_TRUE(second_symbol != nullptr, "second_var should exist");
    ASSERT_EQ(second_symbol->type_name, "i32", "second_var should have type i32");
    ASSERT_EQ(second_symbol->resolution_state, TypeResolutionState::RESOLVED, "second_var should be resolved");
    
    auto third_symbol = symbol_table.lookup_symbol("third_var");
    ASSERT_TRUE(third_symbol != nullptr, "third_var should exist");
    ASSERT_EQ(third_symbol->type_name, "i32", "third_var should have type i32");
    ASSERT_EQ(third_symbol->resolution_state, TypeResolutionState::RESOLVED, "third_var should be resolved");
    
    return TestResult(true, "Chain dependency type inference test passed");
}

// Test type inference with binary expressions
TestResult test_binary_expression_type_inference() {
    std::string source = R"(
        fn test_function(): i32 {
            var arithmetic_result = 5 + 10;
            var comparison_result = 5 < 10;
            var logical_result = true && false;
            return 0;
        }
    )";
    
    auto parser = create_parser_from_source(source);
    auto parse_result = parser->parse();
    
    ASSERT_TRUE(parse_result.is_success(), "Should parse successfully");
    
    SymbolTable symbol_table;
    build_symbol_table(symbol_table, parse_result.get_node());
    
    // Check arithmetic result type
    auto arithmetic_symbol = symbol_table.lookup_symbol("arithmetic_result");
    ASSERT_TRUE(arithmetic_symbol != nullptr, "arithmetic_result should exist");
    ASSERT_EQ(arithmetic_symbol->type_name, "i32", "arithmetic_result should have type i32");
    ASSERT_EQ(arithmetic_symbol->resolution_state, TypeResolutionState::RESOLVED, "arithmetic_result should be resolved");
    
    // Check comparison result type
    auto comparison_symbol = symbol_table.lookup_symbol("comparison_result");
    ASSERT_TRUE(comparison_symbol != nullptr, "comparison_result should exist");
    ASSERT_EQ(comparison_symbol->type_name, "bool", "comparison_result should have type bool");
    ASSERT_EQ(comparison_symbol->resolution_state, TypeResolutionState::RESOLVED, "comparison_result should be resolved");
    
    // Check logical result type
    auto logical_symbol = symbol_table.lookup_symbol("logical_result");
    ASSERT_TRUE(logical_symbol != nullptr, "logical_result should exist");
    ASSERT_EQ(logical_symbol->type_name, "bool", "logical_result should have type bool");
    ASSERT_EQ(logical_symbol->resolution_state, TypeResolutionState::RESOLVED, "logical_result should be resolved");
    
    return TestResult(true, "Binary expression type inference test passed");
}

// Test mixed explicit and inferred types
TestResult test_mixed_explicit_and_inferred_types() {
    std::string source = R"(
        fn test_function(): i32 {
            i32 explicit_int = 5;
            bool explicit_bool = true;
            var inferred_from_int = explicit_int;
            var inferred_from_bool = explicit_bool;
            var inferred_literal = 42;
            return 0;
        }
    )";
    
    auto parser = create_parser_from_source(source);
    auto parse_result = parser->parse();
    
    ASSERT_TRUE(parse_result.is_success(), "Should parse successfully");
    
    SymbolTable symbol_table;
    build_symbol_table(symbol_table, parse_result.get_node());
    
    // Check explicit types
    auto explicit_int = symbol_table.lookup_symbol("explicit_int");
    ASSERT_TRUE(explicit_int != nullptr, "explicit_int should exist");
    ASSERT_EQ(explicit_int->type_name, "i32", "explicit_int should have type i32");
    
    auto explicit_bool = symbol_table.lookup_symbol("explicit_bool");
    ASSERT_TRUE(explicit_bool != nullptr, "explicit_bool should exist");
    ASSERT_EQ(explicit_bool->type_name, "bool", "explicit_bool should have type bool");
    
    // Check inferred types
    auto inferred_from_int = symbol_table.lookup_symbol("inferred_from_int");
    ASSERT_TRUE(inferred_from_int != nullptr, "inferred_from_int should exist");
    ASSERT_EQ(inferred_from_int->type_name, "i32", "inferred_from_int should have type i32");
    
    auto inferred_from_bool = symbol_table.lookup_symbol("inferred_from_bool");
    ASSERT_TRUE(inferred_from_bool != nullptr, "inferred_from_bool should exist");
    ASSERT_EQ(inferred_from_bool->type_name, "bool", "inferred_from_bool should have type bool");
    
    auto inferred_literal = symbol_table.lookup_symbol("inferred_literal");
    ASSERT_TRUE(inferred_literal != nullptr, "inferred_literal should exist");
    ASSERT_EQ(inferred_literal->type_name, "i32", "inferred_literal should have type i32");
    
    return TestResult(true, "Mixed explicit and inferred types test passed");
}

// Test type inference with unary expressions
TestResult test_unary_expression_type_inference() {
    std::string source = R"(
        fn test_function(): i32 {
            var negated = -42;
            var positive = +10;
            var not_bool = !true;
            return 0;
        }
    )";
    
    auto parser = create_parser_from_source(source);
    auto parse_result = parser->parse();
    
    ASSERT_TRUE(parse_result.is_success(), "Should parse successfully");
    
    SymbolTable symbol_table;
    build_symbol_table(symbol_table, parse_result.get_node());
    
    // Check negated number type
    auto negated_symbol = symbol_table.lookup_symbol("negated");
    ASSERT_TRUE(negated_symbol != nullptr, "negated should exist");
    ASSERT_EQ(negated_symbol->type_name, "i32", "negated should have type i32");
    
    // Check positive number type
    auto positive_symbol = symbol_table.lookup_symbol("positive");
    ASSERT_TRUE(positive_symbol != nullptr, "positive should exist");
    ASSERT_EQ(positive_symbol->type_name, "i32", "positive should have type i32");
    
    // Check logical not type
    auto not_symbol = symbol_table.lookup_symbol("not_bool");
    ASSERT_TRUE(not_symbol != nullptr, "not_bool should exist");
    ASSERT_EQ(not_symbol->type_name, "bool", "not_bool should have type bool");
    
    return TestResult(true, "Unary expression type inference test passed");
}

void run_symbol_table_tests() {
    TestFramework framework;
    
    framework.run_test("Basic literal type inference", test_basic_literal_type_inference);
    framework.run_test("Variable dependency type inference", test_variable_dependency_type_inference);
    framework.run_test("Chain dependency type inference", test_chain_dependency_type_inference);
    framework.run_test("Binary expression type inference", test_binary_expression_type_inference);
    framework.run_test("Mixed explicit and inferred types", test_mixed_explicit_and_inferred_types);
    framework.run_test("Unary expression type inference", test_unary_expression_type_inference);
    
    framework.print_summary();
}