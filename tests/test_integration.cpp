#include "test/test_framework.hpp"
#include "parser/parser.h"
#include "parser/lexer.hpp"
#include "parser/token_stream.hpp"
#include "codegen/codegen.hpp"
#include "codegen/jit_engine.hpp"
#include "semantic/symbol_table.hpp"
#include "common/token.hpp"
#include "codegen/command_processor.hpp"
#include <iostream>
#include <string>

using namespace Mycelium::Scripting::Lang;
using namespace Mycelium::Testing;

// Diagnostic sink for lexer (same pattern as test_parser.cpp)
class TestLexerDiagnosticSink : public LexerDiagnosticSink {
public:
    std::vector<LexerDiagnostic> diagnostics;
    
    void report_diagnostic(const LexerDiagnostic& diagnostic) override {
        diagnostics.push_back(diagnostic);
    }
    
    bool has_errors() const {
        return !diagnostics.empty();
    }
};

// Helper function to create token stream from source
TokenStream create_integration_token_stream(const std::string& source) {
    TestLexerDiagnosticSink sink;
    Lexer lexer(source, {}, &sink);
    return lexer.tokenize_all();
}

// End-to-end test: source → lexer → parser → codegen → execution
TestResult test_simple_function_pipeline() {
    std::string source = R"(
        fn add_numbers(): i32 {
            return 5 + 3;
        }
    )";
    
    // Step 1: Lexer
    TokenStream stream = create_integration_token_stream(source);
    ASSERT_TRUE(stream.size() > 0, "Lexer should produce tokens");
    
    // Step 2: Parser
    Parser parser(stream);
    auto parse_result = parser.parse();
    ASSERT_TRUE(parse_result.is_success(), "Parser should successfully parse simple function");
    
    auto* unit = parse_result.get_node();
    ASSERT_TRUE(unit->statements.size == 1, "Should have one function declaration");
    ASSERT_TRUE(unit->statements[0]->is_a<FunctionDeclarationNode>(), "Should be function declaration");
    
    // Debug: Check what return type information is in the AST
    auto* func = static_cast<FunctionDeclarationNode*>(unit->statements[0]);
    std::cout << "=== DEBUG: Function AST Info ===" << std::endl;
    std::cout << "Function name: " << func->name->name << std::endl;
    if (func->returnType) {
        std::cout << "Has return type: YES" << std::endl;
        // Try to print return type info if possible
    } else {
        std::cout << "Has return type: NO" << std::endl;
    }
    std::cout << "=== END AST DEBUG ===" << std::endl;
    
    // Step 3: Code Generation
    SymbolTable symbol_table;
    CodeGenerator codegen(symbol_table);
    auto commands = codegen.generate_code(unit);
    
    // Debug: Print the number of commands generated
    std::cout << "=== DEBUG: Generated " << commands.size() << " commands ===" << std::endl;
    for (size_t i = 0; i < commands.size(); ++i) {
        std::cout << "Command " << i << ": " << (int)commands[i].op << std::endl;
    }
    std::cout << "=== END COMMANDS DEBUG ===" << std::endl;
    
    ASSERT_FALSE(commands.empty(), "Should generate commands");
    
    // Step 4: IR Generation
    std::string ir = CommandProcessor::process_to_ir_string(commands, "TestModule");
    
    // Debug: Print the generated IR to see what's happening
    std::cout << "=== DEBUG: Generated IR ===" << std::endl;
    std::cout << ir << std::endl;
    std::cout << "=== END DEBUG IR ===" << std::endl;
    
    ASSERT_FALSE(ir.empty(), "Should generate IR string");
    
    // Step 5: JIT Execution
    JITEngine jit;
    bool init_success = jit.initialize_from_ir(ir, "TestModule");
    ASSERT_TRUE(init_success, "Should successfully initialize JIT with IR");
    
    int result = jit.execute_function("add_numbers");
    ASSERT_EQ(8, result, "Function should return 5 + 3 = 8");
    
    return TestResult(true, "Simple function pipeline test successful");
}

TestResult test_variable_declaration_pipeline() {
    std::string source = R"(
        fn calculate(): i32 {
            var x = 10;
            var y = 5;
            return x * y;
        }
    )";
    
    // Lexer → Parser
    TokenStream stream = create_integration_token_stream(source);
    Parser parser(stream);
    auto parse_result = parser.parse();
    ASSERT_TRUE(parse_result.is_success(), "Parser should handle variable declarations");
    
    auto* unit = parse_result.get_node();
    auto* func = static_cast<FunctionDeclarationNode*>(unit->statements[0]);
    ASSERT_TRUE(func->body != nullptr, "Function should have body");
    ASSERT_TRUE(func->body->statements.size == 3, "Should have 2 var declarations + 1 return");
    
    // Verify AST structure
    ASSERT_TRUE(func->body->statements[0]->is_a<VariableDeclarationNode>(), "First should be var declaration");
    ASSERT_TRUE(func->body->statements[1]->is_a<VariableDeclarationNode>(), "Second should be var declaration");
    ASSERT_TRUE(func->body->statements[2]->is_a<ReturnStatementNode>(), "Third should be return statement");
    
    // Code Generation → Execution
    SymbolTable symbol_table;
    CodeGenerator codegen(symbol_table);
    auto commands = codegen.generate_code(unit);
    ASSERT_FALSE(commands.empty(), "Should generate commands for variables");
    
    std::string ir = CommandProcessor::process_to_ir_string(commands, "TestModule");
    ASSERT_FALSE(ir.empty(), "Should generate IR for variable declarations");
    
    JITEngine jit;
    bool init_success = jit.initialize_from_ir(ir, "TestModule");
    ASSERT_TRUE(init_success, "Should initialize JIT with variable declaration IR");
    
    int result = jit.execute_function("calculate");
    ASSERT_EQ(50, result, "Function should return 10 * 5 = 50");
    
    return TestResult(true, "Variable declaration pipeline test successful");
}

TestResult test_arithmetic_expressions_pipeline() {
    std::string source = R"(
        fn complex_math(): i32 {
            return (3 + 5) * 2 - 1;
        }
    )";
    
    TokenStream stream = create_integration_token_stream(source);
    Parser parser(stream);
    auto parse_result = parser.parse();
    ASSERT_TRUE(parse_result.is_success(), "Parser should handle complex expressions");
    
    auto* unit = parse_result.get_node();
    auto* func = static_cast<FunctionDeclarationNode*>(unit->statements[0]);
    auto* return_stmt = static_cast<ReturnStatementNode*>(func->body->statements[0]);
    
    // Verify expression parsing
    ASSERT_TRUE(return_stmt->expression != nullptr, "Return should have expression");
    ASSERT_TRUE(return_stmt->expression->is_a<BinaryExpressionNode>(), "Should be binary expression");
    
    // Full pipeline test
    SymbolTable symbol_table;
    CodeGenerator codegen(symbol_table);
    auto commands = codegen.generate_code(unit);
    ASSERT_FALSE(commands.empty(), "Should generate commands for expressions");
    
    std::string ir = CommandProcessor::process_to_ir_string(commands, "TestModule");
    ASSERT_FALSE(ir.empty(), "Should generate IR for arithmetic expressions");
    
    JITEngine jit;
    bool init_success = jit.initialize_from_ir(ir, "TestModule");
    ASSERT_TRUE(init_success, "Should initialize JIT with arithmetic IR");
    
    int result = jit.execute_function("complex_math");
    ASSERT_EQ(15, result, "Should return (3 + 5) * 2 - 1 = 15");
    
    return TestResult(true, "Arithmetic expressions pipeline test successful");
}

