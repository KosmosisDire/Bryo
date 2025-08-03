#include "parser/statement_parser.hpp"
#include "parser/parser.hpp"
#include "parser/expression_parser.hpp"
#include "parser/declaration_parser.hpp"
#include <iostream>
#include <magic_enum.hpp>

namespace Myre
{

    StatementParser::StatementParser(Parser *parser) : ParserBase(parser)
    {
    }

    // Main statement parsing entry point
    ParseResult<StatementNode> StatementParser::parse_statement()
    {
        auto &ctx = context();

        // Handle variable declarations as statements
        if (ctx.check(TokenKind::Var))
        {
            auto decl_result = parser_->get_declaration_parser().parse_variable_declaration();
            if (decl_result.is_success())
            {
                auto *var_decl = decl_result.get_node();

                // Handle semicolon
                if (ctx.check(TokenKind::Semicolon))
                {
                    auto *semicolon = parser_->get_allocator().alloc<TokenNode>();
                    semicolon->text = ctx.current().text;
                    semicolon->contains_errors = false;
                    var_decl->semicolon = semicolon;
                    ctx.advance();
                }
                else
                {
                    create_error("Expected ';' after variable declaration");
                    var_decl->contains_errors = true;
                }

                // Return as StatementNode - VariableDeclarationNode is a StatementNode
                return ParseResult<StatementNode>::success(var_decl);
            }
            else
            {
                return ParseResult<StatementNode>::error(decl_result.get_error());
            }
        }

        // Check for typed variable declarations: Type name1, name2 = value;
        // We need to lookahead to distinguish from expression statements
        if (ctx.check(TokenKind::Identifier))
        {
            // Check if next token is also an identifier (Type name pattern)
            if (ctx.peek().kind == TokenKind::Identifier)
            {
                auto typed_decl_result = parser_->get_declaration_parser().parse_typed_variable_declaration();
                if (typed_decl_result.is_success())
                {
                    auto *var_decl = typed_decl_result.get_node();

                    // Handle semicolon
                    if (ctx.check(TokenKind::Semicolon))
                    {
                        auto *semicolon = parser_->get_allocator().alloc<TokenNode>();
                        semicolon->text = ctx.current().text;
                        semicolon->contains_errors = false;
                        var_decl->semicolon = semicolon;
                        ctx.advance();
                    }
                    else
                    {
                        create_error("Expected ';' after variable declaration");
                        var_decl->contains_errors = true;
                    }

                    return ParseResult<StatementNode>::success(var_decl);
                }
                else
                {
                    return ParseResult<StatementNode>::error(typed_decl_result.get_error());
                }
            }
        }

        // Handle control flow statements
        if (ctx.check(TokenKind::If))
        {
            return parse_if_statement();
        }

        if (ctx.check(TokenKind::While))
        {
            return parse_while_statement();
        }

        if (ctx.check(TokenKind::For))
        {
            return parse_for_statement();
        }

        // Handle return statements
        if (ctx.check(TokenKind::Return))
        {
            return parse_return_statement();
        }

        // Handle break statements
        if (ctx.check(TokenKind::Break))
        {
            return parse_break_statement();
        }

        // Handle continue statements
        if (ctx.check(TokenKind::Continue))
        {
            return parse_continue_statement();
        }

        // Handle block statements
        if (ctx.check(TokenKind::LeftBrace))
        {
            return parse_block_statement();
        }

        // Handle expression statements
        return parse_expression_statement();
    }

    // Expression statement parsing
    ParseResult<StatementNode> StatementParser::parse_expression_statement()
    {
        // Handle expression statements
        auto expr_result = parser_->get_expression_parser().parse_expression();

        if (expr_result.is_success())
        {
            auto *expr_stmt = parser_->get_allocator().alloc<ExpressionStatementNode>();
            expr_stmt->expression = expr_result.get_node();
            expr_stmt->contains_errors = ast_has_errors(expr_result.get_ast_node()); // Propagate errors

            // Expect semicolon
            if (parser_->expect(TokenKind::Semicolon, "Expected ';' after expression"))
            {
                // Semicolon consumed successfully
            }

            return ParseResult<StatementNode>::success(expr_stmt);
        }

        return ParseResult<StatementNode>::error(expr_result.get_error());
    }

