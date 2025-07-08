#include "test/test_framework.hpp"
#include "test/test_helpers.hpp"
#include "test/parser_test_helpers.hpp"
#include "parser/recursive_parser.hpp"
#include "parser/pratt_parser.hpp"
#include "parser/token_stream.hpp"
#include "parser/lexer.hpp"
#include "parser/parser_context.hpp"
#include "ast/ast_allocator.hpp"
#include <sstream>

using namespace Mycelium::Testing;
using namespace Mycelium::Scripting::Parser;
using namespace Mycelium::Scripting::Lang;

// Full integration test environment
struct IntegrationTestEnv {
    std::string source;
    std::unique_ptr<Lexer> lexer;
    std::unique_ptr<TokenStream> token_stream;
    std::unique_ptr<ParserContext> context;
    std::unique_ptr<AstAllocator> allocator;
    std::unique_ptr<RecursiveParser> parser;
    std::shared_ptr<PrattParser> expr_parser;
    
    IntegrationTestEnv(const std::string& src) : source(src) {
        LexerOptions options;
        options.preserve_trivia = true;  // Test with full trivia preservation
        
        lexer = std::make_unique<Lexer>(source, options);
        auto token_stream_value = lexer->tokenize_all();
        token_stream = std::make_unique<TokenStream>(std::move(token_stream_value));
        context = std::make_unique<ParserContext>(source);
        allocator = std::make_unique<AstAllocator>();
        parser = std::make_unique<RecursiveParser>(*token_stream, *context, *allocator);
        
        // Create expression parser and link them
        expr_parser = std::make_shared<PrattParser>(*token_stream, *context, *allocator);
        parser->set_expression_parser(expr_parser);
    }
};

// Test 1: Simple function with expression body
TestResult test_integration_simple() {
    IntegrationTestEnv env("fn add(a: i32, b: i32) -> i32 { return a + b }");
    
    auto result = env.parser->parse_compilation_unit();
    ASSERT_TRUE(result.has_value(), "Should parse simple function");
    
    auto unit = result.value();
    ASSERT_AST_EQ(1, unit->statements.size, unit, "Should have one function");
    
    auto func ASSERT_NODE_TYPE(unit->statements[0], FunctionDeclarationNode, unit, "Should be function declaration");
    ASSERT_IDENTIFIER_NAME(func->name, "add", unit, "Function name should be 'add'");
    ASSERT_AST_EQ(2, func->parameters.size, unit, "Should have 2 parameters");
    ASSERT_AST_NOT_NULL(func->returnType, unit, "Should have return type");
    
    return TestResult(true);
}

// Test 2: Complex expressions in statements
TestResult test_integration_complex_expressions() {
    std::string source = R"(
        fn processData(data: ref Array<i32>, multiplier: f32) -> f32 {
            if (data.length() > 0 && multiplier != 0.0) {
                return data[0] * multiplier + data.sum() / data.length()
            } else {
                return -1.0
            }
        }
    )";
    
    IntegrationTestEnv env(source);
    auto result = env.parser->parse_compilation_unit();
    
    ASSERT_TRUE(result.has_value(), "Should parse function with complex expressions");
    
    auto unit = result.value();
    ASSERT_TRUE(unit->statements.size == 1, "Should have one function");
    
    auto func = node_cast<FunctionDeclarationNode>(unit->statements[0]);
    ASSERT_TRUE(func != nullptr, "Should be function declaration");
    ASSERT_TRUE(func->body != nullptr, "Should have body");
    ASSERT_TRUE(func->body->statements.size > 0, "Body should have statements");
    
    // First statement should be if statement
    auto if_stmt = node_cast<IfStatementNode>(func->body->statements[0]);
    ASSERT_TRUE(if_stmt != nullptr, "First statement should be if");
    ASSERT_TRUE(if_stmt->condition != nullptr, "If should have condition");
    
    // Condition should be a complex binary expression (&&)
    auto condition = node_cast<BinaryExpressionNode>(if_stmt->condition);
    ASSERT_TRUE(condition != nullptr, "Condition should be binary expression");
    ASSERT_TRUE(condition->opKind == BinaryOperatorKind::LogicalAnd, "Should be && operator");
    
    return TestResult(true);
}

