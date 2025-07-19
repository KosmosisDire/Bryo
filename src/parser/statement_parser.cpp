#include "parser/statement_parser.h"
#include "parser/parser.h"
#include "parser/expression_parser.h"

namespace Mycelium::Scripting::Lang {

StatementParser::StatementParser(Parser* parser) : parser_(parser) {
}

// Helper accessors for parser state
ParseContext& StatementParser::context() {
    return parser_->get_context();
}

ErrorNode* StatementParser::create_error(ErrorKind kind, const char* msg) {
    return parser_->create_error(kind, msg);
}

// Main statement parsing entry point
ParseResult<StatementNode> StatementParser::parse_statement() {
    auto& ctx = context();
    
    // Handle variable declarations
    if (ctx.check(TokenKind::Var)) {
        return parse_variable_declaration();
    }
    
    // Handle control flow statements
    if (ctx.check(TokenKind::If)) {
        return parse_if_statement();
    }
    
    if (ctx.check(TokenKind::While)) {
        return parse_while_statement();
    }
    
    // Handle block statements
    if (ctx.check(TokenKind::LeftBrace)) {
        return parse_block_statement();
    }
    
    // Handle expression statements
    return parse_expression_statement();
}

// Expression statement parsing
ParseResult<StatementNode> StatementParser::parse_expression_statement() {
    // Handle expression statements
    auto expr_result = parser_->get_expression_parser().parse_expression();
    
    if (expr_result.is_success()) {
        auto* expr_stmt = parser_->get_allocator().alloc<ExpressionStatementNode>();
        expr_stmt->expression = expr_result.get_node();
        expr_stmt->contains_errors = ast_has_errors(expr_result.get_ast_node());  // Propagate errors
        
        // Expect semicolon
        if (parser_->expect(TokenKind::Semicolon, "Expected ';' after expression")) {
            // Semicolon consumed successfully
        }
        
        return ParseResult<StatementNode>::success(expr_stmt);
    }
    
    return ParseResult<StatementNode>::error(expr_result.get_error());
}

// Variable declaration parsing
ParseResult<StatementNode> StatementParser::parse_variable_declaration() {
    auto& ctx = context();
    
    ctx.advance(); // consume 'var'
    
    if (!ctx.check(TokenKind::Identifier)) {
        return ParseResult<StatementNode>::error(
            create_error(ErrorKind::MissingToken, "Expected identifier after 'var'"));
    }
    
    const Token& name_token = ctx.current();
    ctx.advance();
    
    auto* var_decl = parser_->get_allocator().alloc<VariableDeclarationNode>();
    var_decl->contains_errors = false;  // Initialize, will update based on children
    auto* name_node = parser_->get_allocator().alloc<IdentifierNode>();
    name_node->name = name_token.text;
    name_node->contains_errors = false;
    var_decl->name = name_node;
    
    // Optional type annotation: var x: i32
    if (parser_->match(TokenKind::Colon)) {
        auto type_result = parser_->parse_type_expression();
        if (type_result.is_success()) {
            var_decl->type = type_result.get_node();
        }
    }
    
    // Optional initializer: var x = 5
    if (parser_->match(TokenKind::Assign)) {
        auto init_result = parser_->get_expression_parser().parse_expression();
        if (init_result.is_success()) {
            var_decl->initializer = init_result.get_node();
        }
    }
    
    parser_->expect(TokenKind::Semicolon, "Expected ';' after variable declaration");
    
    // Update error flag based on type and initializer
    bool has_errors = false;
    if (var_decl->type && ast_has_errors(var_decl->type)) has_errors = true;
    if (var_decl->initializer && ast_has_errors(var_decl->initializer)) has_errors = true;
    var_decl->contains_errors = has_errors;
    
    // Wrap in LocalVariableDeclarationNode as per AST design
    auto* local_var_decl = parser_->get_allocator().alloc<LocalVariableDeclarationNode>();
    // For now, we'll need to create a SizedArray for the declarators
    // This is a simplified approach - in full implementation we'd handle multiple declarators
    auto* declarators = parser_->get_allocator().alloc_array<VariableDeclarationNode*>(1);
    declarators[0] = var_decl;
    local_var_decl->declarators.values = declarators;
    local_var_decl->declarators.size = 1;
    local_var_decl->contains_errors = has_errors;  // Propagate errors
    
    return ParseResult<StatementNode>::success(local_var_decl);
}

// Block statement parsing
ParseResult<StatementNode> StatementParser::parse_block_statement() {
    auto& ctx = context();
    
    if (!parser_->match(TokenKind::LeftBrace)) {
        return ParseResult<StatementNode>::error(
            create_error(ErrorKind::MissingToken, "Expected '{'"));
    }
    
    auto* block = parser_->get_allocator().alloc<BlockStatementNode>();
    block->contains_errors = false;  // Initialize, will update based on children
    std::vector<AstNode*> statements;  // Changed to AstNode* for error integration
    
    while (!ctx.check(TokenKind::RightBrace) && !ctx.at_end()) {
        auto stmt_result = parse_statement();
        
        if (stmt_result.is_success()) {
            statements.push_back(stmt_result.get_node());
        } else if (stmt_result.is_error()) {
            // Add error node and continue
            statements.push_back(stmt_result.get_error());
            // Simple recovery: skip to next statement boundary
            parser_->get_recovery().recover_to_safe_point(ctx);
        } else {
            // Fatal error
            break;
        }
    }
    
    parser_->expect(TokenKind::RightBrace, "Expected '}' to close block");
    
    // Allocate SizedArray for statements (now using AstNode* for error integration)
    if (!statements.empty()) {
        auto* stmt_array = parser_->get_allocator().alloc_array<AstNode*>(statements.size());
        bool has_errors = false;
        for (size_t i = 0; i < statements.size(); ++i) {
            stmt_array[i] = statements[i];
            if (ast_has_errors(statements[i])) {
                has_errors = true;
            }
        }
        block->statements.values = stmt_array;
        block->statements.size = static_cast<int>(statements.size());
        block->contains_errors = has_errors;  // Propagate error flag
    } else {
        block->contains_errors = false;
    }
    
    return ParseResult<StatementNode>::success(block);
}

// If statement parsing
ParseResult<StatementNode> StatementParser::parse_if_statement() {
    auto& ctx = context();
    
    ctx.advance(); // consume 'if'
    
    if (!parser_->match(TokenKind::LeftParen)) {
        return ParseResult<StatementNode>::error(
            create_error(ErrorKind::MissingToken, "Expected '(' after 'if'"));
    }
    
    // Parse condition expression
    auto condition_result = parser_->get_expression_parser().parse_expression();
    
    parser_->expect(TokenKind::RightParen, "Expected ')' after if condition");
    
    // Parse then statement
    auto then_result = parse_statement();
    
    auto* if_stmt = parser_->get_allocator().alloc<IfStatementNode>();
    if_stmt->condition = static_cast<ExpressionNode*>(condition_result.get_ast_node());  // Could be ErrorNode
    if_stmt->thenStatement = static_cast<StatementNode*>(then_result.get_ast_node());   // Could be ErrorNode
    if_stmt->elseStatement = nullptr;  // No else for now
    
    // Update error flag
    bool has_errors = ast_has_errors(if_stmt->condition) || ast_has_errors(if_stmt->thenStatement);
    if_stmt->contains_errors = has_errors;
    
    return ParseResult<StatementNode>::success(if_stmt);
}

// While statement parsing with loop context management
ParseResult<StatementNode> StatementParser::parse_while_statement() {
    auto& ctx = context();
    
    ctx.advance(); // consume 'while'
    
    if (!parser_->match(TokenKind::LeftParen)) {
        return ParseResult<StatementNode>::error(
            create_error(ErrorKind::MissingToken, "Expected '(' after 'while'"));
    }
    
    // Parse condition expression
    auto condition_result = parser_->get_expression_parser().parse_expression();
    
    parser_->expect(TokenKind::RightParen, "Expected ')' after while condition");
    
    // Enter loop context for break/continue validation
    auto loop_guard = ctx.save_context();
    ctx.set_loop_context(true);
    
    // Parse body with loop context active
    auto body_result = parse_statement();
    
    // Loop guard automatically restores context when destroyed
    
    auto* while_stmt = parser_->get_allocator().alloc<WhileStatementNode>();
    while_stmt->condition = static_cast<ExpressionNode*>(condition_result.get_ast_node());  // Could be ErrorNode
    while_stmt->body = static_cast<StatementNode*>(body_result.get_ast_node());           // Could be ErrorNode
    
    // Update error flag
    bool has_errors = ast_has_errors(while_stmt->condition) || ast_has_errors(while_stmt->body);
    while_stmt->contains_errors = has_errors;
    
    return ParseResult<StatementNode>::success(while_stmt);
}

ParseResult<StatementNode> StatementParser::parse_for_statement() {
    return ParseResult<StatementNode>::error(
        create_error(ErrorKind::UnexpectedToken, "For statements not implemented yet"));
}

ParseResult<StatementNode> StatementParser::parse_for_in_statement() {
    return ParseResult<StatementNode>::error(
        create_error(ErrorKind::UnexpectedToken, "For-in statements not implemented yet"));
}

ParseResult<StatementNode> StatementParser::parse_return_statement() {
    return ParseResult<StatementNode>::error(
        create_error(ErrorKind::UnexpectedToken, "Return statements not implemented yet"));
}

ParseResult<StatementNode> StatementParser::parse_break_statement() {
    return ParseResult<StatementNode>::error(
        create_error(ErrorKind::UnexpectedToken, "Break statements not implemented yet"));
}

ParseResult<StatementNode> StatementParser::parse_continue_statement() {
    return ParseResult<StatementNode>::error(
        create_error(ErrorKind::UnexpectedToken, "Continue statements not implemented yet"));
}

// Helper methods
bool StatementParser::is_for_in_loop() {
    return false; // Placeholder implementation
}

ParseResult<AstNode> StatementParser::parse_for_variable() {
    return ParseResult<AstNode>::error(
        create_error(ErrorKind::UnexpectedToken, "For variable parsing not implemented yet"));
}

} // namespace Mycelium::Scripting::Lang