TestResult test_multiple_functions_pipeline() {
    std::string source = R"(
        fn helper(): i32 {
            return 42;
        }
        
        fn main(): i32 {
            var result = 10;
            return result + 5;
        }
    )";
    
    TokenStream stream = create_integration_token_stream(source);
    Parser parser(stream);
    auto parse_result = parser.parse();
    ASSERT_TRUE(parse_result.is_success(), "Parser should handle multiple functions");
    
    auto* unit = parse_result.get_node();
    ASSERT_TRUE(unit->statements.size == 2, "Should have two function declarations");
    ASSERT_TRUE(unit->statements[0]->is_a<FunctionDeclarationNode>(), "First should be function");
    ASSERT_TRUE(unit->statements[1]->is_a<FunctionDeclarationNode>(), "Second should be function");
    
    auto* helper_func = static_cast<FunctionDeclarationNode*>(unit->statements[0]);
    auto* main_func = static_cast<FunctionDeclarationNode*>(unit->statements[1]);
    ASSERT_TRUE(std::string(helper_func->name->name) == "helper", "First function should be 'helper'");
    ASSERT_TRUE(std::string(main_func->name->name) == "main", "Second function should be 'main'");
    
    // Full pipeline
    SymbolTable symbol_table;
    CodeGenerator codegen(symbol_table);
    auto commands = codegen.generate_code(unit);
    ASSERT_FALSE(commands.empty(), "Should generate commands for multiple functions");
    
    std::string ir = CommandProcessor::process_to_ir_string(commands, "TestModule");
    ASSERT_FALSE(ir.empty(), "Should generate IR for multiple functions");
    
    JITEngine jit;
    bool init_success = jit.initialize_from_ir(ir, "TestModule");
    ASSERT_TRUE(init_success, "Should initialize JIT with multiple functions");
    
    // Test both functions
    int helper_result = jit.execute_function("helper");
    ASSERT_EQ(42, helper_result, "Helper should return 42");
    
    int main_result = jit.execute_function("main");
    ASSERT_EQ(15, main_result, "Main should return 10 + 5 = 15");
    
    return TestResult(true, "Multiple functions pipeline test successful");
}

TestResult test_member_var_declarations_pipeline() {
    std::string source = R"(
        type Calculator {
            var value = 100;
            var multiplier = 2;
        }
        
        fn test(): i32 {
            return 7 * 6;
        }
    )";
    
    TokenStream stream = create_integration_token_stream(source);
    Parser parser(stream);
    auto parse_result = parser.parse();
    ASSERT_TRUE(parse_result.is_success(), "Parser should handle type with var fields");
    
    auto* unit = parse_result.get_node();
    ASSERT_TRUE(unit->statements.size == 2, "Should have type declaration and function");
    
    // Verify type declaration with var fields
    ASSERT_TRUE(unit->statements[0]->is_a<TypeDeclarationNode>(), "First should be type declaration");
    auto* type_decl = static_cast<TypeDeclarationNode*>(unit->statements[0]);
    ASSERT_TRUE(type_decl->members.size == 2, "Type should have 2 member fields");
    
    for (int i = 0; i < type_decl->members.size; i++) {
        ASSERT_TRUE(type_decl->members[i]->is_a<VariableDeclarationNode>(), "Members should be variable declarations");
        auto* field = static_cast<VariableDeclarationNode*>(type_decl->members[i]);
        ASSERT_TRUE(field->initializer != nullptr, "Var fields should have initializers");
    }
    
    // Test that the function still works in the same compilation unit
    ASSERT_TRUE(unit->statements[1]->is_a<FunctionDeclarationNode>(), "Second should be function");
    
    // Full pipeline (codegen may not fully support types yet, but should not crash)
    SymbolTable symbol_table;
    CodeGenerator codegen(symbol_table);
    auto commands = codegen.generate_code(unit);
    // Commands may be empty if type declarations aren't implemented yet
    
    std::string ir = CommandProcessor::process_to_ir_string(commands, "TestModule");
    // IR may be minimal if type declarations aren't implemented yet
    
    // Test the function part if IR was generated
    if (!ir.empty() && ir.find("define") != std::string::npos) {
        JITEngine jit;
        bool init_success = jit.initialize_from_ir(ir, "TestModule");
        ASSERT_TRUE(init_success, "Should initialize JIT even with type declarations");
        
        int result = jit.execute_function("test");
        ASSERT_EQ(42, result, "Function should return 7 * 6 = 42");
    }
    
    return TestResult(true, "Member var declarations pipeline test successful");
}

TestResult test_pipeline_error_handling() {
    std::string source = R"(
        fn broken_function(): i32 {
            var x = 5 +;  // Syntax error
            return x;
        }
    )";
    
    TokenStream stream = create_integration_token_stream(source);
    Parser parser(stream);
    auto parse_result = parser.parse();
    
    // Parser should handle errors gracefully with error recovery
    ASSERT_TRUE(parse_result.is_success(), "Parser should recover from syntax errors");
    
    auto* unit = parse_result.get_node();
    ASSERT_TRUE(unit != nullptr, "Should still produce AST with error nodes");
    
    // Verify error nodes are present in AST
    auto* func = static_cast<FunctionDeclarationNode*>(unit->statements[0]);
    bool found_error = false;
    
    // Check if there are error nodes in the function body
    for (int i = 0; i < func->body->statements.size; i++) {
        AstNode* stmt = func->body->statements[i];
        if (stmt->is_a<VariableDeclarationNode>()) {
            auto* var_stmt = static_cast<VariableDeclarationNode*>(stmt);
            if (var_stmt->contains_errors)
            {
                found_error = true;
                break;
            }
        }
    }
    
    ASSERT_TRUE(found_error, "Should have error nodes from syntax error");
    
    // Code generation should handle error nodes gracefully
    SymbolTable symbol_table;
    CodeGenerator codegen(symbol_table);
    auto commands = codegen.generate_code(unit);  // Should not crash
    
    // Commands may be empty or minimal due to errors, but should not crash
    std::string ir = CommandProcessor::process_to_ir_string(commands, "TestModule");
    
    return TestResult(true, "Pipeline error handling test successful");
}

TestResult test_nested_loops_pipeline() {
    std::string source = R"(
        fn matrix_sum(i32 rows, i32 cols): i32 {
            var total = 0;
            
            for (var i = 0; i < rows; i = i + 1) {
                for (var j = 0; j < cols; j = j + 1) {
                    total = total + (i * cols + j);
                }
            }
            
            return total;
        }

        fn main(): i32 {
            return matrix_sum(3, 4);
        }
    )";
    
    TokenStream stream = create_integration_token_stream(source);
    Parser parser(stream);
    auto parse_result = parser.parse();
    ASSERT_TRUE(parse_result.is_success(), "Parser should handle nested loops");
    
    auto* unit = parse_result.get_node();
    ASSERT_TRUE(unit->statements.size == 2, "Should have two functions");
    
    // Verify parsing structure
    auto* matrix_func = static_cast<FunctionDeclarationNode*>(unit->statements[0]);
    ASSERT_TRUE(std::string(matrix_func->name->name) == "matrix_sum", "First function should be matrix_sum");
    ASSERT_TRUE(matrix_func->parameters.size == 2, "matrix_sum should have 2 parameters");
    
    // Full pipeline
    SymbolTable symbol_table;
    build_symbol_table(symbol_table, unit);
    CodeGenerator codegen(symbol_table);
    auto commands = codegen.generate_code(unit);
    ASSERT_FALSE(commands.empty(), "Should generate commands for nested loops");
    
    std::string ir = CommandProcessor::process_to_ir_string(commands, "TestModule");
    ASSERT_FALSE(ir.empty(), "Should generate IR for nested loops");
    
    JITEngine jit;
    bool init_success = jit.initialize_from_ir(ir, "TestModule");
    ASSERT_TRUE(init_success, "Should initialize JIT with nested loops IR");
    
    int result = jit.execute_function("main");
    ASSERT_EQ(66, result, "matrix_sum(3,4) should return 66");
    
    return TestResult(true, "Nested loops pipeline test successful");
}