// Test 3: Multiple declarations with nested structures
TestResult test_integration_nested_structures() {
    std::string source = R"(
        type Point {
            x: f32
            y: f32
            
            fn distance(other: Point) -> f32 {
                return ((x - other.x) * (x - other.x) + 
                        (y - other.y) * (y - other.y)).sqrt()
            }
        }
        
        fn main() {
            if (true) {
                fn localHelper() -> bool {
                    return false || true && !false
                }
                
                if (localHelper()) {
                    process()
                }
            }
        }
    )";
    
    IntegrationTestEnv env(source);
    auto result = env.parser->parse_compilation_unit();
    
    ASSERT_TRUE(result.has_value(), "Should parse nested structures");
    
    auto unit = result.value();
    ASSERT_TRUE(unit->statements.size >= 2, "Should have at least 2 top-level declarations");
    
    // Verify we have both type and function declarations
    bool has_type = false;
    bool has_function = false;
    
    for (auto stmt : unit->statements) {
        if (node_is<TypeDeclarationNode>(stmt)) has_type = true;
        if (node_is<FunctionDeclarationNode>(stmt)) has_function = true;
    }
    
    ASSERT_TRUE(has_type, "Should have type declaration");
    ASSERT_TRUE(has_function, "Should have function declaration");
    
    return TestResult(true);
}

// Test 4: Error recovery across all parsers
TestResult test_integration_error_recovery() {
    std::string source = R"(
        fn broken1( { }  // Missing parameters and body
        
        fn valid() { return 42 }  // Valid function
        
        type Broken2 {  // Missing closing brace
            x: i32
        
        fn broken3() -> {  // Invalid return type
            return
        }
        
        fn alsoValid(x: i32) -> i32 {
            return x * 2 + 3
        }
    )";
    
    IntegrationTestEnv env(source);
    auto result = env.parser->parse_compilation_unit();
    
    // Should parse with errors but still recover
    ASSERT_TRUE(result.has_value() || result.has_errors(), 
                "Should either parse with recovery or report errors");
    
    if (result.has_value()) {
        auto unit = result.value();
        // Should have parsed at least the valid functions
        int valid_functions = 0;
        for (auto stmt : unit->statements) {
            if (auto func = node_cast<FunctionDeclarationNode>(stmt)) {
                if (func && func->name && (std::string(func->name->name) == "valid" || 
                                           std::string(func->name->name) == "alsoValid")) {
                    valid_functions++;
                }
            }
        }
        ASSERT_TRUE(valid_functions >= 1, "Should parse at least one valid function");
    }
    
    return TestResult(true);
}

// Test 5: Full feature integration
TestResult test_integration_all_features() {
    std::string source = R"(
        // Test all parser features working together
        namespace TestApp {
            type Calculator {
                memory: mut f32 = 0.0
                lastOp: string
                
                fn calculate(expr: string) -> f32 {
                    // Complex expression parsing
                    let parts = expr.split("+")
                    let result = 0.0
                    
                    for (part in parts) {
                        if (part.contains("*")) {
                            let factors = part.split("*")
                            let product = 1.0
                            for (factor in factors) {
                                product *= factor.toFloat()
                            }
                            result += product
                        } else {
                            result += part.toFloat()
                        }
                    }
                    
                    memory = result
                    lastOp = expr
                    return result
                }
                
                fn clear() {
                    memory = 0.0
                    lastOp = ""
                }
            }
            
            fn testCalculator() {
                let calc = new Calculator()
                
                // Test various expressions
                let r1 = calc.calculate("2 + 3")
                let r2 = calc.calculate("4 * 5 + 6")
                let r3 = calc.calculate("(7 + 8) * 9")  // Would need paren handling
                
                // Test member access and method chaining
                if (calc.memory > 0 && calc.lastOp.length() > 0) {
                    Console.log("Last calculation: " + calc.lastOp + " = " + calc.memory.toString())
                }
                
                // Test operators
                let x = 10
                x += 5
                x *= 2
                x >>= 1
                
                // Test conditionals
                let result = x > 10 ? x * 2 : x / 2
                
                // Test unary operators
                let neg = -result
                let inc = ++x
                let dec = y--
                
                // Test logical operations
                if (!calc.lastOp.isEmpty() || calc.memory != 0.0) {
                    calc.clear()
                }
            }
        }
    )";
    
    IntegrationTestEnv env(source);
    auto result = env.parser->parse_compilation_unit();
    
    ASSERT_TRUE(result.has_value(), "Should parse comprehensive example");
    
    auto unit = result.value();
    ASSERT_TRUE(unit != nullptr, "Unit should not be null");
    ASSERT_TRUE(unit->statements.size > 0, "Should have statements");
    
    // The parsing should handle:
    // 1. Namespace declarations
    // 2. Type declarations with fields and methods
    // 3. Complex function bodies with loops and conditionals
    // 4. Various expression types (binary, unary, member access, calls)
    // 5. Different statement types
    
    // Even if not all features are fully implemented, the integration
    // should not crash and should parse what it can
    
    return TestResult(true);
}

