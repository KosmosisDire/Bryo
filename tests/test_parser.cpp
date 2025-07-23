#include "test/test_framework.hpp"
#include "test/parser_test_helpers.hpp"
#include "parser/parser.h"
#include "parser/lexer.hpp"
#include "parser/token_stream.hpp"

using namespace Mycelium::Testing;
using namespace Mycelium::Scripting::Lang;

// Test diagnostic sink for lexer
class TestLexerDiagnosticSink : public LexerDiagnosticSink {
public:
    std::vector<LexerDiagnostic> diagnostics;
    
    void report_diagnostic(const LexerDiagnostic& diagnostic) override {
        diagnostics.push_back(diagnostic);
    }
    
    void clear() { diagnostics.clear(); }
    
    bool has_errors() const {
        for (const auto& diag : diagnostics) {
            if (diag.is_error) return true;
        }
        return false;
    }
};

// Helper function to create a token stream from source code
TokenStream create_token_stream(const std::string& source) {
    TestLexerDiagnosticSink sink;
    Lexer lexer(source, {}, &sink);
    return lexer.tokenize_all();
}

TestResult test_basic_expression_parsing() {
    std::string source = "42;";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    
    ASSERT_TRUE(result.is_success(), "Parser should successfully parse basic expression");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "CompilationUnitNode should not be null");
    ASSERT_TRUE(unit->statements.size == 1, "Should have exactly one statement");
    
    // Should be an expression statement
    AstNode* stmt_node = unit->statements[0];
    ASSERT_TRUE(ast_is_valid(stmt_node), "Statement should be valid (not an error)");
    
    StatementNode* stmt = ast_cast_or_error<StatementNode>(stmt_node);
    ASSERT_TRUE(stmt != nullptr, "Should be a valid statement");
    ASSERT_TRUE(stmt->is_a<ExpressionStatementNode>(), "Should be an expression statement");
    
    ExpressionStatementNode* expr_stmt = stmt->as<ExpressionStatementNode>();
    ASSERT_TRUE(expr_stmt->expression != nullptr, "Expression should not be null");
    ASSERT_TRUE(ast_is_valid(expr_stmt->expression), "Expression should be valid (not an error)");
    auto* expr = ast_cast_or_error<LiteralExpressionNode>(expr_stmt->expression);
    ASSERT_TRUE(expr != nullptr, "Should be a literal expression");
    
    return TestResult(true, "Basic expression parsing successful");
}

TestResult test_variable_declaration_parsing() {
    std::string source = "var x = 42;";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    
    ASSERT_TRUE(result.is_success(), "Parser should successfully parse variable declaration");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "CompilationUnitNode should not be null");
    ASSERT_TRUE(unit->statements.size == 1, "Should have exactly one statement");
    
    // Should be a local variable declaration
    AstNode* stmt_node = unit->statements[0];
    ASSERT_TRUE(ast_is_valid(stmt_node), "Statement should be valid (not an error)");
    
    StatementNode* stmt = ast_cast_or_error<StatementNode>(stmt_node);
    ASSERT_TRUE(stmt != nullptr, "Should be a valid statement");
    ASSERT_TRUE(stmt->is_a<VariableDeclarationNode>(), "Should be a local variable declaration");
    
    VariableDeclarationNode* declarator = stmt->as<VariableDeclarationNode>();

    ASSERT_TRUE(declarator->name != nullptr, "Variable name should not be null");
    ASSERT_TRUE(declarator->initializer != nullptr, "Variable initializer should not be null");
    ASSERT_TRUE(declarator->varKeyword != nullptr, "var keyword should be stored");
    ASSERT_TRUE(declarator->names.size == 1, "Should have one name in names array");
    ASSERT_TRUE(declarator->names[0] == declarator->name, "name and names[0] should match");
    ASSERT_TRUE(declarator->semicolon != nullptr, "Semicolon should be stored");
    
    return TestResult(true, "Variable declaration parsing successful");
}

TestResult test_multiple_variable_declarations() {
    std::string source = "var x, y, z = 10;";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    
    ASSERT_TRUE(result.is_success(), "Parser should successfully parse multiple variable declaration");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "CompilationUnitNode should not be null");
    ASSERT_TRUE(unit->statements.size == 1, "Should have exactly one statement");
    
    auto* stmt_node = unit->statements[0];
    ASSERT_TRUE(ast_is_valid(stmt_node), "Statement should be valid");
    
    auto* var_decl = ast_cast_or_error<VariableDeclarationNode>(stmt_node);
    ASSERT_TRUE(var_decl != nullptr, "Should be a variable declaration");
    ASSERT_TRUE(var_decl->varKeyword != nullptr, "var keyword should be stored");
    ASSERT_TRUE(var_decl->names.size == 3, "Should have three names in names array");
    ASSERT_TRUE(var_decl->name->name == "x", "First name should be x");
    ASSERT_TRUE(var_decl->names[0]->name == "x", "names[0] should be x");
    ASSERT_TRUE(var_decl->names[1]->name == "y", "names[1] should be y");
    ASSERT_TRUE(var_decl->names[2]->name == "z", "names[2] should be z");
    ASSERT_TRUE(var_decl->initializer != nullptr, "Should have initializer");
    
    return TestResult(true, "Multiple variable declarations parsing successful");
}

TestResult test_typed_variable_declaration() {
    std::string source = "i32 count = 42;";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    
    ASSERT_TRUE(result.is_success(), "Parser should successfully parse typed variable declaration");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "CompilationUnitNode should not be null");
    ASSERT_TRUE(unit->statements.size == 1, "Should have exactly one statement");
    
    auto* stmt_node = unit->statements[0];
    ASSERT_TRUE(ast_is_valid(stmt_node), "Statement should be valid");
    
    auto* var_decl = ast_cast_or_error<VariableDeclarationNode>(stmt_node);
    ASSERT_TRUE(var_decl != nullptr, "Should be a variable declaration");
    ASSERT_TRUE(var_decl->varKeyword == nullptr, "var keyword should be null for typed declaration");
    ASSERT_TRUE(var_decl->type != nullptr, "Type should not be null");
    ASSERT_TRUE(var_decl->names.size == 1, "Should have one name");
    ASSERT_TRUE(var_decl->name->name == "count", "Variable name should be count");
    ASSERT_TRUE(var_decl->initializer != nullptr, "Should have initializer");
    
    return TestResult(true, "Typed variable declaration parsing successful");
}

TestResult test_multiple_typed_variable_declaration() {
    std::string source = "string first, middle, last;";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    
    ASSERT_TRUE(result.is_success(), "Parser should successfully parse multiple typed variable declaration");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "CompilationUnitNode should not be null");
    ASSERT_TRUE(unit->statements.size == 1, "Should have exactly one statement");
    
    auto* stmt_node = unit->statements[0];
    ASSERT_TRUE(ast_is_valid(stmt_node), "Statement should be valid");
    
    auto* var_decl = ast_cast_or_error<VariableDeclarationNode>(stmt_node);
    ASSERT_TRUE(var_decl != nullptr, "Should be a variable declaration");
    ASSERT_TRUE(var_decl->varKeyword == nullptr, "var keyword should be null for typed declaration");
    ASSERT_TRUE(var_decl->type != nullptr, "Type should not be null");
    ASSERT_TRUE(var_decl->names.size == 3, "Should have three names");
    ASSERT_TRUE(var_decl->names[0]->name == "first", "names[0] should be first");
    ASSERT_TRUE(var_decl->names[1]->name == "middle", "names[1] should be middle");
    ASSERT_TRUE(var_decl->names[2]->name == "last", "names[2] should be last");
    ASSERT_TRUE(var_decl->initializer == nullptr, "Should not have initializer");
    
    return TestResult(true, "Multiple typed variable declaration parsing successful");
}

TestResult test_block_statement_parsing() {
    std::string source = "{ var x = 42; var y = 10; }";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    
    ASSERT_TRUE(result.is_success(), "Parser should successfully parse block statement");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "CompilationUnitNode should not be null");
    ASSERT_TRUE(unit->statements.size == 1, "Should have exactly one statement");
    
    // Should be a block statement
    AstNode* stmt_node = unit->statements[0];
    ASSERT_TRUE(ast_is_valid(stmt_node), "Statement should be valid (not an error)");
    
    StatementNode* stmt = ast_cast_or_error<StatementNode>(stmt_node);
    ASSERT_TRUE(stmt != nullptr, "Should be a valid statement");
    ASSERT_TRUE(stmt->is_a<BlockStatementNode>(), "Should be a block statement");
    
    BlockStatementNode* block = stmt->as<BlockStatementNode>();
    ASSERT_TRUE(block->statements.size == 2, "Block should have two statements");
    
    return TestResult(true, "Block statement parsing successful");
}

TestResult test_parser_error_recovery() {
    std::string source = "var x = ; var y = 42;"; // Missing expression after =
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    
    // Parser should still return success but with error nodes and diagnostics
    ASSERT_TRUE(result.is_success(), "Parser should attempt to continue after errors");
    
    // Should have diagnostics
    auto& diagnostics = parser.get_diagnostics();
    ASSERT_TRUE(diagnostics.size() > 0, "Should have error diagnostics");
    
    return TestResult(true, "Parser error recovery successful");
}

TestResult test_empty_program() {
    std::string source = "";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    
    ASSERT_TRUE(result.is_success(), "Parser should successfully parse empty program");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "CompilationUnitNode should not be null");
    ASSERT_TRUE(unit->statements.size == 0, "Empty program should have no statements");
    
    return TestResult(true, "Empty program parsing successful");
}