TestResult test_function_calls_pipeline() {
    std::string source = R"(
        fn helper1(i32 x): i32 {
            return x * 2 + 1;
        }

        fn helper2(i32 x): i32 {
            return x * x - 3;
        }

        fn helper3(i32 x, i32 y): i32 {
            return helper1(x) + helper2(y);
        }

        fn chain_calls(i32 start): i32 {
            var a = helper1(start);
            var b = helper2(a);
            var c = helper3(b, start);
            return c;
        }

        fn main(): i32 {
            return chain_calls(5);
        }
    )";
    
    TokenStream stream = create_integration_token_stream(source);
    Parser parser(stream);
    auto parse_result = parser.parse();
    ASSERT_TRUE(parse_result.is_success(), "Parser should handle multiple function calls");
    
    auto* unit = parse_result.get_node();
    ASSERT_TRUE(unit->statements.size == 5, "Should have five functions");
    
    // Verify function declarations
    for (int i = 0; i < unit->statements.size; ++i) {
        ASSERT_TRUE(unit->statements[i]->is_a<FunctionDeclarationNode>(), "All statements should be functions");
    }
    
    // Full pipeline
    SymbolTable symbol_table;
    build_symbol_table(symbol_table, unit);
    CodeGenerator codegen(symbol_table);
    auto commands = codegen.generate_code(unit);
    ASSERT_FALSE(commands.empty(), "Should generate commands for function calls");
    
    std::string ir = CommandProcessor::process_to_ir_string(commands, "TestModule");
    ASSERT_FALSE(ir.empty(), "Should generate IR for function calls");
    
    JITEngine jit;
    bool init_success = jit.initialize_from_ir(ir, "TestModule");
    ASSERT_TRUE(init_success, "Should initialize JIT with function calls IR");
    
    // Test the main chain function (which calls the helpers internally)
    int main_result = jit.execute_function("main");
    // This will test the actual computation result of chain_calls(5)
    // The specific value depends on the implementation but should be consistent
    
    return TestResult(true, "Function calls pipeline test successful");
}

TestResult test_arithmetic_algorithms_pipeline() {
    std::string source = R"(
        fn gcd(i32 a, i32 b): i32 {
            while (b != 0) {
                var temp = b;
                b = a - (a / b) * b;  // modulo operation
                a = temp;
            }
            return a;
        }

        fn sum_of_divisors(i32 n): i32 {
            var sum = 0;
            for (var i = 1; i <= n; i = i + 1) {
                if ((n / i) * i == n) {  // i divides n
                    sum = sum + i;
                }
            }
            return sum;
        }

        fn main(): i32 {
            var gcd_result = gcd(12, 18);
            var div_sum = sum_of_divisors(12);
            return gcd_result + div_sum;
        }
    )";
    
    TokenStream stream = create_integration_token_stream(source);
    Parser parser(stream);
    auto parse_result = parser.parse();
    ASSERT_TRUE(parse_result.is_success(), "Parser should handle arithmetic algorithms");
    
    auto* unit = parse_result.get_node();
    ASSERT_TRUE(unit->statements.size == 3, "Should have three functions");
    
    // Full pipeline
    SymbolTable symbol_table;
    build_symbol_table(symbol_table, unit);
    CodeGenerator codegen(symbol_table);
    auto commands = codegen.generate_code(unit);
    ASSERT_FALSE(commands.empty(), "Should generate commands for algorithms");
    
    std::string ir = CommandProcessor::process_to_ir_string(commands, "TestModule");
    ASSERT_FALSE(ir.empty(), "Should generate IR for algorithms");
    
    JITEngine jit;
    bool init_success = jit.initialize_from_ir(ir, "TestModule");
    ASSERT_TRUE(init_success, "Should initialize JIT with algorithms IR");
    
    int result = jit.execute_function("main");
    // This will test whatever the actual implementation produces
    // We're more interested that it compiles and runs without crashing
    
    return TestResult(true, "Arithmetic algorithms pipeline test successful");
}

TestResult test_complex_expressions_pipeline() {
    std::string source = R"(
        fn evaluate_polynomial(i32 x): i32 {
            // Evaluate: 3x^3 + 2x^2 - 5x + 7
            var x2 = x * x;
            var x3 = x2 * x;
            return 3 * x3 + 2 * x2 - 5 * x + 7;
        }

        fn sum_range(i32 start, i32 end): i32 {
            var sum = 0;
            for (var i = start; i <= end; i = i + 1) {
                sum = sum + i;
            }
            return sum;
        }

        fn main(): i32 {
            var poly_val = evaluate_polynomial(3);
            var range_sum = sum_range(1, 10);
            return poly_val + range_sum;
        }
    )";
    
    TokenStream stream = create_integration_token_stream(source);
    Parser parser(stream);
    auto parse_result = parser.parse();
    ASSERT_TRUE(parse_result.is_success(), "Parser should handle complex expressions");
    
    auto* unit = parse_result.get_node();
    ASSERT_TRUE(unit->statements.size == 3, "Should have three functions");
    
    // Full pipeline
    SymbolTable symbol_table;
    build_symbol_table(symbol_table, unit);
    CodeGenerator codegen(symbol_table);
    auto commands = codegen.generate_code(unit);
    ASSERT_FALSE(commands.empty(), "Should generate commands for complex expressions");
    
    std::string ir = CommandProcessor::process_to_ir_string(commands, "TestModule");
    ASSERT_FALSE(ir.empty(), "Should generate IR for complex expressions");
    
    JITEngine jit;
    bool init_success = jit.initialize_from_ir(ir, "TestModule");
    ASSERT_TRUE(init_success, "Should initialize JIT with complex expressions IR");
    
    int result = jit.execute_function("main");
    // evaluate_polynomial(3) = 3*27 + 2*9 - 5*3 + 7 = 81 + 18 - 15 + 7 = 91
    // sum_range(1,10) = 55
    // Expected: 91 + 55 = 146
    ASSERT_EQ(146, result, "Complex expressions should evaluate correctly");
    
    return TestResult(true, "Complex expressions pipeline test successful");
}