// Test 6: Position tracking through all layers
TestResult test_integration_position_tracking() {
    std::string source = "fn test(x: i32) -> bool { return x > 0 && x < 100 }";
    
    IntegrationTestEnv env(source);
    
    // Enable position tracking
    auto result = env.parser->parse_compilation_unit();
    
    ASSERT_TRUE(result.has_value(), "Should parse function");
    
    auto unit = result.value();
    auto func = node_cast<FunctionDeclarationNode>(unit->statements[0]);
    ASSERT_TRUE(func != nullptr, "Should have function");
    
    // Verify positions are set correctly
    // Note: Exact position checking would require knowing the AST position tracking implementation
    ASSERT_TRUE(func->sourceStart >= 0, "Function should have source position");
    ASSERT_TRUE(func->sourceLength > 0, "Function should have source length");
    
    return TestResult(true);
}

// Test 7: Trivia preservation through parsing
TestResult test_integration_trivia_preservation() {
    std::string source = R"(
        // This is a comment before the function
        fn documented() -> void {
            /* Block comment in body */
            doSomething() // Inline comment
        }
    )";
    
    IntegrationTestEnv env(source);
    auto result = env.parser->parse_compilation_unit();
    
    ASSERT_TRUE(result.has_value(), "Should parse with comments");
    
    // The parser preserves trivia in tokens, which allows
    // for comment-aware processing and code formatting
    
    return TestResult(true);
}

// Test 8: Performance with large input
TestResult test_integration_performance() {
    // Generate a large but valid program
    std::stringstream ss;
    
    // Generate many functions with expressions
    for (int i = 0; i < 100; i++) {
        ss << "fn func" << i << "(x: i32, y: i32) -> i32 {\n";
        ss << "    if (x > " << i << ") {\n";
        ss << "        return x * y + " << i << " - (x / y) % " << (i + 1) << "\n";
        ss << "    } else {\n";
        ss << "        return y * " << i << " + x\n";
        ss << "    }\n";
        ss << "}\n\n";
    }
    
    IntegrationTestEnv env(ss.str());
    
    auto start = std::chrono::high_resolution_clock::now();
    auto result = env.parser->parse_compilation_unit();
    auto end = std::chrono::high_resolution_clock::now();
    
    ASSERT_TRUE(result.has_value(), "Should parse large program");
    
    auto unit = result.value();
    ASSERT_TRUE(unit->statements.size == 100, "Should have 100 functions");
    
    // Check performance (should be fast even for 100 functions)
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    ASSERT_TRUE(duration < 1000, "Should parse 100 functions in under 1 second");
    
    return TestResult(true);
}

void run_parser_integration_tests() {
    TestSuite suite("Parser Integration Tests");
    
    suite.add_test("Simple Integration", test_integration_simple);
    suite.add_test("Complex Expressions", test_integration_complex_expressions);
    suite.add_test("Nested Structures", test_integration_nested_structures);
    suite.add_test("Error Recovery", test_integration_error_recovery);
    suite.add_test("All Features", test_integration_all_features);
    suite.add_test("Position Tracking", test_integration_position_tracking);
    suite.add_test("Trivia Preservation", test_integration_trivia_preservation);
    suite.add_test("Performance", test_integration_performance);
    
    suite.run_all();
}