// Helper to count ErrorNodes in AST
size_t count_errors_in_ast(AstNode* node) {
    if (!node) return 0;
    
    size_t count = 0;
    
    // Check if this node is an error
    if (node->is_a<ErrorNode>()) {
        count++;
    }
    
    // Recursively check children based on node type
    if (auto* unit = node_cast<CompilationUnitNode>(node)) {
        for (int i = 0; i < unit->statements.size; ++i) {
            count += count_errors_in_ast(unit->statements[i]);
        }
    } else if (auto* block = node_cast<BlockStatementNode>(node)) {
        for (int i = 0; i < block->statements.size; ++i) {
            count += count_errors_in_ast(block->statements[i]);
        }
    } else if (auto* expr_stmt = node_cast<ExpressionStatementNode>(node)) {
        count += count_errors_in_ast(expr_stmt->expression);
    } else if (auto* binary = node_cast<BinaryExpressionNode>(node)) {
        count += count_errors_in_ast(binary->left);
        count += count_errors_in_ast(binary->right);
    } else if (auto* var_decl = node_cast<VariableDeclarationNode>(node)) {
        count += count_errors_in_ast(var_decl->initializer);
    }
    // Add more node types as needed
    
    return count;
}

// ==================== COMPREHENSIVE ERROR HANDLING TESTS ====================

TestResult test_missing_semicolon_recovery() {
    std::string source = R"(
        var x = 42
        var y = 100;
        var z = x + y;
    )";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    ASSERT_TRUE(result.is_success(), "Parser should attempt to continue after missing semicolon");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "Should still produce AST");
    
    // Should have diagnostics about the missing semicolon
    auto& diagnostics = parser.get_diagnostics();
    ASSERT_TRUE(diagnostics.size() > 0, "Should have error diagnostics");
    
    return TestResult(true, "Missing semicolon recovery successful");
}

TestResult test_malformed_expression_recovery() {
    std::string source = R"(
        var x = 5 + ;
        var y = 10;
        var z = x + y;
    )";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    ASSERT_TRUE(result.is_success(), "Parser should recover from malformed expression");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "Should still produce AST");
    
    // Should have some statements parsed successfully
    ASSERT_TRUE(unit->statements.size >= 2, "Should parse later statements after error");
    
    // Should have error diagnostics
    auto& diagnostics = parser.get_diagnostics();
    ASSERT_TRUE(diagnostics.size() > 0, "Should have error diagnostics");
    
    // Should have error nodes
    size_t error_count = count_errors_in_ast(unit);
    ASSERT_TRUE(error_count > 0, "Should have error nodes");
    
    return TestResult(true, "Malformed expression recovery successful");
}

TestResult test_multiple_errors_recovery() {
    std::string source = R"(
        var x = 5 +;
        var y = * 10;
        var = 15;
        var z = x + y;
    )";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    ASSERT_TRUE(result.is_success(), "Parser should handle multiple errors");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "Should still produce AST");
    
    // Should have multiple error diagnostics
    auto& diagnostics = parser.get_diagnostics();
    ASSERT_TRUE(diagnostics.size() >= 3, "Should have multiple error diagnostics");
    
    return TestResult(true, "Multiple errors recovery successful");
}

TestResult test_ast_helper_functions() {
    std::string source = "var x = 5 +;"; // Malformed expression
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "Should have AST");
    
    // Test type-safe helper functions
    for (int i = 0; i < unit->statements.size; ++i) {
        AstNode* stmt_node = unit->statements[i];
        
        // Test ast_is_valid
        bool is_valid = ast_is_valid(stmt_node);
        bool is_error = stmt_node && stmt_node->is_a<ErrorNode>();
        ASSERT_TRUE(is_valid != is_error, "ast_is_valid should be opposite of is_a<ErrorNode>");
        
        // Test ast_cast_or_error
        auto* as_statement = ast_cast_or_error<StatementNode>(stmt_node);
        if (is_valid) {
            ASSERT_TRUE(as_statement != nullptr, "Valid nodes should cast successfully");
        } else {
            ASSERT_TRUE(as_statement == nullptr, "Error nodes should not cast to other types");
        }
        
        // Test ast_has_errors
        bool has_errors = ast_has_errors(stmt_node);
        if (is_error) {
            ASSERT_TRUE(has_errors, "Error nodes should report has_errors = true");
        }
    }
    
    return TestResult(true, "AST helper functions working correctly");
}

// ==================== COMPREHENSIVE EXPRESSION TESTS ====================

TestResult test_complex_expressions() {
    std::string source = "x + y * z - (a / b) % c;";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    ASSERT_TRUE(result.is_success(), "Should parse complex expression");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "CompilationUnitNode should not be null");
    ASSERT_TRUE(unit->statements.size == 1, "Should have one statement");
    
    AstNode* stmt_node = unit->statements[0];
    ASSERT_TRUE(ast_is_valid(stmt_node), "Statement should be valid");
    
    auto* stmt = ast_cast_or_error<ExpressionStatementNode>(stmt_node);
    ASSERT_TRUE(stmt != nullptr, "Should be expression statement");
    ASSERT_TRUE(stmt->expression != nullptr, "Should have expression");
    
    return TestResult(true, "Complex expressions parsing successful");
}

TestResult test_literal_types() {
    std::string source = R"(
        42;
        "hello world";
        true;
        false;
    )";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    ASSERT_TRUE(result.is_success(), "Should parse all literal types");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "CompilationUnitNode should not be null");
    ASSERT_TRUE(unit->statements.size == 4, "Should have 4 statements");
    
    // Check each statement is a valid expression statement
    for (int i = 0; i < unit->statements.size; ++i) {
        AstNode* stmt_node = unit->statements[i];
        ASSERT_TRUE(ast_is_valid(stmt_node), "Each statement should be valid");
        
        auto* stmt = ast_cast_or_error<ExpressionStatementNode>(stmt_node);
        ASSERT_TRUE(stmt != nullptr, "Should be expression statement");
        ASSERT_TRUE(stmt->expression != nullptr, "Should have expression");
        auto* literal = ast_cast_or_error<LiteralExpressionNode>(stmt->expression);
        ASSERT_TRUE(literal != nullptr, "Should be literal");
    }
    
    return TestResult(true, "All literal types parsing successful");
}

// ==================== COMPREHENSIVE STATEMENT TESTS ====================

TestResult test_complex_variable_declarations() {
    std::string source = R"(
        var x = 42;
        var y = 100;
        var name = "test";
        var result = x + y * 2;
    )";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    ASSERT_TRUE(result.is_success(), "Should parse complex variable declarations");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "CompilationUnitNode should not be null");
    ASSERT_TRUE(unit->statements.size == 4, "Should have 4 statements");
    
    // Verify all are local variable declarations
    for (int i = 0; i < unit->statements.size; ++i) {
        AstNode* stmt_node = unit->statements[i];
        ASSERT_TRUE(ast_is_valid(stmt_node), "Each statement should be valid");
        
        auto* stmt = ast_cast_or_error<VariableDeclarationNode>(stmt_node);
        ASSERT_TRUE(stmt != nullptr, "Should be local variable declaration");
        ASSERT_TRUE(stmt->names.size == 1, "Should have one identifier");
    }
    
    return TestResult(true, "Complex variable declarations parsing successful");
}

TestResult test_nested_blocks() {
    std::string source = R"(
        {
            var x = 1;
            {
                var y = 2;
                {
                    var z = x + y;
                }
            }
        }
    )";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    ASSERT_TRUE(result.is_success(), "Should parse nested blocks");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "CompilationUnitNode should not be null");
    ASSERT_TRUE(unit->statements.size == 1, "Should have one top-level statement");
    
    auto* block = ast_cast_or_error<BlockStatementNode>(unit->statements[0]);
    ASSERT_TRUE(block != nullptr, "Should be block statement");
    ASSERT_TRUE(block->statements.size == 2, "Outer block should have 2 statements");
    
    return TestResult(true, "Nested blocks parsing successful");
}

// ==================== STRESS TESTS ====================

TestResult test_large_expression() {
    // Create a large expression with many operators
    std::string source = "var result = ";
    for (int i = 0; i < 20; ++i) {
        if (i > 0) source += " + ";
        source += "x" + std::to_string(i);
    }
    source += ";";
    
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    ASSERT_TRUE(result.is_success(), "Should parse large expression");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "Should have AST");
    ASSERT_TRUE(unit->statements.size == 1, "Should have one statement");
    
    // Verify no errors
    size_t error_count = count_errors_in_ast(unit);
    ASSERT_TRUE(error_count == 0, "Should have no errors in large expression");
    
    return TestResult(true, "Large expression parsing successful");
}

TestResult test_many_statements() {
    std::string source = "";
    int count = 50;
    
    // Create many variable declarations
    for (int i = 0; i < count; ++i) {
        source += "var x" + std::to_string(i) + " = " + std::to_string(i) + ";\n";
    }
    
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    ASSERT_TRUE(result.is_success(), "Should parse many statements");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "Should have AST");
    ASSERT_TRUE(unit->statements.size == count, "Should have all statements");
    
    // Verify no errors
    size_t error_count = count_errors_in_ast(unit);
    ASSERT_TRUE(error_count == 0, "Should have no errors in many statements");
    
    return TestResult(true, "Many statements parsing successful");
}

// ==================== EDGE CASES ====================

TestResult test_empty_blocks() {
    std::string source = R"(
        {}
        { }
        {
        
        }
    )";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    ASSERT_TRUE(result.is_success(), "Should parse empty blocks");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "Should have AST");
    ASSERT_TRUE(unit->statements.size == 3, "Should have 3 empty blocks");
    
    for (int i = 0; i < unit->statements.size; ++i) {
        auto* block = ast_cast_or_error<BlockStatementNode>(unit->statements[i]);
        ASSERT_TRUE(block != nullptr, "Should be block statement");
        ASSERT_TRUE(block->statements.size == 0, "Should be empty block");
    }
    
    return TestResult(true, "Empty blocks parsing successful");
}

