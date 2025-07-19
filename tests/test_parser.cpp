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
    ASSERT_TRUE(stmt->is_a<LocalVariableDeclarationNode>(), "Should be a local variable declaration");
    
    LocalVariableDeclarationNode* var_decl = stmt->as<LocalVariableDeclarationNode>();
    ASSERT_TRUE(var_decl->declarators.size == 1, "Should have one declarator");
    
    VariableDeclarationNode* declarator = var_decl->declarators[0];
    ASSERT_TRUE(declarator->name != nullptr, "Variable name should not be null");
    ASSERT_TRUE(declarator->initializer != nullptr, "Variable initializer should not be null");
    
    return TestResult(true, "Variable declaration parsing successful");
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
    } else if (auto* var_decl = node_cast<LocalVariableDeclarationNode>(node)) {
        for (int i = 0; i < var_decl->declarators.size; ++i) {
            auto* declarator = var_decl->declarators[i];
            if (declarator->initializer) {
                count += count_errors_in_ast(declarator->initializer);
            }
        }
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
        
        auto* stmt = ast_cast_or_error<LocalVariableDeclarationNode>(stmt_node);
        ASSERT_TRUE(stmt != nullptr, "Should be local variable declaration");
        ASSERT_TRUE(stmt->declarators.size == 1, "Should have one declarator");
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
    
    auto* var_decl = ast_cast_or_error<LocalVariableDeclarationNode>(unit->statements[0]);
    ASSERT_TRUE(var_decl != nullptr, "Should be variable declaration");
    ASSERT_TRUE(var_decl->declarators.size == 1, "Should have one declarator");
    
    auto* declarator = var_decl->declarators[0];
    ASSERT_TRUE(declarator->initializer != nullptr, "Should have initializer");
    
    // The initializer should be a complex binary expression tree
    auto* expr = ast_cast_or_error<BinaryExpressionNode>(declarator->initializer);
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
        auto* var_decl = ast_cast_or_error<LocalVariableDeclarationNode>(unit->statements[i]);
        ASSERT_TRUE(var_decl != nullptr, "Should be variable declaration");
        
        auto* declarator = var_decl->declarators[0];
        auto* binary_expr = ast_cast_or_error<BinaryExpressionNode>(declarator->initializer);
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
    auto* and_var = ast_cast_or_error<LocalVariableDeclarationNode>(unit->statements[0]);
    auto* and_expr = ast_cast_or_error<BinaryExpressionNode>(and_var->declarators[0]->initializer);
    ASSERT_TRUE(and_expr->opKind == BinaryOperatorKind::LogicalAnd, "Should be logical AND");
    
    // Check logical OR
    auto* or_var = ast_cast_or_error<LocalVariableDeclarationNode>(unit->statements[1]);
    auto* or_expr = ast_cast_or_error<BinaryExpressionNode>(or_var->declarators[0]->initializer);
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
    
    auto* var_decl = ast_cast_or_error<LocalVariableDeclarationNode>(unit->statements[0]);
    auto* mult_expr = ast_cast_or_error<BinaryExpressionNode>(var_decl->declarators[0]->initializer);
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
    ASSERT_TRUE(unit->statements[0]->is_a<LocalVariableDeclarationNode>(), "First should be variable declaration");
    ASSERT_TRUE(unit->statements[1]->is_a<BlockStatementNode>(), "Second should be block statement");
    ASSERT_TRUE(unit->statements[2]->is_a<LocalVariableDeclarationNode>(), "Third should be variable declaration");
    
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

// Main test runner function
void run_parser_tests() {
    TestSuite suite("Parser Tests");
    
    // Basic tests
    suite.add_test("Basic Expression Parsing", test_basic_expression_parsing);
    suite.add_test("Variable Declaration Parsing", test_variable_declaration_parsing);
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
    
    suite.run_all();
}