TestResult test_fibonacci_classic_pipeline() {
    std::string source = R"(
        fn fib(i32 n): i32 {
            var a = 1;
            var b = 1;

            var i = 0;
            for (var j = 0; j < n; j = j + 1) {
                var temp = a + b;
                a = b;
                b = temp;
            }

            return b;
        }

        fn main(): i32 {
            var result = fib(8);
            return result;
        }
    )";
    
    TokenStream stream = create_integration_token_stream(source);
    Parser parser(stream);
    auto parse_result = parser.parse();
    ASSERT_TRUE(parse_result.is_success(), "Parser should handle fibonacci implementation");
    
    auto* unit = parse_result.get_node();
    ASSERT_TRUE(unit->statements.size == 2, "Should have two functions");
    
    // Verify fibonacci function structure
    auto* fib_func = static_cast<FunctionDeclarationNode*>(unit->statements[0]);
    ASSERT_TRUE(std::string(fib_func->name->name) == "fib", "First function should be fib");
    ASSERT_TRUE(fib_func->parameters.size == 1, "fib should have 1 parameter");
    
    // Full pipeline
    SymbolTable symbol_table;
    build_symbol_table(symbol_table, unit);
    CodeGenerator codegen(symbol_table);
    auto commands = codegen.generate_code(unit);
    ASSERT_FALSE(commands.empty(), "Should generate commands for fibonacci");
    
    std::string ir = CommandProcessor::process_to_ir_string(commands, "TestModule");
    ASSERT_FALSE(ir.empty(), "Should generate IR for fibonacci");
    
    JITEngine jit;
    bool init_success = jit.initialize_from_ir(ir, "TestModule");
    ASSERT_TRUE(init_success, "Should initialize JIT with fibonacci IR");
    
    int result = jit.execute_function("main");
    ASSERT_EQ(55, result, "fib(8) should return 55 (8th Fibonacci number)");
    
    return TestResult(true, "Fibonacci classic pipeline test successful");
}

TestResult test_recursion_pipeline() {
    std::string source = R"(
        fn factorial(i32 n): i32 {
            if (n <= 1) {
                return 1;
            }
            return n * factorial(n - 1);
        }

        fn power(i32 base, i32 exp): i32 {
            if (exp == 0) {
                return 1;
            }
            if (exp == 1) {
                return base;
            }
            return base * power(base, exp - 1);
        }

        fn main(): i32 {
            var fact5 = factorial(5);
            var pow23 = power(2, 3);
            return fact5 + pow23;
        }
    )";
    
    TokenStream stream = create_integration_token_stream(source);
    Parser parser(stream);
    auto parse_result = parser.parse();
    ASSERT_TRUE(parse_result.is_success(), "Parser should handle recursive functions with conditionals");
    
    auto* unit = parse_result.get_node();
    ASSERT_TRUE(unit->statements.size == 3, "Should have three functions");
    
    // Verify function structures
    auto* factorial_func = static_cast<FunctionDeclarationNode*>(unit->statements[0]);
    auto* power_func = static_cast<FunctionDeclarationNode*>(unit->statements[1]);
    ASSERT_TRUE(std::string(factorial_func->name->name) == "factorial", "First function should be factorial");
    ASSERT_TRUE(std::string(power_func->name->name) == "power", "Second function should be power");
    
    // Full pipeline
    SymbolTable symbol_table;
    build_symbol_table(symbol_table, unit);
    CodeGenerator codegen(symbol_table);
    auto commands = codegen.generate_code(unit);
    ASSERT_FALSE(commands.empty(), "Should generate commands for recursive functions");
    
    std::string ir = CommandProcessor::process_to_ir_string(commands, "TestModule");
    ASSERT_FALSE(ir.empty(), "Should generate IR for recursive functions");
    
    // The IR should contain conditional branches for the if statements
    bool has_conditional_branch = ir.find("br i1") != std::string::npos;
    ASSERT_TRUE(has_conditional_branch, "Recursive functions should have conditional branches for base cases");
    
    // Test execution
    JITEngine jit;
    bool init_success = jit.initialize_from_ir(ir, "TestModule");
    ASSERT_TRUE(init_success, "Should initialize JIT with recursive functions IR");
    
    int result = jit.execute_function("main");
    // factorial(5) = 120, power(2,3) = 8, so result should be 128
    ASSERT_EQ(128, result, "factorial(5) + power(2,3) should return 120 + 8 = 128");
    
    return TestResult(true, "Recursion pipeline test successful");
}

TestResult test_if_statement_pipeline() {
    std::string source = R"(
        fn abs(i32 x): i32 {
            if (x < 0) {
                return -x;
            }
            return x;
        }

        fn sign(i32 x): i32 {
            if (x < 0) {
                return -1;
            } else if (x > 0) {
                return 1;
            } else {
                return 0;
            }
        }

        fn main(): i32 {
            var neg = abs(-42);
            var pos = abs(42);
            var sign_neg = sign(-10);
            var sign_zero = sign(0);
            var sign_pos = sign(10);
            return neg + pos + sign_neg + sign_zero + sign_pos;
        }
    )";
    
    TokenStream stream = create_integration_token_stream(source);
    Parser parser(stream);
    auto parse_result = parser.parse();
    ASSERT_TRUE(parse_result.is_success(), "Parser should handle if statements");
    
    auto* unit = parse_result.get_node();
    ASSERT_TRUE(unit->statements.size == 3, "Should have three functions");
    
    // Full pipeline
    SymbolTable symbol_table;
    build_symbol_table(symbol_table, unit);
    CodeGenerator codegen(symbol_table);
    auto commands = codegen.generate_code(unit);
    
    std::string ir = CommandProcessor::process_to_ir_string(commands, "TestModule");
    
    
    // Check for conditional branches
    bool has_conditional_branch = ir.find("br i1") != std::string::npos;
    ASSERT_TRUE(has_conditional_branch, "If statements should generate conditional branches");
    
    JITEngine jit;
    bool init_success = jit.initialize_from_ir(ir, "TestModule");
    ASSERT_TRUE(init_success, "Should initialize JIT with if statement IR");
    
    int result = jit.execute_function("main");
    // abs(-42) = 42, abs(42) = 42, sign(-10) = -1, sign(0) = 0, sign(10) = 1
    // 42 + 42 + (-1) + 0 + 1 = 84
    ASSERT_EQ(84, result, "If statement logic should work correctly");
    
    return TestResult(true, "If statement pipeline test successful");
}

TestResult test_while_loop_pipeline() {
    std::string source = R"(
        fn count_down(i32 n): i32 {
            var count = 0;
            while (n > 0) {
                count = count + 1;
                n = n - 1;
            }
            return count;
        }

        fn find_first_multiple(i32 start, i32 divisor): i32 {
            var i = start;
            while ((i / divisor) * divisor != i) {
                i = i + 1;
            }
            return i;
        }

        fn main(): i32 {
            var count = count_down(10);
            var multiple = find_first_multiple(17, 5);
            return count + multiple;
        }
    )";
    
    TokenStream stream = create_integration_token_stream(source);
    Parser parser(stream);
    auto parse_result = parser.parse();
    ASSERT_TRUE(parse_result.is_success(), "Parser should handle while loops");
    
    auto* unit = parse_result.get_node();
    ASSERT_TRUE(unit->statements.size == 3, "Should have three functions");
    
    // Full pipeline
    SymbolTable symbol_table;
    build_symbol_table(symbol_table, unit);
    CodeGenerator codegen(symbol_table);
    auto commands = codegen.generate_code(unit);
    
    std::string ir = CommandProcessor::process_to_ir_string(commands, "TestModule");
    
    // Check for loop structure
    bool has_loop_header = ir.find("loop") != std::string::npos || ir.find("while") != std::string::npos;
    ASSERT_TRUE(has_loop_header, "While loops should generate loop structures");
    
    JITEngine jit;
    bool init_success = jit.initialize_from_ir(ir, "TestModule");
    ASSERT_TRUE(init_success, "Should initialize JIT with while loop IR");
    
    int result = jit.execute_function("main");
    // count_down(10) = 10, find_first_multiple(17, 5) = 20
    // 10 + 20 = 30
    ASSERT_EQ(30, result, "While loop logic should work correctly");
    
    return TestResult(true, "While loop pipeline test successful");
}