TestResult test_whitespace_handling() {
    std::string source = R"(
    
        var x = 42;    
        
        
        var y = 100;
        
    )";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    ASSERT_TRUE(result.is_success(), "Should handle whitespace gracefully");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "Should have AST");
    ASSERT_TRUE(unit->statements.size == 2, "Should have 2 statements despite whitespace");
    
    return TestResult(true, "Whitespace handling successful");
}

// ==================== ADVANCED OPERATOR PRECEDENCE TESTS ====================

TestResult test_operator_precedence() {
    std::string source = "var result = 2 + 3 * 4 - 1;"; // Should be (2 + (3 * 4) - 1) = 13
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    ASSERT_TRUE(result.is_success(), "Should parse operator precedence correctly");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "Should have AST");
    ASSERT_TRUE(unit->statements.size == 1, "Should have one statement");
    
    auto* var_decl = ast_cast_or_error<VariableDeclarationNode>(unit->statements[0]);
    ASSERT_TRUE(var_decl != nullptr, "Should be variable declaration");
    ASSERT_TRUE(var_decl->names.size == 1, "Should have one identifier");
    
    ASSERT_TRUE(var_decl->initializer != nullptr, "Should have initializer");
    
    // The initializer should be a complex binary expression tree
    auto* expr = ast_cast_or_error<BinaryExpressionNode>(var_decl->initializer);
    ASSERT_TRUE(expr != nullptr, "Should be binary expression");
    
    return TestResult(true, "Operator precedence parsing successful");
}

TestResult test_comparison_operators() {
    std::string source = R"(
        var a = x == y;
        var b = x != y;
        var c = x < y;
        var d = x <= y;
        var e = x > y;
        var f = x >= y;
    )";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    ASSERT_TRUE(result.is_success(), "Should parse comparison operators");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "Should have AST");
    ASSERT_TRUE(unit->statements.size == 6, "Should have 6 statements");
    
    // Verify each comparison operator
    BinaryOperatorKind expected_ops[] = {
        BinaryOperatorKind::Equals,
        BinaryOperatorKind::NotEquals,
        BinaryOperatorKind::LessThan,
        BinaryOperatorKind::LessThanOrEqual,
        BinaryOperatorKind::GreaterThan,
        BinaryOperatorKind::GreaterThanOrEqual
    };
    
    for (int i = 0; i < unit->statements.size; ++i) {
        auto* var_decl = ast_cast_or_error<VariableDeclarationNode>(unit->statements[i]);
        ASSERT_TRUE(var_decl != nullptr, "Should be variable declaration");

        auto* binary_expr = ast_cast_or_error<BinaryExpressionNode>(var_decl->initializer);
        ASSERT_TRUE(binary_expr != nullptr, "Should be binary expression");
        ASSERT_TRUE(binary_expr->opKind == expected_ops[i], "Should have correct operator");
    }
    
    return TestResult(true, "Comparison operators parsing successful");
}

TestResult test_logical_operators() {
    std::string source = R"(
        var and_result = a && b;
        var or_result = c || d;
        var complex = a && b || c && d;
    )";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    ASSERT_TRUE(result.is_success(), "Should parse logical operators");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "Should have AST");
    ASSERT_TRUE(unit->statements.size == 3, "Should have 3 statements");
    
    // Check logical AND
    auto* and_var = ast_cast_or_error<VariableDeclarationNode>(unit->statements[0]);
    auto* and_expr = ast_cast_or_error<BinaryExpressionNode>(and_var->initializer);
    ASSERT_TRUE(and_expr->opKind == BinaryOperatorKind::LogicalAnd, "Should be logical AND");
    
    // Check logical OR
    auto* or_var = ast_cast_or_error<VariableDeclarationNode>(unit->statements[1]);
    auto* or_expr = ast_cast_or_error<BinaryExpressionNode>(or_var->initializer);
    ASSERT_TRUE(or_expr->opKind == BinaryOperatorKind::LogicalOr, "Should be logical OR");
    
    return TestResult(true, "Logical operators parsing successful");
}

TestResult test_parenthesized_expressions() {
    std::string source = "var result = (2 + 3) * (4 - 1);";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    ASSERT_TRUE(result.is_success(), "Should parse parenthesized expressions");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "Should have AST");
    ASSERT_TRUE(unit->statements.size == 1, "Should have one statement");
    
    auto* var_decl = ast_cast_or_error<VariableDeclarationNode>(unit->statements[0]);
    auto* mult_expr = ast_cast_or_error<BinaryExpressionNode>(var_decl->initializer);
    ASSERT_TRUE(mult_expr != nullptr, "Should be binary expression");
    ASSERT_TRUE(mult_expr->opKind == BinaryOperatorKind::Multiply, "Should be multiplication");
    
    return TestResult(true, "Parenthesized expressions parsing successful");
}

// ==================== CONTROL FLOW STATEMENT TESTS ====================

TestResult test_if_statement_parsing() {
    std::string source = R"(
        if (x > 0) {
            var positive = true;
        }
    )";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    ASSERT_TRUE(result.is_success(), "Should parse if statement");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "Should have AST");
    ASSERT_TRUE(unit->statements.size == 1, "Should have one statement");
    
    auto* if_stmt = ast_cast_or_error<IfStatementNode>(unit->statements[0]);
    ASSERT_TRUE(if_stmt != nullptr, "Should be if statement");
    ASSERT_TRUE(if_stmt->condition != nullptr, "Should have condition");
    ASSERT_TRUE(if_stmt->thenStatement != nullptr, "Should have then statement");
    ASSERT_TRUE(if_stmt->elseStatement == nullptr, "Should not have else statement");
    
    // Verify condition is a comparison
    auto* condition = ast_cast_or_error<BinaryExpressionNode>(if_stmt->condition);
    ASSERT_TRUE(condition != nullptr, "Condition should be binary expression");
    ASSERT_TRUE(condition->opKind == BinaryOperatorKind::GreaterThan, "Should be greater than");
    
    // Verify then statement is a block
    auto* then_block = ast_cast_or_error<BlockStatementNode>(if_stmt->thenStatement);
    ASSERT_TRUE(then_block != nullptr, "Then statement should be block");
    ASSERT_TRUE(then_block->statements.size == 1, "Block should have one statement");
    
    return TestResult(true, "If statement parsing successful");
}

TestResult test_while_statement_parsing() {
    std::string source = R"(
        while (count < 10) {
            var temp = count + 1;
        }
    )";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    ASSERT_TRUE(result.is_success(), "Should parse while statement");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "Should have AST");
    ASSERT_TRUE(unit->statements.size == 1, "Should have one statement");
    
    auto* while_stmt = ast_cast_or_error<WhileStatementNode>(unit->statements[0]);
    ASSERT_TRUE(while_stmt != nullptr, "Should be while statement");
    ASSERT_TRUE(while_stmt->condition != nullptr, "Should have condition");
    ASSERT_TRUE(while_stmt->body != nullptr, "Should have body");
    
    // Verify condition is a comparison
    auto* condition = ast_cast_or_error<BinaryExpressionNode>(while_stmt->condition);
    ASSERT_TRUE(condition != nullptr, "Condition should be binary expression");
    ASSERT_TRUE(condition->opKind == BinaryOperatorKind::LessThan, "Should be less than");
    
    // Verify body is a block
    auto* body_block = ast_cast_or_error<BlockStatementNode>(while_stmt->body);
    ASSERT_TRUE(body_block != nullptr, "Body should be block");
    ASSERT_TRUE(body_block->statements.size == 1, "Block should have one statement");
    
    return TestResult(true, "While statement parsing successful");
}

TestResult test_for_statement_parsing() {
    std::string source = R"(
        for (var i = 0; i < 10; i++) {
            var temp = i * 2;
        }
    )";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    ASSERT_TRUE(result.is_success(), "Should parse for statement");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "Should have AST");
    
    // Verify the statement is a ForStatement
    bool found_for_statement = false;
    for (int i = 0; i < unit->statements.size; ++i) {
        auto* stmt = unit->statements[i];
        if (stmt->is_a<ForStatementNode>()) {
            found_for_statement = true;
            break;
        }
    }
    ASSERT_TRUE(found_for_statement, "Should contain a ForStatementNode");
    
    ASSERT_TRUE(unit->statements.size == 1, "Should have one statement");
    
    auto* for_stmt = ast_cast_or_error<ForStatementNode>(unit->statements[0]);
    ASSERT_TRUE(for_stmt != nullptr, "Should be for statement");
    
    return TestResult(true, "For statement parsing successful");
}

TestResult test_for_statement_variations() {
    // Test simple expression initializer
    std::string source = R"(
        for (i = 0; i < 10; i++) {
            var temp = i;
        }
    )";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    ASSERT_TRUE(result.is_success(), "Should parse for statement with expression initializer");
    
    CompilationUnitNode* unit = result.get_node();
    
    // Verify we have exactly one statement that is a ForStatement
    ASSERT_TRUE(unit->statements.size == 1, "Should have exactly one statement");
    ASSERT_TRUE(unit->statements[0]->is_a<ForStatementNode>(), "Should be a ForStatementNode");
    
    auto* for_stmt = ast_cast_or_error<ForStatementNode>(unit->statements[0]);
    ASSERT_TRUE(for_stmt != nullptr, "Should be for statement");
    
    return TestResult(true, "For statement variations parsing successful");
}