    // Block statement parsing
    ParseResult<StatementNode> StatementParser::parse_block_statement()
    {
        auto &ctx = context();

        if (!parser_->match(TokenKind::LeftBrace))
        {
            return ParseResult<StatementNode>::error(
                create_error("Expected '{'"));
        }

        auto *block = parser_->get_allocator().alloc<BlockStatementNode>();
        block->contains_errors = false;    // Initialize, will update based on children
        std::vector<AstNode *> statements; // Changed to AstNode* for error integration

        while (!ctx.check(TokenKind::RightBrace) && !ctx.at_end())
        {
            auto stmt_result = parse_statement();

            if (stmt_result.is_success())
            {
                statements.push_back(stmt_result.get_node());
            }
            else if (stmt_result.is_error())
            {
                // Add error node and continue
                statements.push_back(stmt_result.get_error());
                // Simple recovery: skip to next statement boundary
                parser_->recover_to_safe_point(ctx);
            }
            else
            {
                // Fatal error
                break;
            }
        }

        parser_->expect(TokenKind::RightBrace, "Expected '}' to close block");

        // Allocate SizedArray for statements (now using AstNode* for error integration)
        if (!statements.empty())
        {
            auto *stmt_array = parser_->get_allocator().alloc_array<AstNode *>(statements.size());
            bool has_errors = false;
            for (size_t i = 0; i < statements.size(); ++i)
            {
                stmt_array[i] = statements[i];
                if (ast_has_errors(statements[i]))
                {
                    has_errors = true;
                }
            }
            block->statements.values = stmt_array;
            block->statements.size = static_cast<int>(statements.size());
            block->contains_errors = has_errors; // Propagate error flag
        }
        else
        {
            block->contains_errors = false;
        }

        return ParseResult<StatementNode>::success(block);
    }

    // If statement parsing
    ParseResult<StatementNode> StatementParser::parse_if_statement()
    {
        auto &ctx = context();

        ctx.advance(); // consume 'if'

        if (!parser_->match(TokenKind::LeftParen))
        {
            return ParseResult<StatementNode>::error(
                create_error("Expected '(' after 'if'"));
        }

        // Parse condition expression
        auto condition_result = parser_->get_expression_parser().parse_expression();

        parser_->expect(TokenKind::RightParen, "Expected ')' after if condition");

        // Parse then statement
        auto then_result = parse_statement();

        auto *if_stmt = parser_->get_allocator().alloc<IfStatementNode>();
        if_stmt->condition = static_cast<ExpressionNode *>(condition_result.get_ast_node()); // Could be ErrorNode
        if_stmt->thenStatement = static_cast<StatementNode *>(then_result.get_ast_node());   // Could be ErrorNode
        
        // Initialize else clause fields
        if_stmt->elseKeyword = nullptr;
        if_stmt->elseStatement = nullptr;

        // Parse optional else clause
        if (ctx.check(TokenKind::Else))
        {
            // Create and store else keyword token
            auto *else_token = parser_->get_allocator().alloc<TokenNode>();
            else_token->text = ctx.current().text;
            else_token->tokenKind = ctx.current().kind;
            else_token->location = ctx.current().location;
            else_token->contains_errors = false;
            if_stmt->elseKeyword = else_token;
            
            ctx.advance(); // consume 'else'

            // Parse else statement
            auto else_result = parse_statement();
            if_stmt->elseStatement = static_cast<StatementNode *>(else_result.get_ast_node()); // Could be ErrorNode
        }

        // Update error flag
        bool has_errors = ast_has_errors(if_stmt->condition) || ast_has_errors(if_stmt->thenStatement);
        if (if_stmt->elseStatement && ast_has_errors(if_stmt->elseStatement))
        {
            has_errors = true;
        }
        if_stmt->contains_errors = has_errors;

        return ParseResult<StatementNode>::success(if_stmt);
    }