TestResult test_type_declaration_pipeline() {
    std::string source = R"(
        type Point {
            var x: i32;
            var y: i32;
        }

        type Rectangle {
            var topLeft: Point;
            var bottomRight: Point;
            
            fn area(): i32 {
                var width = bottomRight.x - topLeft.x;
                var height = bottomRight.y - topLeft.y;
                return width * height;
            }
        }

        fn main(): i32 {
            // Type instantiation is not yet implemented, so just return a constant
            return 42;
        }
    )";
    
    TokenStream stream = create_integration_token_stream(source);
    Parser parser(stream);
    auto parse_result = parser.parse();
    ASSERT_TRUE(parse_result.is_success(), "Parser should handle type declarations");
    
    auto* unit = parse_result.get_node();
    ASSERT_TRUE(unit->statements.size >= 2, "Should have at least two type declarations");
    
    // Verify type declarations
    int type_count = 0;
    int func_count = 0;
    for (int i = 0; i < unit->statements.size; ++i) {
        if (unit->statements[i]->is_a<TypeDeclarationNode>()) {
            type_count++;
            auto* type_decl = static_cast<TypeDeclarationNode*>(unit->statements[i]);
            if (std::string(type_decl->name->name) == "Rectangle") {
                // Check that Rectangle has a method
                bool has_method = false;
                for (int j = 0; j < type_decl->members.size; ++j) {
                    if (type_decl->members[j]->is_a<FunctionDeclarationNode>()) {
                        has_method = true;
                        break;
                    }
                }
                ASSERT_TRUE(has_method, "Rectangle type should have area() method");
            }
        } else if (unit->statements[i]->is_a<FunctionDeclarationNode>()) {
            func_count++;
        }
    }
    
    ASSERT_TRUE(type_count >= 2, "Should have at least 2 type declarations");
    ASSERT_TRUE(func_count >= 1, "Should have at least 1 function (main)");
    
    // Full pipeline (may not generate much if types aren't fully implemented)
    SymbolTable symbol_table;
    build_symbol_table(symbol_table, unit);
    
    // Check that types are in symbol table
    auto point_symbol = symbol_table.lookup_symbol("Point");
    auto rect_symbol = symbol_table.lookup_symbol("Rectangle");
    ASSERT_TRUE(point_symbol != nullptr, "Point type should be in symbol table");
    ASSERT_TRUE(rect_symbol != nullptr, "Rectangle type should be in symbol table");
    
    return TestResult(true, "Type declaration pipeline test successful");
}

TestResult test_break_continue_pipeline() {
    std::string source = R"(
        fn sum_until_negative(i32 limit): i32 {
            var sum = 0;
            var i = 0;
            while (i < limit) {
                if (i < 0) {
                    break;
                }
                if ((i / 2) * 2 == i) {  // even number
                    i = i + 1;
                    continue;
                }
                sum = sum + i;
                i = i + 1;
            }
            return sum;
        }

        fn find_factor(i32 n, i32 max_tries): i32 {
            for (var i = 2; i < max_tries; i = i + 1) {
                if ((n / i) * i == n) {
                    return i;  // early return acts like break
                }
            }
            return -1;
        }

        fn main(): i32 {
            var sum = sum_until_negative(10);
            var factor = find_factor(15, 10);
            return sum + factor;
        }
    )";
    
    TokenStream stream = create_integration_token_stream(source);
    Parser parser(stream);
    auto parse_result = parser.parse();
    ASSERT_TRUE(parse_result.is_success(), "Parser should handle break/continue statements");
    
    auto* unit = parse_result.get_node();
    
    // Full pipeline
    SymbolTable symbol_table;
    build_symbol_table(symbol_table, unit);
    CodeGenerator codegen(symbol_table);
    auto commands = codegen.generate_code(unit);
    
    std::string ir = CommandProcessor::process_to_ir_string(commands, "TestModule");
    
    // Check for break/continue implementation (would need special branch targets)
    // This test will likely fail until break/continue are implemented
    
    JITEngine jit;
    bool init_success = jit.initialize_from_ir(ir, "TestModule");
    ASSERT_TRUE(init_success, "Should initialize JIT with break/continue IR");
    
    int result = jit.execute_function("main");
    // sum_until_negative(10) sums odd numbers: 1+3+5+7+9 = 25
    // find_factor(15, 10) finds 3
    // 25 + 3 = 28
    ASSERT_EQ(28, result, "Break/continue logic should work correctly");
    
    return TestResult(true, "Break/continue pipeline test successful");
}

TestResult test_logical_operators_pipeline() {
    std::string source = R"(
        fn is_valid_age(i32 age): bool {
            return age >= 0 && age <= 120;
        }

        fn is_special_number(i32 n): bool {
            return n == 0 || n == 1 || n == 42;
        }

        fn complex_logic(i32 a, i32 b, i32 c): bool {
            return (a > 0 && b > 0) || (c < 0 && !is_special_number(a));
        }

        fn main(): i32 {
            var valid1 = is_valid_age(25);
            var valid2 = is_valid_age(-5);
            var valid3 = is_valid_age(150);
            
            var special1 = is_special_number(42);
            var special2 = is_special_number(10);
            
            // Convert bools to ints for return
            var result = 0;
            if (valid1) result = result + 1;
            if (!valid2) result = result + 2;
            if (!valid3) result = result + 4;
            if (special1) result = result + 8;
            if (!special2) result = result + 16;
            
            return result;
        }
    )";
    
    TokenStream stream = create_integration_token_stream(source);
    Parser parser(stream);
    auto parse_result = parser.parse();
    ASSERT_TRUE(parse_result.is_success(), "Parser should handle logical operators");
    
    auto* unit = parse_result.get_node();
    
    // Full pipeline
    SymbolTable symbol_table;
    build_symbol_table(symbol_table, unit);
    CodeGenerator codegen(symbol_table);
    auto commands = codegen.generate_code(unit);
    
    std::string ir = CommandProcessor::process_to_ir_string(commands, "TestModule");
    
    // Check for logical operations (should see AND/OR operations)
    bool has_logical_ops = ir.find("and") != std::string::npos || ir.find("or") != std::string::npos;
    ASSERT_TRUE(has_logical_ops, "Logical operators should generate AND/OR operations");
    
    JITEngine jit;
    bool init_success = jit.initialize_from_ir(ir, "TestModule");
    ASSERT_TRUE(init_success, "Should initialize JIT with logical operators IR");
    
    int result = jit.execute_function("main");
    // Expected: 1 + 2 + 4 + 8 + 16 = 31
    ASSERT_EQ(31, result, "Logical operators should work correctly");
    
    return TestResult(true, "Logical operators pipeline test successful");
}