TestResult test_nested_control_flow() {
    std::string source = R"(
        if (x > 0) {
            while (y < 10) {
                if (z == 0) {
                    var found = true;
                }
                var temp = y + 1;
            }
        }
    )";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    ASSERT_TRUE(result.is_success(), "Should parse nested control flow");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "Should have AST");
    ASSERT_TRUE(unit->statements.size == 1, "Should have one statement");
    
    auto* outer_if = ast_cast_or_error<IfStatementNode>(unit->statements[0]);
    ASSERT_TRUE(outer_if != nullptr, "Should be if statement");
    
    auto* if_body = ast_cast_or_error<BlockStatementNode>(outer_if->thenStatement);
    ASSERT_TRUE(if_body != nullptr, "If body should be block");
    ASSERT_TRUE(if_body->statements.size == 1, "Should have one statement in if body");
    
    auto* while_stmt = ast_cast_or_error<WhileStatementNode>(if_body->statements[0]);
    ASSERT_TRUE(while_stmt != nullptr, "Should have while statement inside if");
    
    auto* while_body = ast_cast_or_error<BlockStatementNode>(while_stmt->body);
    ASSERT_TRUE(while_body != nullptr, "While body should be block");
    ASSERT_TRUE(while_body->statements.size == 2, "While body should have 2 statements");
    
    auto* inner_if = ast_cast_or_error<IfStatementNode>(while_body->statements[0]);
    ASSERT_TRUE(inner_if != nullptr, "Should have nested if statement");
    
    return TestResult(true, "Nested control flow parsing successful");
}

// ==================== ADVANCED ERROR RECOVERY TESTS ====================

TestResult test_malformed_if_statement_recovery() {
    std::string source = R"(
        if x > 0 {  // Missing parentheses
            var a = 1;
        }
        var b = 2;  // Should still parse this
    )";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    ASSERT_TRUE(result.is_success(), "Should recover from malformed if statement");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "Should have AST");
    
    // Should have error diagnostics
    auto& diagnostics = parser.get_diagnostics();
    ASSERT_TRUE(diagnostics.size() > 0, "Should have error diagnostics");
    
    // Should still parse the valid statement after the error
    ASSERT_TRUE(unit->statements.size >= 1, "Should have at least one statement");
    
    return TestResult(true, "Malformed if statement recovery successful");
}

TestResult test_unclosed_block_recovery() {
    std::string source = R"(
        {
            var x = 1;
            var y = 2;
        // Missing closing brace
        var z = 3;
    )";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    ASSERT_TRUE(result.is_success(), "Should recover from unclosed block");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "Should have AST");
    
    // Should have error diagnostics
    auto& diagnostics = parser.get_diagnostics();
    ASSERT_TRUE(diagnostics.size() > 0, "Should have error diagnostics");
    
    return TestResult(true, "Unclosed block recovery successful");
}

TestResult test_invalid_variable_name_recovery() {
    std::string source = R"(
        var 123invalid = 5;  // Invalid variable name
        var valid = 10;      // Should still parse this
        var = 15;            // Missing name
        var another = 20;    // Should still parse this
    )";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    ASSERT_TRUE(result.is_success(), "Should recover from invalid variable names");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "Should have AST");
    
    // Should have multiple error diagnostics
    auto& diagnostics = parser.get_diagnostics();
    ASSERT_TRUE(diagnostics.size() >= 2, "Should have multiple error diagnostics");
    
    // Should still have some valid statements
    ASSERT_TRUE(unit->statements.size >= 2, "Should have at least some statements");
    
    return TestResult(true, "Invalid variable name recovery successful");
}

// ==================== COMPREHENSIVE STRESS TESTS ====================

TestResult test_deeply_nested_expressions() {
    std::string source = "var result = ((((1 + 2) * 3) - 4) / 5) + ((6 * 7) - (8 + 9));";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    ASSERT_TRUE(result.is_success(), "Should parse deeply nested expressions");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "Should have AST");
    ASSERT_TRUE(unit->statements.size == 1, "Should have one statement");
    
    // Verify no errors in the complex expression
    size_t error_count = count_errors_in_ast(unit);
    ASSERT_TRUE(error_count == 0, "Should have no errors in nested expression");
    
    return TestResult(true, "Deeply nested expressions parsing successful");
}

TestResult test_mixed_statement_types() {
    std::string source = R"(
        var x = 10;
        {
            var y = 20;
            if (x < y) {
                var diff = y - x;
                while (diff > 0) {
                    diff = diff - 1;
                }
            }
        }
        var final = x + 5;
    )";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    ASSERT_TRUE(result.is_success(), "Should parse mixed statement types");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "Should have AST");
    ASSERT_TRUE(unit->statements.size == 3, "Should have 3 top-level statements");
    
    // Verify the types of statements
    ASSERT_TRUE(unit->statements[0]->is_a<VariableDeclarationNode>(), "First should be variable declaration");
    ASSERT_TRUE(unit->statements[1]->is_a<BlockStatementNode>(), "Second should be block statement");
    ASSERT_TRUE(unit->statements[2]->is_a<VariableDeclarationNode>(), "Third should be variable declaration");
    
    // Verify no errors
    size_t error_count = count_errors_in_ast(unit);
    ASSERT_TRUE(error_count == 0, "Should have no errors in mixed statements");
    
    return TestResult(true, "Mixed statement types parsing successful");
}

TestResult test_performance_many_binary_operations() {
    // Create a very long chain of binary operations
    std::string source = "var result = a";
    for (int i = 0; i < 100; ++i) {
        source += " + b" + std::to_string(i);
    }
    source += ";";
    
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    ASSERT_TRUE(result.is_success(), "Should parse many binary operations efficiently");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "Should have AST");
    ASSERT_TRUE(unit->statements.size == 1, "Should have one statement");
    
    // Verify no errors
    size_t error_count = count_errors_in_ast(unit);
    ASSERT_TRUE(error_count == 0, "Should have no errors in long chain");
    
    return TestResult(true, "Many binary operations parsing successful");
}

// ==================== EDGE CASE TESTS ====================

TestResult test_empty_parentheses() {
    std::string source = "var x = ();"; // Empty parentheses - should be an error
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    ASSERT_TRUE(result.is_success(), "Parser should recover from empty parentheses");
    
    // Should have error diagnostics
    auto& diagnostics = parser.get_diagnostics();
    ASSERT_TRUE(diagnostics.size() > 0, "Should have error diagnostics for empty parentheses");
    
    return TestResult(true, "Empty parentheses handling successful");
}

TestResult test_malformed_operators() {
    std::string source = R"(
        var a = x +;
        var b = * y;
        var c = z /;
    )";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    ASSERT_TRUE(result.is_success(), "Should recover from malformed operators");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "Should have AST");
    
    // Should have multiple error diagnostics
    auto& diagnostics = parser.get_diagnostics();
    ASSERT_TRUE(diagnostics.size() >= 3, "Should have multiple error diagnostics");
    
    return TestResult(true, "Malformed operators recovery successful");
}

// ==================== NEW EXPRESSION TYPE TESTS ====================

TestResult test_assignment_expressions() {
    std::string source = R"(
        x = 42;
        y = x + 10;
        z = a = b;  // Right-associative
    )";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    ASSERT_TRUE(result.is_success(), "Should parse assignment expressions");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "Should have AST");
    ASSERT_TRUE(unit->statements.size == 3, "Should have 3 statements");
    
    // Check first assignment: x = 42;
    auto* first_stmt = ast_cast_or_error<ExpressionStatementNode>(unit->statements[0]);
    ASSERT_TRUE(first_stmt != nullptr, "Should be expression statement");
    auto* first_assign = ast_cast_or_error<AssignmentExpressionNode>(first_stmt->expression);
    ASSERT_TRUE(first_assign != nullptr, "Should be assignment expression");
    ASSERT_TRUE(first_assign->opKind == AssignmentOperatorKind::Assign, "Should be simple assignment");
    
    // Check target is identifier
    auto* target = ast_cast_or_error<IdentifierExpressionNode>(first_assign->target);
    ASSERT_TRUE(target != nullptr, "Target should be identifier");
    
    // Check source is literal
    auto* source_literal = ast_cast_or_error<LiteralExpressionNode>(first_assign->source);
    ASSERT_TRUE(source_literal != nullptr, "Source should be literal");
    
    return TestResult(true, "Assignment expressions parsing successful");
}

TestResult test_unary_expressions() {
    std::string source = R"(
        var a = !true;
        var b = -42;
        var c = ++count;
        var d = --index;
    )";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    ASSERT_TRUE(result.is_success(), "Should parse unary expressions");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "Should have AST");
    ASSERT_TRUE(unit->statements.size == 4, "Should have 4 statements");
    
    // Check logical NOT: !true
    auto* first_var = ast_cast_or_error<VariableDeclarationNode>(unit->statements[0]);
    auto* first_unary = ast_cast_or_error<UnaryExpressionNode>(first_var->initializer);
    ASSERT_TRUE(first_unary != nullptr, "Should be unary expression");
    ASSERT_TRUE(first_unary->opKind == UnaryOperatorKind::Not, "Should be logical NOT");
    ASSERT_TRUE(first_unary->isPostfix == false, "Should be prefix");
    
    // Check negation: -42
    auto* second_var = ast_cast_or_error<VariableDeclarationNode>(unit->statements[1]);
    auto* second_unary = ast_cast_or_error<UnaryExpressionNode>(second_var->initializer);
    ASSERT_TRUE(second_unary != nullptr, "Should be unary expression");
    ASSERT_TRUE(second_unary->opKind == UnaryOperatorKind::Minus, "Should be negation");
    
    return TestResult(true, "Unary expressions parsing successful");
}

TestResult test_call_expressions() {
    std::string source = R"(
        func();
        method(arg1, arg2);
        nested(outer(inner()));
    )";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    ASSERT_TRUE(result.is_success(), "Should parse call expressions");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "Should have AST");
    ASSERT_TRUE(unit->statements.size == 3, "Should have 3 statements");
    
    // Check simple call: func()
    auto* first_stmt = ast_cast_or_error<ExpressionStatementNode>(unit->statements[0]);
    auto* first_call = ast_cast_or_error<CallExpressionNode>(first_stmt->expression);
    ASSERT_TRUE(first_call != nullptr, "Should be call expression");
    ASSERT_TRUE(first_call->arguments.size == 0, "Should have no arguments");
    
    auto* target = ast_cast_or_error<IdentifierExpressionNode>(first_call->target);
    ASSERT_TRUE(target != nullptr, "Target should be identifier");
    
    // Check call with arguments: method(arg1, arg2)
    auto* second_stmt = ast_cast_or_error<ExpressionStatementNode>(unit->statements[1]);
    auto* second_call = ast_cast_or_error<CallExpressionNode>(second_stmt->expression);
    ASSERT_TRUE(second_call != nullptr, "Should be call expression");
    ASSERT_TRUE(second_call->arguments.size == 2, "Should have 2 arguments");
    
    return TestResult(true, "Call expressions parsing successful");
}