    // While statement parsing with loop context management
    ParseResult<StatementNode> StatementParser::parse_while_statement()
    {
        auto &ctx = context();

        ctx.advance(); // consume 'while'

        if (!parser_->match(TokenKind::LeftParen))
        {
            return ParseResult<StatementNode>::error(
                create_error("Expected '(' after 'while'"));
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

        auto *while_stmt = parser_->get_allocator().alloc<WhileStatementNode>();
        while_stmt->condition = static_cast<ExpressionNode *>(condition_result.get_ast_node()); // Could be ErrorNode
        while_stmt->body = static_cast<StatementNode *>(body_result.get_ast_node());            // Could be ErrorNode

        // Update error flag
        bool has_errors = ast_has_errors(while_stmt->condition) || ast_has_errors(while_stmt->body);
        while_stmt->contains_errors = has_errors;

        return ParseResult<StatementNode>::success(while_stmt);
    }

    ParseResult<StatementNode> StatementParser::parse_for_statement()
    {
        auto &ctx = context();

        ctx.advance(); // consume 'for'

        if (!parser_->match(TokenKind::LeftParen))
        {
            return ParseResult<StatementNode>::error(
                create_error("Expected '(' after 'for'"));
        }

        // Parse initializer (can be variable declaration or expression)
        ParseResult<StatementNode> init_result = ParseResult<StatementNode>::success(nullptr);
        if (!ctx.check(TokenKind::Semicolon))
        {
            if (ctx.check(TokenKind::Var))
            {
                auto var_result = parser_->get_declaration_parser().parse_variable_declaration();
                if (var_result.is_success())
                {
                    init_result = ParseResult<StatementNode>::success(var_result.get_node());
                }
                else
                {
                    init_result = ParseResult<StatementNode>::error(var_result.get_error());
                }
            }
            else if (ctx.check(TokenKind::Identifier) && ctx.peek().kind == TokenKind::Identifier)
            {
                // Typed variable declaration: Type name = value
                auto typed_var_result = parser_->get_declaration_parser().parse_typed_variable_declaration();
                if (typed_var_result.is_success())
                {
                    init_result = ParseResult<StatementNode>::success(typed_var_result.get_node());
                }
                else
                {
                    init_result = ParseResult<StatementNode>::error(typed_var_result.get_error());
                }
            }
            else
            {
                // Parse just the expression, not an expression statement
                auto expr_result = parser_->get_expression_parser().parse_expression();
                if (expr_result.is_success())
                {
                    // Wrap in ExpressionStatementNode for consistency
                    auto *expr_stmt = parser_->get_allocator().alloc<ExpressionStatementNode>();
                    expr_stmt->expression = expr_result.get_node();
                    expr_stmt->contains_errors = ast_has_errors(expr_result.get_ast_node());
                    init_result = ParseResult<StatementNode>::success(expr_stmt);
                }
                else
                {
                    init_result = ParseResult<StatementNode>::error(expr_result.get_error());
                }
            }
        }
        else
        {
            // Empty initializer, still need to consume semicolon
            ctx.advance();
        }

        // Consume semicolon after initializer
        if (!parser_->match(TokenKind::Semicolon))
        {
            return ParseResult<StatementNode>::error(
                create_error("Expected ';' after for initializer"));
        }

        // Parse condition expression (optional)
        ParseResult<ExpressionNode> condition_result = ParseResult<ExpressionNode>::success(nullptr);
        if (!ctx.check(TokenKind::Semicolon))
        {
            condition_result = parser_->get_expression_parser().parse_expression();
        }

        if (!parser_->match(TokenKind::Semicolon))
        {
            return ParseResult<StatementNode>::error(
                create_error("Expected ';' after for condition"));
        }

        // Parse increment expressions (optional, can be multiple separated by commas)
        std::vector<ExpressionNode *> incrementors;
        while (!ctx.check(TokenKind::RightParen) && !ctx.at_end())
        {
            auto incr_result = parser_->get_expression_parser().parse_expression();
            if (incr_result.is_success())
            {
                incrementors.push_back(incr_result.get_node());
            }
            else
            {
                // Error recovery: break on parse failure
                break;
            }

            // Check for comma separator
            if (ctx.check(TokenKind::Comma))
            {
                ctx.advance();
            }
            else if (!ctx.check(TokenKind::RightParen))
            {
                create_error("Expected ',' or ')' in for increment list");
                break;
            }
        }

        if (!parser_->match(TokenKind::RightParen))
        {
            return ParseResult<StatementNode>::error(
                create_error("Expected ')' after for clauses"));
        }

        // Enter loop context for break/continue validation
        auto loop_guard = ctx.save_context();
        ctx.set_loop_context(true);

        // Parse body with loop context active
        auto body_result = parse_statement();

        // Create for statement node
        auto *for_stmt = parser_->get_allocator().alloc<ForStatementNode>();
        for_stmt->initializer = init_result.is_success() ? init_result.get_node() : nullptr;
        for_stmt->condition = condition_result.is_success() ? condition_result.get_node() : nullptr;
        for_stmt->body = body_result.is_success() ? body_result.get_node() : nullptr;

        // Allocate incrementor array
        if (!incrementors.empty())
        {
            auto *incr_array = parser_->get_allocator().alloc_array<ExpressionNode *>(incrementors.size());
            for (size_t i = 0; i < incrementors.size(); ++i)
            {
                incr_array[i] = incrementors[i];
            }
            for_stmt->incrementors.values = incr_array;
            for_stmt->incrementors.size = static_cast<int>(incrementors.size());
        }

        // Update error flag
        bool has_errors = false;
        if (for_stmt->initializer && ast_has_errors(for_stmt->initializer))
            has_errors = true;
        if (for_stmt->condition && ast_has_errors(for_stmt->condition))
            has_errors = true;
        if (for_stmt->body && ast_has_errors(for_stmt->body))
            has_errors = true;
        for (int i = 0; i < for_stmt->incrementors.size; ++i)
        {
            if (ast_has_errors(for_stmt->incrementors[i]))
            {
                has_errors = true;
                break;
            }
        }
        for_stmt->contains_errors = has_errors;

        return ParseResult<StatementNode>::success(for_stmt);
    }

    ParseResult<StatementNode> StatementParser::parse_for_in_statement()
    {
        return ParseResult<StatementNode>::error(
            create_error("For-in statements not implemented yet"));
    }

    ParseResult<StatementNode> StatementParser::parse_return_statement()
    {
        auto &ctx = context();

        // Check if we're in a function context
        if (!ctx.in_function())
        {
            return ParseResult<StatementNode>::error(
                create_error("Return statement outside function"));
        }

        ctx.advance(); // consume 'return'

        auto *return_stmt = parser_->get_allocator().alloc<ReturnStatementNode>();
        return_stmt->contains_errors = false;

        // Optional expression
        if (!ctx.check(TokenKind::Semicolon))
        {
            auto expr_result = parser_->get_expression_parser().parse_expression();
            if (expr_result.is_success())
            {
                return_stmt->expression = expr_result.get_node();
                if (ast_has_errors(return_stmt->expression))
                {
                    return_stmt->contains_errors = true;
                }
            }
            else
            {
                // Set expression to null since we can't cast ErrorNode to ExpressionNode
                // The error will be captured in the error collection via get_error()
                return_stmt->expression = nullptr;
                return_stmt->contains_errors = true;
            }
        }
        else
        {
            return_stmt->expression = nullptr; // void return
        }

        parser_->expect(TokenKind::Semicolon, "Expected ';' after return statement");

        return ParseResult<StatementNode>::success(return_stmt);
    }

    ParseResult<StatementNode> StatementParser::parse_break_statement()
    {
        auto &ctx = context();

        // Check if we're in a loop context
        if (!ctx.in_loop())
        {
            return ParseResult<StatementNode>::error(
                create_error("Break statement outside loop"));
        }

        ctx.advance(); // consume 'break'

        auto *break_stmt = parser_->get_allocator().alloc<BreakStatementNode>();
        break_stmt->contains_errors = false;

        parser_->expect(TokenKind::Semicolon, "Expected ';' after break statement");

        return ParseResult<StatementNode>::success(break_stmt);
    }

    ParseResult<StatementNode> StatementParser::parse_continue_statement()
    {
        auto &ctx = context();

        // Check if we're in a loop context
        if (!ctx.in_loop())
        {
            return ParseResult<StatementNode>::error(
                create_error("Continue statement outside loop"));
        }

        ctx.advance(); // consume 'continue'

        auto *continue_stmt = parser_->get_allocator().alloc<ContinueStatementNode>();
        continue_stmt->contains_errors = false;

        parser_->expect(TokenKind::Semicolon, "Expected ';' after continue statement");

        return ParseResult<StatementNode>::success(continue_stmt);
    }

    // Helper methods
    bool StatementParser::is_for_in_loop()
    {
        return false; // Placeholder implementation
    }

    ParseResult<AstNode> StatementParser::parse_for_variable()
    {
        return ParseResult<AstNode>::error(
            create_error("For variable parsing not implemented yet"));
    }

} // namespace Myre