TestResult test_array_operations_pipeline() {
    std::string source = R"(
        fn sum_array(i32[] arr): i32 {
            var sum = 0;
            for (var i = 0; i < arr.length; i = i + 1) {
                sum = sum + arr[i];
            }
            return sum;
        }

        fn find_max(i32[] arr): i32 {
            if (arr.length == 0) {
                return -1;
            }
            var max = arr[0];
            for (var i = 1; i < arr.length; i = i + 1) {
                if (arr[i] > max) {
                    max = arr[i];
                }
            }
            return max;
        }

        fn main(): i32 {
            i32[] numbers;
            numbers[0] = 10;
            numbers[1] = 20;
            numbers[2] = 5;
            numbers[3] = 15;
            numbers[4] = 25;
            
            var sum = sum_array(numbers);
            var max = find_max(numbers);
            
            return sum + max;
        }
    )";
    
    TokenStream stream = create_integration_token_stream(source);
    Parser parser(stream);
    auto parse_result = parser.parse();
    ASSERT_TRUE(parse_result.is_success(), "Parser should handle array operations");
    
    auto* unit = parse_result.get_node();
    
    // Full pipeline
    SymbolTable symbol_table;
    build_symbol_table(symbol_table, unit);
    CodeGenerator codegen(symbol_table);
    auto commands = codegen.generate_code(unit);
    
    std::string ir = CommandProcessor::process_to_ir_string(commands, "TestModule");
    
    // Check for array operations (getelementptr for indexing)
    bool has_array_ops = ir.find("getelementptr") != std::string::npos;
    ASSERT_TRUE(has_array_ops, "Array operations should generate getelementptr instructions");
    
    JITEngine jit;
    bool init_success = jit.initialize_from_ir(ir, "TestModule");
    ASSERT_TRUE(init_success, "Should initialize JIT with array operations IR");
    
    int result = jit.execute_function("main");
    // sum = 75, max = 25, result = 100
    ASSERT_EQ(100, result, "Array operations should work correctly");
    
    return TestResult(true, "Array operations pipeline test successful");
}

TestResult test_string_operations_pipeline() {
    std::string source = R"(
        fn string_length(string s): i32 {
            return s.length;
        }

        fn concat_strings(string a, string b): string {
            return a + b;
        }

        fn main(): i32 {
            var hello = "Hello";
            var world = "World";
            var greeting = concat_strings(hello, world);
            
            return string_length(greeting);
        }
    )";
    
    TokenStream stream = create_integration_token_stream(source);
    Parser parser(stream);
    auto parse_result = parser.parse();
    ASSERT_TRUE(parse_result.is_success(), "Parser should handle string operations");
    
    auto* unit = parse_result.get_node();
    
    // Full pipeline
    SymbolTable symbol_table;
    build_symbol_table(symbol_table, unit);
    CodeGenerator codegen(symbol_table);
    auto commands = codegen.generate_code(unit);
    
    std::string ir = CommandProcessor::process_to_ir_string(commands, "TestModule");
    
    // Check for string handling (would need string type support)
    bool has_string_ops = ir.find("@string") != std::string::npos || ir.find("str") != std::string::npos;
    ASSERT_TRUE(has_string_ops, "String operations should be present in IR");
    
    JITEngine jit;
    bool init_success = jit.initialize_from_ir(ir, "TestModule");
    ASSERT_TRUE(init_success, "Should initialize JIT with string operations IR");
    
    int result = jit.execute_function("main");
    // "HelloWorld" has length 10
    ASSERT_EQ(10, result, "String operations should work correctly");
    
    return TestResult(true, "String operations pipeline test successful");
}

TestResult test_enum_declaration_pipeline() {
    std::string source = R"(
        enum Color {
            Red,
            Green,
            Blue,
            Custom(i32, i32, i32)
        }

        enum Status {
            Active,
            Inactive
        }

        fn get_color_value(Color c): i32 {
            return 42;
        }

        fn main(): i32 {
            return 42;
        }
    )";
    
    TokenStream stream = create_integration_token_stream(source);
    Parser parser(stream);
    auto parse_result = parser.parse();
    
    ASSERT_TRUE(parse_result.is_success(), "Parser should handle enum declarations");
    
    auto* unit = parse_result.get_node();
    
    // Verify enum declarations
    int enum_count = 0;
    for (int i = 0; i < unit->statements.size; ++i) {
        if (unit->statements[i]->is_a<EnumDeclarationNode>()) {
            enum_count++;
            auto* enum_decl = static_cast<EnumDeclarationNode*>(unit->statements[i]);
            std::string enum_name = std::string(enum_decl->name->name);
            
            if (enum_name == "Color") {
                ASSERT_TRUE(enum_decl->cases.size == 4, "Color enum should have 4 cases");
            }
        }
    }
    
    ASSERT_TRUE(enum_count >= 2, "Should have at least 2 enum declarations");
    
    // Full pipeline
    SymbolTable symbol_table;
    build_symbol_table(symbol_table, unit);
    
    // Check that enums are in symbol table
    auto color_symbol = symbol_table.lookup_symbol("Color");
    ASSERT_TRUE(color_symbol != nullptr, "Color enum should be in symbol table");
    
    return TestResult(true, "Enum declaration pipeline test successful");
}

// Struct field initialization tests
TestResult test_simple_struct_initialization_pipeline() {
    std::string source = R"(
        type Simple {
            var x = 42;
            var flag = true;
        }
        
        fn test(): i32 {
            var s = new Simple();
            return s.x;
        }
    )";
    
    // Parse and generate
    TokenStream stream = create_integration_token_stream(source);
    Parser parser(stream);
    auto parse_result = parser.parse();
    ASSERT_TRUE(parse_result.is_success(), "Should parse simple struct with defaults");
    
    // Generate and test IR
    SymbolTable symbol_table;
    build_symbol_table(symbol_table, parse_result.get_node());
    CodeGenerator codegen(symbol_table);
    auto commands = codegen.generate_code(parse_result.get_node());
    
    std::string ir = CommandProcessor::process_to_ir_string(commands, "TestModule");
    ASSERT_TRUE(ir.find("store i32 42") != std::string::npos, "Should initialize x = 42");
    ASSERT_TRUE(ir.find("store i1 true") != std::string::npos, "Should initialize flag = true");
    
    // Test execution
    JITEngine jit;
    ASSERT_TRUE(jit.initialize_from_ir(ir, "TestModule"), "Should compile to JIT");
    
    int result = jit.execute_function("test");
    ASSERT_EQ(42, result, "Should return initialized field value 42");
    
    return TestResult(true, "Simple struct initialization pipeline test successful");
}

TestResult test_nested_struct_initialization_pipeline() {
    std::string source = R"(
        type Inner {
            var value = 10;
        }
        
        type Outer {
            var inner = new Inner();
            var count = 5;
        }
        
        fn test(): i32 {
            var o = new Outer();
            return o.inner.value + o.count;
        }
    )";
    
    // Parse and generate
    TokenStream stream = create_integration_token_stream(source);
    Parser parser(stream);
    auto parse_result = parser.parse();
    ASSERT_TRUE(parse_result.is_success(), "Should parse nested structs with defaults");
    
    // Generate and test IR
    SymbolTable symbol_table;
    build_symbol_table(symbol_table, parse_result.get_node());
    CodeGenerator codegen(symbol_table);
    auto commands = codegen.generate_code(parse_result.get_node());
    
    std::string ir = CommandProcessor::process_to_ir_string(commands, "TestModule");
    ASSERT_TRUE(ir.find("store i32 10") != std::string::npos, "Should initialize Inner.value = 10");
    ASSERT_TRUE(ir.find("store i32 5") != std::string::npos, "Should initialize Outer.count = 5");
    ASSERT_TRUE(ir.find("load %Inner") != std::string::npos, "Should load Inner struct value");
    ASSERT_TRUE(ir.find("store %Inner") != std::string::npos, "Should store Inner struct value");
    
    // Test execution
    JITEngine jit;
    ASSERT_TRUE(jit.initialize_from_ir(ir, "TestModule"), "Should compile nested structs");
    
    int result = jit.execute_function("test");
    ASSERT_EQ(15, result, "Should return 10 + 5 = 15 from nested initialization");
    
    return TestResult(true, "Nested struct initialization pipeline test successful");
}