TestResult test_member_access_expressions() {
    std::string source = R"(
        obj.field;
        obj.method().property;
        deeply.nested.member.access;
    )";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    ASSERT_TRUE(result.is_success(), "Should parse member access expressions");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "Should have AST");
    ASSERT_TRUE(unit->statements.size == 3, "Should have 3 statements");
    
    // Check simple member access: obj.field
    auto* first_stmt = ast_cast_or_error<ExpressionStatementNode>(unit->statements[0]);
    auto* first_member = ast_cast_or_error<MemberAccessExpressionNode>(first_stmt->expression);
    ASSERT_TRUE(first_member != nullptr, "Should be member access expression");
    
    auto* target = ast_cast_or_error<IdentifierExpressionNode>(first_member->target);
    ASSERT_TRUE(target != nullptr, "Target should be identifier");
    ASSERT_TRUE(first_member->member != nullptr, "Should have member name");
    
    // Check chained access: deeply.nested.member.access
    auto* third_stmt = ast_cast_or_error<ExpressionStatementNode>(unit->statements[2]);
    auto* third_member = ast_cast_or_error<MemberAccessExpressionNode>(third_stmt->expression);
    ASSERT_TRUE(third_member != nullptr, "Should be member access expression");
    
    // The target should be another member access (nested)
    auto* nested_target = ast_cast_or_error<MemberAccessExpressionNode>(third_member->target);
    ASSERT_TRUE(nested_target != nullptr, "Target should be nested member access");
    
    return TestResult(true, "Member access expressions parsing successful");
}

TestResult test_complex_postfix_chaining() {
    std::string source = R"(
        obj.method(arg1, arg2).property[index].anotherMethod();
        array[0][1][2];
        func()(x)(y);
    )";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    ASSERT_TRUE(result.is_success(), "Should parse complex postfix chaining");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "Should have AST");
    ASSERT_TRUE(unit->statements.size == 3, "Should have 3 statements");
    
    // Verify the complex chain parses without errors
    size_t error_count = count_errors_in_ast(unit);
    ASSERT_TRUE(error_count == 0, "Should have no errors in complex postfix expressions");
    
    // Check first statement structure: obj.method(arg1, arg2).property[index].anotherMethod()
    auto* first_stmt = ast_cast_or_error<ExpressionStatementNode>(unit->statements[0]);
    ASSERT_TRUE(first_stmt != nullptr, "Should be expression statement");
    auto* final_call = ast_cast_or_error<CallExpressionNode>(first_stmt->expression);
    ASSERT_TRUE(final_call != nullptr, "Final operation should be call");
    
    return TestResult(true, "Complex postfix chaining parsing successful");
}

TestResult test_expression_precedence_with_new_operators() {
    std::string source = R"(
        var a = !x && y;           // !x && y
        var b = x = y + z;         // x = (y + z)
        var c = -a.field + b();    // ((-a.field) + b())
    )";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    ASSERT_TRUE(result.is_success(), "Should parse expressions with correct precedence");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "Should have AST");
    ASSERT_TRUE(unit->statements.size == 3, "Should have 3 statements");
    
    // Verify precedence is handled correctly (no errors in complex expressions)
    size_t error_count = count_errors_in_ast(unit);
    ASSERT_TRUE(error_count == 0, "Should have no errors in precedence expressions");
    
    // Check assignment with binary expression: x = y + z
    auto* second_var = ast_cast_or_error<VariableDeclarationNode>(unit->statements[1]);
    auto* assignment = ast_cast_or_error<AssignmentExpressionNode>(second_var->initializer);
    ASSERT_TRUE(assignment != nullptr, "Should be assignment expression");
    
    // The source should be a binary expression (y + z)
    auto* binary_source = ast_cast_or_error<BinaryExpressionNode>(assignment->source);
    ASSERT_TRUE(binary_source != nullptr, "Assignment source should be binary expression");
    
    return TestResult(true, "Expression precedence with new operators successful");
}

// ===== DECLARATION PARSER TESTS =====

TestResult test_function_declaration_parsing() {
    std::string source = "fn test() { }";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    
    ASSERT_TRUE(result.is_success(), "Parser should successfully parse function declaration");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "CompilationUnitNode should not be null");
    ASSERT_TRUE(unit->statements.size == 1, "Should have exactly one statement");
    
    // Should be a function declaration
    AstNode* stmt_node = unit->statements[0];
    ASSERT_TRUE(stmt_node->is_a<FunctionDeclarationNode>(), "Should be a FunctionDeclarationNode");
    
    FunctionDeclarationNode* func = stmt_node->as<FunctionDeclarationNode>();
    ASSERT_TRUE(func->name != nullptr, "Function should have a name");
    ASSERT_TRUE(std::string(func->name->name) == "test", "Function name should be 'test'");
    ASSERT_TRUE(func->body != nullptr, "Function should have a body");
    
    return TestResult(true, "Function declaration parsing successful");
}

TestResult test_function_with_parameters() {
    std::string source = "fn add(i32 x, i32 y) { }";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    
    // Debug: Check if parsing succeeded at all
    if (!result.is_success()) {
        return TestResult(false, "Parser failed to parse function with parameters");
    }
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "CompilationUnitNode should not be null");
    
    // Debug: Print number of statements
    if (unit->statements.size != 1) {
        return TestResult(false, "Expected 1 statement, got " + std::to_string(unit->statements.size));
    }
    
    AstNode* stmt_node = unit->statements[0];
    ASSERT_TRUE(stmt_node->is_a<FunctionDeclarationNode>(), "Should be a FunctionDeclarationNode");
    
    FunctionDeclarationNode* func = stmt_node->as<FunctionDeclarationNode>();
    ASSERT_TRUE(std::string(func->name->name) == "add", "Function name should be 'add'");
    
    // Verify that parameters were actually parsed
    ASSERT_TRUE(func->parameters.size == 2, "Function should have 2 parameters, got " + std::to_string(func->parameters.size));
    
    // Check first parameter: i32 x
    if (auto* param1 = ast_cast_or_error<ParameterNode>(func->parameters.values[0])) {
        ASSERT_TRUE(std::string(param1->name->name) == "x", "First parameter name should be 'x'");
        ASSERT_TRUE(param1->type != nullptr, "First parameter should have a type");
        if (auto* type1 = param1->type->as<TypeNameNode>()) {
            ASSERT_TRUE(std::string(type1->identifier->name) == "i32", "First parameter type should be 'i32'");
        } else {
            return TestResult(false, "First parameter type should be a TypeNameNode");
        }
    } else {
        return TestResult(false, "First parameter should be a ParameterNode");
    }
    
    // Check second parameter: i32 y  
    if (auto* param2 = ast_cast_or_error<ParameterNode>(func->parameters.values[1])) {
        ASSERT_TRUE(std::string(param2->name->name) == "y", "Second parameter name should be 'y'");
        ASSERT_TRUE(param2->type != nullptr, "Second parameter should have a type");
        if (auto* type2 = param2->type->as<TypeNameNode>()) {
            ASSERT_TRUE(std::string(type2->identifier->name) == "i32", "Second parameter type should be 'i32'");
        } else {
            return TestResult(false, "Second parameter type should be a TypeNameNode");
        }
    } else {
        return TestResult(false, "Second parameter should be a ParameterNode");
    }
    
    return TestResult(true, "Function with parameters parsing successful");
}

TestResult test_function_with_return_type() {
    std::string source = "fn get_value(): i32 { }";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    
    ASSERT_TRUE(result.is_success(), "Parser should successfully parse function with return type");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "CompilationUnitNode should not be null");
    ASSERT_TRUE(unit->statements.size == 1, "Should have exactly one statement");
    
    AstNode* stmt_node = unit->statements[0];
    ASSERT_TRUE(stmt_node->is_a<FunctionDeclarationNode>(), "Should be a FunctionDeclarationNode");
    
    FunctionDeclarationNode* func = stmt_node->as<FunctionDeclarationNode>();
    ASSERT_TRUE(std::string(func->name->name) == "get_value", "Function name should be 'get_value'");
    ASSERT_TRUE(func->returnType != nullptr, "Function should have a return type");
    
    return TestResult(true, "Function with return type parsing successful");
}

TestResult test_type_declaration_parsing() {
    std::string source = "type Point { }";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    
    ASSERT_TRUE(result.is_success(), "Parser should successfully parse type declaration");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "CompilationUnitNode should not be null");
    ASSERT_TRUE(unit->statements.size == 1, "Should have exactly one statement");
    
    AstNode* stmt_node = unit->statements[0];
    ASSERT_TRUE(stmt_node->is_a<TypeDeclarationNode>(), "Should be a TypeDeclarationNode");
    
    TypeDeclarationNode* type_decl = stmt_node->as<TypeDeclarationNode>();
    ASSERT_TRUE(type_decl->name != nullptr, "Type should have a name");
    ASSERT_TRUE(std::string(type_decl->name->name) == "Point", "Type name should be 'Point'");
    
    return TestResult(true, "Type declaration parsing successful");
}