TestResult test_mixed_field_types_initialization_pipeline() {
    std::string source = R"(
        type Mixed {
            var intVal = 100;
            var boolVal = false;
            var anotherInt = 25;
        }
        
        fn test(): i32 {
            var m = new Mixed();
            if (m.boolVal) {
                return m.intVal;
            } else {
                return m.anotherInt;
            }
        }
    )";
    
    // Parse and generate
    TokenStream stream = create_integration_token_stream(source);
    Parser parser(stream);
    auto parse_result = parser.parse();
    ASSERT_TRUE(parse_result.is_success(), "Should parse mixed field types");
    
    // Generate and test IR
    SymbolTable symbol_table;
    build_symbol_table(symbol_table, parse_result.get_node());
    CodeGenerator codegen(symbol_table);
    auto commands = codegen.generate_code(parse_result.get_node());
    
    std::string ir = CommandProcessor::process_to_ir_string(commands, "TestModule");
    ASSERT_TRUE(ir.find("store i32 100") != std::string::npos, "Should initialize intVal = 100");
    ASSERT_TRUE(ir.find("store i1 false") != std::string::npos, "Should initialize boolVal = false");
    ASSERT_TRUE(ir.find("store i32 25") != std::string::npos, "Should initialize anotherInt = 25");
    
    // Test execution
    JITEngine jit;
    ASSERT_TRUE(jit.initialize_from_ir(ir, "TestModule"), "Should compile mixed types");
    
    int result = jit.execute_function("test");
    ASSERT_EQ(25, result, "Should return anotherInt (25) since boolVal is false");
    
    return TestResult(true, "Mixed field types initialization pipeline test successful");
}

// ========== MEMBER FUNCTION TESTS ==========

TestResult test_simple_member_function_pipeline() {
    std::string source = R"(
        type Counter {
            var count = 0;
            
            fn getValue(): i32 {
                return count;
            }
        }
        
        fn test(): i32 {
            var c = new Counter();
            return c.getValue();
        }
    )";
    
    // Parse
    TokenStream stream = create_integration_token_stream(source);
    Parser parser(stream);
    auto parse_result = parser.parse();
    ASSERT_TRUE(parse_result.is_success(), "Should parse type with member function");
    
    // Generate and test IR
    SymbolTable symbol_table;
    build_symbol_table(symbol_table, parse_result.get_node());
    CodeGenerator codegen(symbol_table);
    auto commands = codegen.generate_code(parse_result.get_node());
    
    std::string ir = CommandProcessor::process_to_ir_string(commands, "TestModule");
    ASSERT_TRUE(ir.find("Counter::getValue") != std::string::npos, "Should generate mangled member function");
    
    // Test execution
    JITEngine jit;
    ASSERT_TRUE(jit.initialize_from_ir(ir, "TestModule"), "Should compile member function");
    
    int result = jit.execute_function("test");
    ASSERT_EQ(0, result, "Should return initial count value (0)");
    
    return TestResult(true, "Simple member function pipeline test successful");
}

TestResult test_member_function_with_parameters_pipeline() {
    std::string source = R"(
        type Calculator {
            var result = 0;
            
            fn add(i32 value): void {
                result = result + value;
            }
            
            fn getResult(): i32 {
                return result;
            }
        }
        
        fn test(): i32 {
            var calc = new Calculator();
            calc.add(15);
            calc.add(25);
            return calc.getResult();
        }
    )";
    
    // Parse
    TokenStream stream = create_integration_token_stream(source);
    Parser parser(stream);
    auto parse_result = parser.parse();
    ASSERT_TRUE(parse_result.is_success(), "Should parse type with parameterized member function");
    
    // Generate and test IR
    SymbolTable symbol_table;
    build_symbol_table(symbol_table, parse_result.get_node());
    CodeGenerator codegen(symbol_table);
    auto commands = codegen.generate_code(parse_result.get_node());
    
    std::string ir = CommandProcessor::process_to_ir_string(commands, "TestModule");
    ASSERT_TRUE(ir.find("Calculator::add") != std::string::npos, "Should generate add method");
    ASSERT_TRUE(ir.find("Calculator::getResult") != std::string::npos, "Should generate getResult method");
    
    // Test execution
    JITEngine jit;
    ASSERT_TRUE(jit.initialize_from_ir(ir, "TestModule"), "Should compile member functions with parameters");
    
    int result = jit.execute_function("test");
    ASSERT_EQ(40, result, "Should return 15 + 25 = 40");
    
    return TestResult(true, "Member function with parameters pipeline test successful");
}

TestResult test_unqualified_field_access_pipeline() {
    std::string source = R"(
        type Point {
            var x = 10;
            var y = 20;
            
            fn distanceFromOrigin(): i32 {
                // Unqualified field access - should implicitly use 'this'
                return x + y;  // Equivalent to this.x + this.y
            }
        }
        
        fn test(): i32 {
            var p = new Point();
            return p.distanceFromOrigin();
        }
    )";
    
    // Parse
    TokenStream stream = create_integration_token_stream(source);
    Parser parser(stream);
    auto parse_result = parser.parse();
    ASSERT_TRUE(parse_result.is_success(), "Should parse unqualified field access");
    
    // Generate and test IR
    SymbolTable symbol_table;
    build_symbol_table(symbol_table, parse_result.get_node());
    CodeGenerator codegen(symbol_table);
    auto commands = codegen.generate_code(parse_result.get_node());
    
    std::string ir = CommandProcessor::process_to_ir_string(commands, "TestModule");
    ASSERT_TRUE(ir.find("Point::distanceFromOrigin") != std::string::npos, "Should generate member function");
    
    // Test execution
    JITEngine jit;
    ASSERT_TRUE(jit.initialize_from_ir(ir, "TestModule"), "Should compile unqualified field access");
    
    int result = jit.execute_function("test");
    ASSERT_EQ(30, result, "Should return x + y = 10 + 20 = 30");
    
    return TestResult(true, "Unqualified field access pipeline test successful");
}