TestResult test_type_with_members() {
    std::string source = "type Point { i32 x; i32 y; }";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    
    ASSERT_TRUE(result.is_success(), "Parser should successfully parse type with members");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "CompilationUnitNode should not be null");
    ASSERT_TRUE(unit->statements.size == 1, "Should have exactly one statement");
    
    AstNode* stmt_node = unit->statements[0];
    ASSERT_TRUE(stmt_node->is_a<TypeDeclarationNode>(), "Should be a TypeDeclarationNode");
    
    TypeDeclarationNode* type_decl = stmt_node->as<TypeDeclarationNode>();
    ASSERT_TRUE(std::string(type_decl->name->name) == "Point", "Type name should be 'Point'");
    ASSERT_TRUE(type_decl->members.size == 2, "Type should have 2 members");
    
    return TestResult(true, "Type with members parsing successful");
}

TestResult test_enum_declaration_parsing() {
    std::string source = "enum Direction { North, South, East, West }";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    
    ASSERT_TRUE(result.is_success(), "Parser should successfully parse enum declaration");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "CompilationUnitNode should not be null");
    ASSERT_TRUE(unit->statements.size == 1, "Should have exactly one statement");
    
    AstNode* stmt_node = unit->statements[0];
    ASSERT_TRUE(stmt_node->is_a<EnumDeclarationNode>(), "Should be an EnumDeclarationNode");
    
    EnumDeclarationNode* enum_decl = stmt_node->as<EnumDeclarationNode>();
    ASSERT_TRUE(enum_decl->name != nullptr, "Enum should have a name");
    ASSERT_TRUE(std::string(enum_decl->name->name) == "Direction", "Enum name should be 'Direction'");
    ASSERT_TRUE(enum_decl->cases.size == 4, "Enum should have 4 cases");
    
    return TestResult(true, "Enum declaration parsing successful");
}

TestResult test_enum_with_associated_data() {
    std::string source = "enum Shape { Circle(f32 radius), Rectangle(f32 width, f32 height) }";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    
    ASSERT_TRUE(result.is_success(), "Parser should successfully parse enum with associated data");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "CompilationUnitNode should not be null");
    ASSERT_TRUE(unit->statements.size == 1, "Should have exactly one statement");
    
    AstNode* stmt_node = unit->statements[0];
    ASSERT_TRUE(stmt_node->is_a<EnumDeclarationNode>(), "Should be an EnumDeclarationNode");
    
    EnumDeclarationNode* enum_decl = stmt_node->as<EnumDeclarationNode>();
    ASSERT_TRUE(std::string(enum_decl->name->name) == "Shape", "Enum name should be 'Shape'");
    ASSERT_TRUE(enum_decl->cases.size == 2, "Enum should have 2 cases");
    
    return TestResult(true, "Enum with associated data parsing successful");
}

TestResult test_using_directive_parsing() {
    std::string source = "using System;";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    
    ASSERT_TRUE(result.is_success(), "Parser should successfully parse using directive");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "CompilationUnitNode should not be null");
    ASSERT_TRUE(unit->statements.size == 1, "Should have exactly one statement");
    
    AstNode* stmt_node = unit->statements[0];
    ASSERT_TRUE(stmt_node->is_a<UsingDirectiveNode>(), "Should be a UsingDirectiveNode");
    
    UsingDirectiveNode* using_stmt = stmt_node->as<UsingDirectiveNode>();
    ASSERT_TRUE(using_stmt->namespaceName != nullptr, "Using directive should have a namespace name");
    
    return TestResult(true, "Using directive parsing successful");
}

// ===== FUNCTION AND TYPE BODY TESTS =====

TestResult test_function_with_body_and_return() {
    std::string source = R"(
        fn calculate(i32 x, i32 y): i32 {
            var result = x + y;
            return result;
        }
    )";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    
    ASSERT_TRUE(result.is_success(), "Parser should successfully parse function with body and return");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "CompilationUnitNode should not be null");
    ASSERT_TRUE(unit->statements.size == 1, "Should have exactly one statement");
    
    AstNode* stmt_node = unit->statements[0];
    ASSERT_TRUE(stmt_node->is_a<FunctionDeclarationNode>(), "Should be a FunctionDeclarationNode");
    
    FunctionDeclarationNode* func = stmt_node->as<FunctionDeclarationNode>();
    ASSERT_TRUE(std::string(func->name->name) == "calculate", "Function name should be 'calculate'");
    ASSERT_TRUE(func->returnType != nullptr, "Function should have a return type");
    ASSERT_TRUE(func->body != nullptr, "Function should have a body");
    
    // Check that the body is a block with statements
    BlockStatementNode* body = func->body;
    ASSERT_TRUE(body->statements.size == 2, "Body should have 2 statements (var declaration + return)");
    
    return TestResult(true, "Function with body and return parsing successful");
}

TestResult test_function_with_complex_body() {
    std::string source = R"(
        fn complex_function(i32 a, i32 b): i32 {
            if (a > b) {
                var temp = a;
                a = b;
                b = temp;
            }
            
            while (a < 10) {
                a++;
            }
            
            for (i32 i = 0; i < b; i++) {
                a = a + i;
            }
            
            return a + b;
        }
    )";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    
    ASSERT_TRUE(result.is_success(), "Parser should successfully parse function with complex body");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "CompilationUnitNode should not be null");
    ASSERT_TRUE(unit->statements.size == 1, "Should have exactly one statement");
    
    AstNode* stmt_node = unit->statements[0];
    ASSERT_TRUE(stmt_node->is_a<FunctionDeclarationNode>(), "Should be a FunctionDeclarationNode");
    
    FunctionDeclarationNode* func = stmt_node->as<FunctionDeclarationNode>();
    ASSERT_TRUE(std::string(func->name->name) == "complex_function", "Function name should be 'complex_function'");
    ASSERT_TRUE(func->body != nullptr, "Function should have a body");
    
    // The body should contain: if, while, for, return statements
    BlockStatementNode* body = func->body;
    ASSERT_TRUE(body->statements.size == 4, "Body should have 4 statements");
    
    return TestResult(true, "Function with complex body parsing successful");
}

TestResult test_type_with_mixed_members() {
    std::string source = R"(
        type Calculator {
            i32 value;
            string name;
            
            fn add(i32 x): i32 {
                value = value + x;
                return value;
            }
            
            fn get_name(): string {
                return name;
            }
        }
    )";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    
    ASSERT_TRUE(result.is_success(), "Parser should successfully parse type with mixed members");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "CompilationUnitNode should not be null");
    ASSERT_TRUE(unit->statements.size == 1, "Should have exactly one statement");
    
    AstNode* stmt_node = unit->statements[0];
    ASSERT_TRUE(stmt_node->is_a<TypeDeclarationNode>(), "Should be a TypeDeclarationNode");
    
    TypeDeclarationNode* type_decl = stmt_node->as<TypeDeclarationNode>();
    ASSERT_TRUE(std::string(type_decl->name->name) == "Calculator", "Type name should be 'Calculator'");
    ASSERT_TRUE(type_decl->members.size == 4, "Type should have 4 members (2 fields + 2 functions)");
    
    // Check that we have both field and function members
    bool found_field = false;
    bool found_function = false;
    for (int i = 0; i < type_decl->members.size; ++i) {
        AstNode* member = type_decl->members[i];
        if (member->is_a<VariableDeclarationNode>()) {
            found_field = true;
        } else if (member->is_a<FunctionDeclarationNode>()) {
            found_function = true;
        }
    }
    
    ASSERT_TRUE(found_field, "Type should contain field declarations");
    ASSERT_TRUE(found_function, "Type should contain function declarations");
    
    return TestResult(true, "Type with mixed members parsing successful");
}

TestResult test_type_with_nested_functions() {
    std::string source = R"(
        type Math {
            fn factorial(i32 n): i32 {
                if (n <= 1) {
                    return 1;
                }
                return n * factorial(n - 1);
            }
            
            fn fibonacci(i32 n): i32 {
                if (n <= 1) {
                    return n;
                }
                return fibonacci(n - 1) + fibonacci(n - 2);
            }
        }
    )";
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    
    ASSERT_TRUE(result.is_success(), "Parser should successfully parse type with nested functions");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "CompilationUnitNode should not be null");
    ASSERT_TRUE(unit->statements.size == 1, "Should have exactly one statement");
    
    AstNode* stmt_node = unit->statements[0];
    ASSERT_TRUE(stmt_node->is_a<TypeDeclarationNode>(), "Should be a TypeDeclarationNode");
    
    TypeDeclarationNode* type_decl = stmt_node->as<TypeDeclarationNode>();
    ASSERT_TRUE(std::string(type_decl->name->name) == "Math", "Type name should be 'Math'");
    ASSERT_TRUE(type_decl->members.size == 2, "Type should have 2 function members");
    
    // Verify both members are functions
    for (int i = 0; i < type_decl->members.size; ++i) {
        AstNode* member = type_decl->members[i];
        ASSERT_TRUE(member->is_a<FunctionDeclarationNode>(), "All members should be functions");
        
        FunctionDeclarationNode* func = member->as<FunctionDeclarationNode>();
        ASSERT_TRUE(func->body != nullptr, "Function should have a body");
        ASSERT_TRUE(func->returnType != nullptr, "Function should have a return type");
    }
    
    return TestResult(true, "Type with nested functions parsing successful");
}