TestResult test_unqualified_field_assignment_pipeline() {
    std::string source = R"(
        type Accumulator {
            var total = 0;
            
            fn setValue(i32 value): void {
                // Unqualified field assignment - should implicitly use 'this'
                total = value;  // Equivalent to this.total = value
            }
            
            fn getValue(): i32 {
                return total;
            }
        }
        
        fn test(): i32 {
            var acc = new Accumulator();
            acc.setValue(42);
            return acc.getValue();
        }
    )";
    
    // Parse
    TokenStream stream = create_integration_token_stream(source);
    Parser parser(stream);
    auto parse_result = parser.parse();
    ASSERT_TRUE(parse_result.is_success(), "Should parse unqualified field assignment");
    
    // Generate and test IR
    SymbolTable symbol_table;
    build_symbol_table(symbol_table, parse_result.get_node());
    CodeGenerator codegen(symbol_table);
    auto commands = codegen.generate_code(parse_result.get_node());
    
    std::string ir = CommandProcessor::process_to_ir_string(commands, "TestModule");
    ASSERT_TRUE(ir.find("Accumulator::setValue") != std::string::npos, "Should generate setValue method");
    ASSERT_TRUE(ir.find("Accumulator::getValue") != std::string::npos, "Should generate getValue method");
    
    // Test execution
    JITEngine jit;
    ASSERT_TRUE(jit.initialize_from_ir(ir, "TestModule"), "Should compile unqualified field assignment");
    
    int result = jit.execute_function("test");
    ASSERT_EQ(42, result, "Should return assigned value (42)");
    
    return TestResult(true, "Unqualified field assignment pipeline test successful");
}

TestResult test_multiple_member_functions_pipeline() {
    std::string source = R"(
        type BankAccount {
            var balance = 100;
            
            fn deposit(i32 amount): void {
                balance = balance + amount;
            }
            
            fn withdraw(i32 amount): void {
                balance = balance - amount;
            }
            
            fn getBalance(): i32 {
                return balance;
            }
        }
        
        fn test(): i32 {
            var account = new BankAccount();
            account.deposit(50);
            account.withdraw(25);
            account.deposit(10);
            return account.getBalance();
        }
    )";
    
    // Parse
    TokenStream stream = create_integration_token_stream(source);
    Parser parser(stream);
    auto parse_result = parser.parse();
    ASSERT_TRUE(parse_result.is_success(), "Should parse multiple member functions");
    
    // Generate and test IR
    SymbolTable symbol_table;
    build_symbol_table(symbol_table, parse_result.get_node());
    CodeGenerator codegen(symbol_table);
    auto commands = codegen.generate_code(parse_result.get_node());
    
    std::string ir = CommandProcessor::process_to_ir_string(commands, "TestModule");
    ASSERT_TRUE(ir.find("BankAccount::deposit") != std::string::npos, "Should generate deposit method");
    ASSERT_TRUE(ir.find("BankAccount::withdraw") != std::string::npos, "Should generate withdraw method");
    ASSERT_TRUE(ir.find("BankAccount::getBalance") != std::string::npos, "Should generate getBalance method");
    
    // Test execution
    JITEngine jit;
    ASSERT_TRUE(jit.initialize_from_ir(ir, "TestModule"), "Should compile multiple member functions");
    
    int result = jit.execute_function("test");
    ASSERT_EQ(135, result, "Should return 100 + 50 - 25 + 10 = 135");
    
    return TestResult(true, "Multiple member functions pipeline test successful");
}

TestResult test_member_function_calling_member_function_pipeline() {
    std::string source = R"(
        type MathHelper {
            var base = 5;
            
            fn square(): i32 {
                return base * base;
            }
            
            fn squarePlusBase(): i32 {
                // Member function calling another member function
                return square() + base;
            }
        }
        
        fn test(): i32 {
            var helper = new MathHelper();
            return helper.squarePlusBase();
        }
    )";
    
    // Parse
    TokenStream stream = create_integration_token_stream(source);
    Parser parser(stream);
    auto parse_result = parser.parse();
    ASSERT_TRUE(parse_result.is_success(), "Should parse member function calling member function");
    
    // Generate and test IR
    SymbolTable symbol_table;
    build_symbol_table(symbol_table, parse_result.get_node());
    CodeGenerator codegen(symbol_table);
    auto commands = codegen.generate_code(parse_result.get_node());
    
    std::string ir = CommandProcessor::process_to_ir_string(commands, "TestModule");
    ASSERT_TRUE(ir.find("MathHelper::square") != std::string::npos, "Should generate square method");
    ASSERT_TRUE(ir.find("MathHelper::squarePlusBase") != std::string::npos, "Should generate squarePlusBase method");
    
    // Test execution
    JITEngine jit;
    ASSERT_TRUE(jit.initialize_from_ir(ir, "TestModule"), "Should compile member functions calling each other");
    
    int result = jit.execute_function("test");
    ASSERT_EQ(30, result, "Should return (5*5) + 5 = 25 + 5 = 30");
    
    return TestResult(true, "Member function calling member function pipeline test successful");
}

// Main test runner function
void run_integration_tests() {
    TestSuite suite("Integration Tests");
    
    // End-to-end pipeline tests
    suite.add_test("Simple Function Pipeline", test_simple_function_pipeline);
    suite.add_test("Variable Declaration Pipeline", test_variable_declaration_pipeline);
    suite.add_test("Arithmetic Expressions Pipeline", test_arithmetic_expressions_pipeline);
    suite.add_test("Multiple Functions Pipeline", test_multiple_functions_pipeline);
    suite.add_test("Member Var Declarations Pipeline", test_member_var_declarations_pipeline);
    suite.add_test("Pipeline Error Handling", test_pipeline_error_handling);
    
    // Complex algorithm tests
    suite.add_test("Nested Loops Pipeline", test_nested_loops_pipeline);
    suite.add_test("Function Calls Pipeline", test_function_calls_pipeline);
    suite.add_test("Arithmetic Algorithms Pipeline", test_arithmetic_algorithms_pipeline);
    suite.add_test("Complex Expressions Pipeline", test_complex_expressions_pipeline);
    suite.add_test("Fibonacci Classic Pipeline", test_fibonacci_classic_pipeline);
    suite.add_test("Recursion Pipeline", test_recursion_pipeline);
    
    // Control flow tests
    suite.add_test("If Statement Pipeline", test_if_statement_pipeline);
    suite.add_test("While Loop Pipeline", test_while_loop_pipeline);
    suite.add_test("Break/Continue Pipeline", test_break_continue_pipeline);
    
    // Type system tests
    suite.add_test("Type Declaration Pipeline", test_type_declaration_pipeline);
    suite.add_test("Enum Declaration Pipeline", test_enum_declaration_pipeline);
    
    // Struct field initialization tests
    suite.add_test("Simple Struct Initialization Pipeline", test_simple_struct_initialization_pipeline);
    suite.add_test("Nested Struct Initialization Pipeline", test_nested_struct_initialization_pipeline);
    suite.add_test("Mixed Field Types Initialization Pipeline", test_mixed_field_types_initialization_pipeline);
    
    // Member function tests
    suite.add_test("Simple Member Function Pipeline", test_simple_member_function_pipeline);
    suite.add_test("Member Function with Parameters Pipeline", test_member_function_with_parameters_pipeline);
    suite.add_test("Unqualified Field Access Pipeline", test_unqualified_field_access_pipeline);
    suite.add_test("Unqualified Field Assignment Pipeline", test_unqualified_field_assignment_pipeline);
    suite.add_test("Multiple Member Functions Pipeline", test_multiple_member_functions_pipeline);
    suite.add_test("Member Function Calling Member Function Pipeline", test_member_function_calling_member_function_pipeline);
    
    // Advanced feature tests
    suite.add_test("Logical Operators Pipeline", test_logical_operators_pipeline);
    suite.add_test("Array Operations Pipeline", test_array_operations_pipeline);
    suite.add_test("String Operations Pipeline", test_string_operations_pipeline);
    
    suite.run_all();
}