TestResult test_comprehensive_parser_integration() {
    std::string source = R"(
        // Complex integration test with multiple features and intentional errors
        using Collections;
        
        enum Status {
            Active,
            Pending(i32 priority, string message),
            Failed(string reason)
        }
        
        type Calculator {
            // Field declarations with various types
            i32 value, count;
            var precision = 0.001;
            string name;
            
            // Function with complex body including all control flow
            fn process(i32 input, Status status): i32 {
                var result = input * 2;
                
                if (result > 100) {
                    result = 100;
                } else if (result < 0) {
                    result = 0;
                }
                
                while (result > 50) {
                    result = result - 10;
                }
                
                for (i32 i = 0; i < count; i++) {
                    result = result + i;
                }
                
                // Intentional error: missing semicolon should be recovered
                i32 temp = result + value  // Missing semicolon here
                
                return result + temp;
            }
            
            // Complex expression function
            fn calculate(): f64 {
                return (precision * 100.0) + (value / 2) - count;
            }
        }
        
        fn global_function(Calculator calc, i32 x, i32 y): i32 {
            // Complex expressions with member access, calls, and operators
            var total = calc.process(x + y, Status.Pending(1, "test"));
            var intermediate = calc.calculate() * 2.5;
            
            // Nested function calls and member access
            var complex = calc.process(
                calc.value + (x * y),
                Status.Active
            ) + intermediate;
            
            // Intentional error: unclosed parentheses should be recovered
            var broken = (x + y  // Missing closing paren
            
            if (total > complex) {
                return total;
            }
            
            return complex;
        }
        
        // Intentional error: invalid enum case should create error node
        enum InvalidEnum {
            Case1,
            Case2(  // Incomplete case declaration
        }
    )";
    
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    
    ASSERT_TRUE(result.is_success(), "Parser should successfully parse despite errors");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "CompilationUnitNode should not be null");
    ASSERT_TRUE(unit->statements.size == 5, "Should have 5 top-level statements (using, enum, type, function, invalid enum)");
    
    // Test using directive
    AstNode* stmt0 = unit->statements[0];
    ASSERT_TRUE(stmt0->is_a<UsingDirectiveNode>(), "First statement should be using directive");
    UsingDirectiveNode* using_stmt = stmt0->as<UsingDirectiveNode>();
    ASSERT_TRUE(using_stmt->namespaceName != nullptr, "Using directive should have namespace name");
    
    // Test enum declaration
    AstNode* stmt1 = unit->statements[1];
    ASSERT_TRUE(stmt1->is_a<EnumDeclarationNode>(), "Second statement should be enum declaration");
    EnumDeclarationNode* status_enum = stmt1->as<EnumDeclarationNode>();
    ASSERT_TRUE(std::string(status_enum->name->name) == "Status", "Enum name should be 'Status'");
    ASSERT_TRUE(status_enum->cases.size == 3, "Status enum should have 3 cases");
    
    // Test enum cases
    EnumCaseNode* active_case = status_enum->cases[0];
    ASSERT_TRUE(std::string(active_case->name->name) == "Active", "First case should be 'Active'");
    ASSERT_TRUE(active_case->associatedData.size == 0, "Active case should have no associated data");
    
    EnumCaseNode* pending_case = status_enum->cases[1];
    ASSERT_TRUE(std::string(pending_case->name->name) == "Pending", "Second case should be 'Pending'");
    ASSERT_TRUE(pending_case->associatedData.size == 2, "Pending case should have 2 parameters");
    
    // Test type declaration
    AstNode* stmt2 = unit->statements[2];
    ASSERT_TRUE(stmt2->is_a<TypeDeclarationNode>(), "Third statement should be type declaration");
    TypeDeclarationNode* calc_type = stmt2->as<TypeDeclarationNode>();
    ASSERT_TRUE(std::string(calc_type->name->name) == "Calculator", "Type name should be 'Calculator'");
    ASSERT_TRUE(calc_type->members.size >= 5, "Calculator should have at least 5 members (fields + functions)");
    
    // Test field declarations in type
    bool found_value_field = false;
    bool found_precision_field = false;
    bool found_process_function = false;
    bool found_calculate_function = false;
    
    for (int i = 0; i < calc_type->members.size; i++) {
        AstNode* member = calc_type->members[i];
        
        if (member->is_a<VariableDeclarationNode>()) {
            VariableDeclarationNode* field = member->as<VariableDeclarationNode>();
            if (field->names.size > 0) {
                std::string field_name(field->names[0]->name);
                if (field_name == "value") found_value_field = true;
                if (field_name == "precision") {
                    found_precision_field = true;
                    ASSERT_TRUE(field->initializer != nullptr, "Precision field should have initializer");
                }
            }
        } else if (member->is_a<FunctionDeclarationNode>()) {
            FunctionDeclarationNode* func = member->as<FunctionDeclarationNode>();
            std::string func_name(func->name->name);
            
            if (func_name == "process") {
                found_process_function = true;
                ASSERT_TRUE(func->body != nullptr, "Process function should have body");
                ASSERT_TRUE(func->returnType != nullptr, "Process function should have return type");
                
                // Test complex function body with errors
                BlockStatementNode* body = func->body;
                ASSERT_TRUE(body->statements.size >= 6, "Process function should have multiple statements including error recovery");
                
                // Check for if statement
                bool found_if = false;
                bool found_while = false;
                bool found_for = false;
                bool found_return = false;
                bool found_error = false;
                
                for (int j = 0; j < body->statements.size; j++) {
                    AstNode* stmt = body->statements[j];
                    if (stmt->is_a<IfStatementNode>()) found_if = true;
                    if (stmt->is_a<WhileStatementNode>()) found_while = true;
                    if (stmt->is_a<ForStatementNode>()) found_for = true;
                    if (stmt->is_a<ReturnStatementNode>()) found_return = true;
                    if (stmt->is_a<ErrorNode>()) found_error = true;
                }
                
                ASSERT_TRUE(found_if, "Process function should contain if statement");
                ASSERT_TRUE(found_while, "Process function should contain while statement");
                ASSERT_TRUE(found_for, "Process function should contain for statement");
                ASSERT_TRUE(found_return, "Process function should contain return statement");
                ASSERT_TRUE(found_error, "Process function should contain error nodes from missing semicolon");
            }
            
            if (func_name == "calculate") {
                found_calculate_function = true;
                ASSERT_TRUE(func->body != nullptr, "Calculate function should have body");
            }
        }
    }
    
    ASSERT_TRUE(found_value_field, "Calculator should have value field");
    ASSERT_TRUE(found_precision_field, "Calculator should have precision field");
    ASSERT_TRUE(found_process_function, "Calculator should have process function");
    ASSERT_TRUE(found_calculate_function, "Calculator should have calculate function");
    
    // Test global function
    AstNode* stmt3 = unit->statements[3];
    ASSERT_TRUE(stmt3->is_a<FunctionDeclarationNode>(), "Fourth statement should be function declaration");
    FunctionDeclarationNode* global_func = stmt3->as<FunctionDeclarationNode>();
    ASSERT_TRUE(std::string(global_func->name->name) == "global_function", "Function name should be 'global_function'");
    ASSERT_TRUE(global_func->body != nullptr, "Global function should have body");
    
    // Test that global function body contains complex expressions and error recovery
    BlockStatementNode* global_body = global_func->body;
    ASSERT_TRUE(global_body->statements.size >= 4, "Global function should have multiple statements");
    
    // Test invalid enum (should have error nodes)
    AstNode* stmt4 = unit->statements[4];
    ASSERT_TRUE(stmt4->is_a<EnumDeclarationNode>() || stmt4->is_a<ErrorNode>(), 
               "Fifth statement should be enum or error node");
    
    if (stmt4->is_a<EnumDeclarationNode>()) {
        EnumDeclarationNode* invalid_enum = stmt4->as<EnumDeclarationNode>();
        // Check if any of the cases have errors
        bool has_error_cases = false;
        for (int i = 0; i < invalid_enum->cases.size; i++) {
            if (invalid_enum->cases[i] == nullptr) {
                has_error_cases = true;
            } else if (invalid_enum->cases[i]->contains_errors) {
                has_error_cases = true;
            }
        }
        
        ASSERT_TRUE(invalid_enum->contains_errors || has_error_cases, "Invalid enum should contain errors or have error cases");
    }
    
    // Verify parser collected diagnostics for errors
    auto& diagnostics = parser.get_diagnostics();
    ASSERT_TRUE(diagnostics.size() > 0, "Parser should have collected error diagnostics");
    
    return TestResult(true, "Comprehensive parser integration test successful");
}

TestResult test_floating_point_literals() {
    std::string source = R"(
        fn test_floats() {
            var float_val = 3.14;
            var double_val = 2.718;
            return float_val + double_val;
        }
    )";
    
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    
    ASSERT_TRUE(result.is_success(), "Parser should successfully parse floating-point literals");
    
    CompilationUnitNode* unit = result.get_node();
    ASSERT_TRUE(unit != nullptr, "CompilationUnitNode should not be null");
    ASSERT_TRUE(unit->statements.size == 1, "Should have 1 function declaration");
    
    // Get the function
    AstNode* stmt0 = unit->statements[0];
    ASSERT_TRUE(stmt0->is_a<FunctionDeclarationNode>(), "Should be function declaration");
    FunctionDeclarationNode* func = stmt0->as<FunctionDeclarationNode>();
    ASSERT_TRUE(func->body != nullptr, "Function should have body");
    
    // Check function body
    BlockStatementNode* body = func->body;
    std::cout << "DEBUG: Function body has " << body->statements.size << " statements" << std::endl;
    
    for (int i = 0; i < body->statements.size; i++) {
        AstNode* stmt = body->statements[i];
        if (stmt->is_a<VariableDeclarationNode>()) {
            std::cout << "  Statement " << i << ": VariableDeclarationNode" << std::endl;
        } else if (stmt->is_a<VariableDeclarationNode>()) {
            std::cout << "  Statement " << i << ": VariableDeclarationNode" << std::endl;
        } else if (stmt->is_a<ExpressionStatementNode>()) {
            std::cout << "  Statement " << i << ": ExpressionStatementNode" << std::endl;
        } else if (stmt->is_a<ReturnStatementNode>()) {
            std::cout << "  Statement " << i << ": ReturnStatementNode" << std::endl;
        } else if (stmt->is_a<ErrorNode>()) {
            std::cout << "  Statement " << i << ": ErrorNode" << std::endl;
        } else {
            std::cout << "  Statement " << i << ": Other type (node type: " << get_node_type_name(stmt) << ")" << std::endl;
        }
    }
    
    ASSERT_TRUE(body->statements.size == 3, "Function body should have 3 statements (2 vars + 1 return)");
    
    // Test first floating-point literal
    AstNode* var_stmt0 = body->statements[0];
    ASSERT_TRUE(var_stmt0->is_a<VariableDeclarationNode>(), "First statement should be local variable declaration");
    VariableDeclarationNode* var0 = var_stmt0->as<VariableDeclarationNode>();
    ASSERT_TRUE(var0->names.size == 1, "First local var should have 1 name");

    ASSERT_TRUE(std::string(var0->name->name) == "float_val", "First variable should be 'float_val'");
    ASSERT_TRUE(var0->initializer != nullptr, "First variable should have initializer");
    ASSERT_TRUE(var0->initializer->is_a<LiteralExpressionNode>(), "Initializer should be literal expression");
    
    LiteralExpressionNode* literal0 = var0->initializer->as<LiteralExpressionNode>();
    ASSERT_TRUE(literal0->kind == LiteralKind::Float, "First literal should be float");
    ASSERT_TRUE(std::string(literal0->token->text) == "3.14", "First literal should be '3.14'");
    
    // Test second floating-point literal  
    AstNode* var_stmt1 = body->statements[1];
    ASSERT_TRUE(var_stmt1->is_a<VariableDeclarationNode>(), "Second statement should be local variable declaration");
    VariableDeclarationNode* var1 = var_stmt1->as<VariableDeclarationNode>();
    ASSERT_TRUE(var1->names.size == 1, "Second local var should have 1 name");

    ASSERT_TRUE(std::string(var1->name->name) == "double_val", "Second variable should be 'double_val'");
    ASSERT_TRUE(var1->initializer != nullptr, "Second variable should have initializer");
    ASSERT_TRUE(var1->initializer->is_a<LiteralExpressionNode>(), "Initializer should be literal expression");
    
    LiteralExpressionNode* literal1 = var1->initializer->as<LiteralExpressionNode>();
    ASSERT_TRUE(literal1->kind == LiteralKind::Float, "Second literal should be float (lexer treats all float literals as Float)");
    ASSERT_TRUE(std::string(literal1->token->text) == "2.718", "Second literal should be '2.718'");
    
    return TestResult(true, "Floating-point literal parsing successful");
}

TestResult test_member_var_declarations() {
    std::string source = R"(
        type TestClass {
            public static var count = 42;
            private var name = "test";
            var value = 3.14;
        }
    )";
    
    TokenStream stream = create_token_stream(source);
    Parser parser(stream);
    
    auto result = parser.parse();
    
    ASSERT_TRUE(result.is_success(), "Parser should successfully parse member var declarations");
    
    auto* unit = result.get_node();
    ASSERT_TRUE(unit->statements.size == 1, "Should have one type declaration");
    
    auto* type_decl = static_cast<TypeDeclarationNode*>(unit->statements[0]);
    ASSERT_TRUE(type_decl != nullptr, "Should have type declaration");
    ASSERT_TRUE(std::string(type_decl->name->name) == "TestClass", "Type name should be 'TestClass'");
    
    // Check that we have field members
    ASSERT_TRUE(type_decl->members.size == 3, "Should have exactly 3 members");
    
    // Verify specific fields
    ASSERT_TRUE(type_decl->members[0]->is_a<VariableDeclarationNode>(), "First member should be field");
    auto* count_field = type_decl->members[0]->as<VariableDeclarationNode>();
    ASSERT_TRUE(std::string(count_field->names[0]->name) == "count", "First field should be 'count'");
    ASSERT_TRUE(count_field->initializer != nullptr, "Count field should have initializer");
    ASSERT_TRUE(count_field->initializer->is_a<LiteralExpressionNode>(), "Count initializer should be literal");
    
    ASSERT_TRUE(type_decl->members[1]->is_a<VariableDeclarationNode>(), "Second member should be field");
    auto* name_field = type_decl->members[1]->as<VariableDeclarationNode>();
    ASSERT_TRUE(std::string(name_field->names[0]->name) == "name", "Second field should be 'name'");
    ASSERT_TRUE(name_field->initializer != nullptr, "Name field should have initializer");
    ASSERT_TRUE(name_field->initializer->is_a<LiteralExpressionNode>(), "Name initializer should be literal");
    
    return TestResult(true, "Member var declaration parsing successful");
}

// Main test runner function
void run_parser_tests() {
    TestSuite suite("Parser Tests");
    
    // Basic tests
    suite.add_test("Basic Expression Parsing", test_basic_expression_parsing);
    suite.add_test("Variable Declaration Parsing", test_variable_declaration_parsing);
    suite.add_test("Multiple Variable Declarations", test_multiple_variable_declarations);
    suite.add_test("Typed Variable Declaration", test_typed_variable_declaration);
    suite.add_test("Multiple Typed Variable Declaration", test_multiple_typed_variable_declaration);
    suite.add_test("Block Statement Parsing", test_block_statement_parsing);
    suite.add_test("Parser Error Recovery", test_parser_error_recovery);
    suite.add_test("Empty Program", test_empty_program);
    
    // Error handling tests
    suite.add_test("Missing Semicolon Recovery", test_missing_semicolon_recovery);
    suite.add_test("Malformed Expression Recovery", test_malformed_expression_recovery);
    suite.add_test("Multiple Errors Recovery", test_multiple_errors_recovery);
    suite.add_test("AST Helper Functions", test_ast_helper_functions);
    
    // Expression tests
    suite.add_test("Complex Expressions", test_complex_expressions);
    suite.add_test("Literal Types", test_literal_types);
    
    // Statement tests
    suite.add_test("Complex Variable Declarations", test_complex_variable_declarations);
    suite.add_test("Nested Blocks", test_nested_blocks);
    
    // Stress tests
    suite.add_test("Large Expression", test_large_expression);
    suite.add_test("Many Statements", test_many_statements);
    
    // Edge cases
    suite.add_test("Empty Blocks", test_empty_blocks);
    suite.add_test("Whitespace Handling", test_whitespace_handling);
    
    // ===== NEW COMPREHENSIVE TESTS =====
    
    // Advanced operator tests
    suite.add_test("Operator Precedence", test_operator_precedence);
    suite.add_test("Comparison Operators", test_comparison_operators);
    suite.add_test("Logical Operators", test_logical_operators);
    suite.add_test("Parenthesized Expressions", test_parenthesized_expressions);
    
    // Control flow tests
    suite.add_test("If Statement Parsing", test_if_statement_parsing);
    suite.add_test("While Statement Parsing", test_while_statement_parsing);
    suite.add_test("For Statement Parsing", test_for_statement_parsing);
    suite.add_test("For Statement Variations", test_for_statement_variations);
    suite.add_test("Nested Control Flow", test_nested_control_flow);
    
    // Advanced error recovery tests
    suite.add_test("Malformed If Statement Recovery", test_malformed_if_statement_recovery);
    suite.add_test("Unclosed Block Recovery", test_unclosed_block_recovery);
    suite.add_test("Invalid Variable Name Recovery", test_invalid_variable_name_recovery);
    
    // Comprehensive stress tests
    suite.add_test("Deeply Nested Expressions", test_deeply_nested_expressions);
    suite.add_test("Mixed Statement Types", test_mixed_statement_types);
    suite.add_test("Performance Many Binary Operations", test_performance_many_binary_operations);
    
    // Edge case tests
    suite.add_test("Empty Parentheses", test_empty_parentheses);
    suite.add_test("Malformed Operators", test_malformed_operators);
    
    // ===== NEW EXPRESSION TYPE TESTS =====
    
    // Assignment expressions
    suite.add_test("Assignment Expressions", test_assignment_expressions);
    
    // Unary expressions
    suite.add_test("Unary Expressions", test_unary_expressions);
    
    // Call expressions
    suite.add_test("Call Expressions", test_call_expressions);
    
    // Member access expressions
    suite.add_test("Member Access Expressions", test_member_access_expressions);
    
    // Complex expression combinations
    suite.add_test("Complex Postfix Chaining", test_complex_postfix_chaining);
    suite.add_test("Expression Precedence with New Operators", test_expression_precedence_with_new_operators);
    
    // New expressions
    // suite.add_test("New Expressions", test_new_expressions); // Test not implemented
    
    // ===== DECLARATION PARSER TESTS =====
    
    // Function declarations
    suite.add_test("Function Declaration Parsing", test_function_declaration_parsing);
    suite.add_test("Function With Parameters", test_function_with_parameters);
    suite.add_test("Function With Return Type", test_function_with_return_type);
    
    // Type declarations
    suite.add_test("Type Declaration Parsing", test_type_declaration_parsing);
    suite.add_test("Type With Members", test_type_with_members);
    
    // Enum declarations
    suite.add_test("Enum Declaration Parsing", test_enum_declaration_parsing);
    suite.add_test("Enum With Associated Data", test_enum_with_associated_data);
    
    // Using directives
    suite.add_test("Using Directive Parsing", test_using_directive_parsing);
    
    // ===== FUNCTION AND TYPE BODY TESTS =====
    
    // Function body tests
    suite.add_test("Function With Body And Return", test_function_with_body_and_return);
    suite.add_test("Function With Complex Body", test_function_with_complex_body);
    
    // Type body tests
    suite.add_test("Type With Mixed Members", test_type_with_mixed_members);
    suite.add_test("Type With Nested Functions", test_type_with_nested_functions);
    
    // Comprehensive integration test
    suite.add_test("Comprehensive Parser Integration", test_comprehensive_parser_integration);
    
    // Floating-point literal test
    suite.add_test("Floating Point Literals", test_floating_point_literals);
    
    // Member var declaration test  
    suite.add_test("Member Var Declarations", test_member_var_declarations);
    
    suite.run_all();
}