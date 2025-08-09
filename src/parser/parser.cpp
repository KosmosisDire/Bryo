#include "parser/parser.hpp"
#include "ast/ast_allocator.hpp"

namespace Myre
{

    Parser::Parser(TokenStream &tokens)
        : tokens(tokens)
    {
    }

    Parser::~Parser() = default;

    ParseResult<CompilationUnitNode> Parser::parse()
    {
        auto *unit = alloc.alloc<CompilationUnitNode>();
        std::vector<AstNode *> statements;

        while (!tokens.at_end())
        {
            auto stmt_result = parse_top_level_construct();

            if (stmt_result.is_success())
            {
                statements.push_back(stmt_result.get_node());
            }
            else if (stmt_result.is_error())
            {
                // Add error node and continue
                statements.push_back(stmt_result.get_ast_node());
                // Simple recovery: skip to next statement boundary
                recover_to_safe_point();
            }
            else
            {
                // Fatal error - attempt recovery
                auto cp = tokens.save_checkpoint();
                recover_to_safe_point();

                // Ensure we made progress
                if (!tokens.ahead_of_checkpoint(cp))
                {
                    tokens.advance(); // Force progress
                }

                if (!tokens.at_end())
                {
                    statements.push_back(create_error<StatementNode>("Invalid top-level construct").get_ast_node());
                    continue;
                }
                break;
            }
        }

        // Allocate SizedArray for statements (now using AstNode* for error integration)
        if (!statements.empty())
        {
            auto *stmt_array = alloc.alloc_array<AstNode *>(statements.size());
            for (size_t i = 0; i < statements.size(); ++i)
            {
                stmt_array[i] = statements[i];
            }
            unit->statements.values = stmt_array;
            unit->statements.size = static_cast<int>(statements.size());
        }

        return ParseResult<CompilationUnitNode>::success(unit);
    }

    ParseResult<StatementNode> Parser::parse_top_level_construct()
    {
        if (tokens.check(TokenKind::Using))
        {
            return parse_using_directive();
        }

        // Try parsing as declaration first (using, namespace, fn, type, enum)
        if (check_declaration())
        {
            auto decl_result = parse_declaration();

            // DeclarationNode* is automatically StatementNode* due to inheritance
            if (decl_result.is_success())
            {
                auto *decl = decl_result.get_node();
                return ParseResult<StatementNode>::success(static_cast<StatementNode *>(decl));
            }
            else if (decl_result.is_error())
            {
                return ParseResult<StatementNode>::error(decl_result.get_error());
            }
            else
            {
                return ParseResult<StatementNode>::fatal();
            }
        }

        // Otherwise parse as executable statement (if, while, for, expressions, etc.)
        return parse_statement();
    }

    
    // Main statement parsing entry point
    ParseResult<StatementNode> Parser::parse_statement()
    {
        // Handle control flow statements
        if (tokens.check(TokenKind::If))
        {
            return parse_if_statement();
        }

        if (tokens.check(TokenKind::While))
        {
            return parse_while_statement();
        }

        if (tokens.check(TokenKind::For))
        {
            if (tokens.check_until(TokenKind::In, {TokenKind::RightParen, TokenKind::Semicolon, TokenKind::RightBrace, TokenKind::LeftBrace }))
            {
                return parse_for_in_statement();
            }

            return parse_for_statement();
        }

        // Handle return statements
        if (tokens.check(TokenKind::Return))
        {
            return parse_return_statement();
        }

        // Handle break statements
        if (tokens.check(TokenKind::Break))
        {
            return parse_break_statement();
        }

        // Handle continue statements
        if (tokens.check(TokenKind::Continue))
        {
            return parse_continue_statement();
        }

        // Handle block statements
        if (tokens.check(TokenKind::LeftBrace))
        {
            return parse_block_statement();
        }

        // try to parse a declaration
        if (check_declaration())
        {
            auto decl_result = parse_declaration();
            if (decl_result.is_success())
            {
                return ParseResult<StatementNode>::success(decl_result.get_node());
            }
            else if (decl_result.is_error())
            {
                // If declaration parsing failed, return the error
                return ParseResult<StatementNode>::error(decl_result.get_error());
            }
        }

        // Handle expression statements
        return parse_expression_statement();
    }

    // Expression statement parsing
    ParseResult<StatementNode> Parser::parse_expression_statement()
    {
        // Handle expression statements
        auto expr_result = parse_expression();

        if (expr_result.is_success())
        {
            auto *expr_stmt = alloc.alloc<ExpressionStatementNode>();
            expr_stmt->expression = expr_result.get_node();


            // Expect semicolon
            if (!tokens.match(TokenKind::Semicolon))
            {
                create_error<StatementNode>("Expected ';' after expression");
            }

            return ParseResult<StatementNode>::success(expr_stmt);
        }

        return ParseResult<StatementNode>::error(expr_result.get_error());
    }

    // Block statement parsing
    ParseResult<StatementNode> Parser::parse_block_statement()
    {
        if (!tokens.match(TokenKind::LeftBrace))
        {
            return create_error<StatementNode>("Expected '{'");
        }

        auto *block = alloc.alloc<BlockStatementNode>();

        std::vector<AstNode *> statements; // Changed to AstNode* for error integration

        while (!tokens.check(TokenKind::RightBrace) && !tokens.at_end())
        {
            auto stmt_result = parse_statement();

            if (stmt_result.is_success())
            {
                statements.push_back(stmt_result.get_node());
            }
            else if (stmt_result.is_error())
            {
                // Add error node and continue
                statements.push_back(stmt_result.get_ast_node());
                recover_to_safe_point();
            }
            else
            {
                // Fatal error
                break;
            }
        }

        if (!tokens.match(TokenKind::RightBrace))
        {
            create_error<StatementNode>("Expected '}' to close block");
        }

        // Allocate SizedArray for statements (now using AstNode* for error integration)
        if (!statements.empty())
        {
            auto *stmt_array = alloc.alloc_array<AstNode *>(statements.size());
            for (size_t i = 0; i < statements.size(); ++i)
            {
                stmt_array[i] = statements[i];
            }
            block->statements.values = stmt_array;
            block->statements.size = static_cast<int>(statements.size());

        }

        return ParseResult<StatementNode>::success(block);
    }

    // If statement parsing
    ParseResult<StatementNode> Parser::parse_if_statement()
    {
        tokens.advance(); // consume 'if'

        if (!tokens.match(TokenKind::LeftParen))
        {
            return create_error<StatementNode>("Expected '(' after 'if'");
        }

        // Parse condition expression
        auto condition_result = parse_expression();

        if (!tokens.match(TokenKind::RightParen))
        {
            create_error<StatementNode>("Expected ')' after if condition");
        }

        // Parse then statement
        auto then_result = parse_statement();

        auto *if_stmt = alloc.alloc<IfStatementNode>();
        if_stmt->condition = static_cast<ExpressionNode *>(condition_result.get_ast_node()); // Could be ErrorNode
        if_stmt->thenStatement = static_cast<StatementNode *>(then_result.get_ast_node());   // Could be ErrorNode

        // Initialize else clause fields
        if_stmt->elseKeyword = nullptr;
        if_stmt->elseStatement = nullptr;

        // Parse optional else clause
        if (tokens.check(TokenKind::Else))
        {
            // Create and store else keyword token
            auto *else_token = alloc.alloc<TokenNode>();
            else_token->text = tokens.current().text;
            else_token->location = tokens.current().location;

            if_stmt->elseKeyword = else_token;

            tokens.advance(); // consume 'else'

            // Parse else statement
            auto else_result = parse_statement();
            if_stmt->elseStatement = static_cast<StatementNode *>(else_result.get_ast_node()); // Could be ErrorNode
        }

        return ParseResult<StatementNode>::success(if_stmt);
    }

    // While statement parsing with loop context management
    ParseResult<StatementNode> Parser::parse_while_statement()
    {
        tokens.advance(); // consume 'while'

        if (!tokens.match(TokenKind::LeftParen))
        {
            return create_error<StatementNode>("Expected '(' after 'while'");
        }

        // Parse condition expression
        auto condition_result = parse_expression();

        if (!tokens.match(TokenKind::RightParen))
        {
            create_error<StatementNode>("Expected ')' after while condition");
        }

        // Parse body with loop context active
        auto body_result = parse_statement();

        // Loop guard automatically restores context when destroyed

        auto *while_stmt = alloc.alloc<WhileStatementNode>();
        while_stmt->condition = static_cast<ExpressionNode *>(condition_result.get_ast_node()); // Could be ErrorNode
        while_stmt->body = static_cast<StatementNode *>(body_result.get_ast_node());            // Could be ErrorNode

        return ParseResult<StatementNode>::success(while_stmt);
    }

    ParseResult<StatementNode> Parser::parse_for_statement()
    {
        

        tokens.advance(); // consume 'for'

        if (!tokens.match(TokenKind::LeftParen))
        {
            return create_error<StatementNode>("Expected '(' after 'for'");
        }

        // Parse initializer (can be variable declaration or expression)
        ParseResult<StatementNode> init_result = ParseResult<StatementNode>::success(nullptr);
        if (!tokens.check(TokenKind::Semicolon))
        {
            if (tokens.check(TokenKind::Var) || tokens.check(TokenKind::Identifier) && tokens.peek().kind == TokenKind::Identifier)
            {
                auto var_result = parse_variable_declaration();
                if (var_result.is_success())
                {
                    init_result = ParseResult<StatementNode>::success(var_result.get_node());
                }
                else
                {
                    init_result = ParseResult<StatementNode>::error(var_result.get_error());
                }
            }
            else
            {
                // Parse just the expression, not an expression statement
                auto expr_result = parse_expression();
                if (expr_result.is_success())
                {
                    // Wrap in ExpressionStatementNode for consistency
                    auto *expr_stmt = alloc.alloc<ExpressionStatementNode>();
                    expr_stmt->expression = expr_result.get_node();

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
            tokens.advance();
        }

        // Consume semicolon after initializer
        if (!tokens.match(TokenKind::Semicolon))
        {
            create_error<StatementNode>("Expected ';' after for initializer");
        }

        // Parse condition expression (optional)
        ParseResult<ExpressionNode> condition_result = ParseResult<ExpressionNode>::success(nullptr);
        if (!tokens.check(TokenKind::Semicolon))
        {
            condition_result = parse_expression();
        }

        if (!tokens.match(TokenKind::Semicolon))
        {
            create_error<StatementNode>("Expected ';' after for condition");
        }

        // Parse increment expressions (optional, can be multiple separated by commas)
        std::vector<ExpressionNode *> incrementors;
        while (!tokens.check(TokenKind::RightParen) && !tokens.at_end())
        {
            auto incr_result = parse_expression();
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
            if (tokens.check(TokenKind::Comma))
            {
                tokens.advance();
            }
            else if (!tokens.check(TokenKind::RightParen))
            {
                create_error<StatementNode>("Expected ',' or ')' in for increment list");
                break;
            }
        }

        if (!tokens.match(TokenKind::RightParen))
        {
            create_error<StatementNode>("Expected ')' after for clauses");
        }

        // Parse body with loop context active
        auto body_result = parse_statement();

        // Create for statement node
        auto *for_stmt = alloc.alloc<ForStatementNode>();
        for_stmt->initializer = init_result.is_success() ? init_result.get_node() : nullptr;
        for_stmt->condition = condition_result.is_success() ? condition_result.get_node() : nullptr;
        for_stmt->body = body_result.is_success() ? body_result.get_node() : nullptr;

        // Allocate incrementor array
        if (!incrementors.empty())
        {
            auto *incr_array = alloc.alloc_array<ExpressionNode *>(incrementors.size());
            for (size_t i = 0; i < incrementors.size(); ++i)
            {
                incr_array[i] = incrementors[i];
            }
            for_stmt->incrementors.values = incr_array;
            for_stmt->incrementors.size = static_cast<int>(incrementors.size());
        }

        return ParseResult<StatementNode>::success(for_stmt);
    }

    ParseResult<StatementNode> Parser::parse_for_in_statement()
    {
        

        // Create the for-in node
        auto *for_in_stmt = alloc.alloc<ForInStatementNode>();


        // Store 'for' keyword
        auto *for_token = alloc.alloc<TokenNode>();
        for_token->text = tokens.current().text;
        for_token->location = tokens.current().location;

        for_in_stmt->forKeyword = for_token;

        tokens.advance(); // consume 'for'

        // Store '('
        if (!tokens.check(TokenKind::LeftParen))
        {
            return create_error<StatementNode>("Expected '(' after 'for'");
        }
        auto *open_paren = alloc.alloc<TokenNode>();
        open_paren->text = tokens.current().text;
        open_paren->location = tokens.current().location;

        for_in_stmt->openParen = open_paren;
        tokens.advance(); // consume '('

        // Parse main variable using declaration parser helper
        auto main_var_result = parse_for_variable_declaration();
        if (!main_var_result.is_success())
        {
            return ParseResult<StatementNode>::error(main_var_result.get_error());
        }
        for_in_stmt->mainVariable = main_var_result.get_node();

        // Expect 'in' keyword
        if (!tokens.check(TokenKind::In))
        {
            return create_error<StatementNode>("Expected 'in' in for-in statement");
        }
        auto *in_token = alloc.alloc<TokenNode>();
        in_token->text = tokens.current().text;
        in_token->location = tokens.current().location;

        for_in_stmt->inKeyword = in_token;
        tokens.advance(); // consume 'in'

        // Parse iterable expression (could be range or collection)
        auto iterable_result = parse_expression();
        if (!iterable_result.is_success())
        {
            return create_error<StatementNode>("Expected iterable expression after 'in'");
        }
        for_in_stmt->iterable = iterable_result.get_node();

        // Check for optional 'at' clause
        for_in_stmt->atKeyword = nullptr;
        for_in_stmt->indexVariable = nullptr;
        if (tokens.check(TokenKind::At))
        {
            auto *at_token = alloc.alloc<TokenNode>();
            at_token->text = tokens.current().text;
            at_token->location = tokens.current().location;

            for_in_stmt->atKeyword = at_token;
            tokens.advance(); // consume 'at'

            // Parse index variable using declaration parser helper
            auto index_var_result = parse_for_variable_declaration();
            if (!index_var_result.is_success())
            {
                return ParseResult<StatementNode>::error(index_var_result.get_error());
            }
            for_in_stmt->indexVariable = index_var_result.get_node();
        }

        // Expect ')'
        if (!tokens.check(TokenKind::RightParen))
        {
            return create_error<StatementNode>("Expected ')' after for-in clauses");
        }
        auto *close_paren = alloc.alloc<TokenNode>();
        close_paren->text = tokens.current().text;
        close_paren->location = tokens.current().location;

        for_in_stmt->closeParen = close_paren;
        tokens.advance(); // consume ')'

        // Parse body with loop context active
        auto body_result = parse_statement();
        if (!body_result.is_success())
        {
            return ParseResult<StatementNode>::error(body_result.get_error());
        }
        for_in_stmt->body = body_result.get_node();

        return ParseResult<StatementNode>::success(for_in_stmt);
    }

    ParseResult<StatementNode> Parser::parse_return_statement()
    {
        // Check if we're in a function context
        // if (!tokens.in_function())
        // {
        //     return create_error<StatementNode>("Return statement outside function");
        // }

        tokens.advance(); // consume 'return'

        auto *return_stmt = alloc.alloc<ReturnStatementNode>();


        // Optional expression
        if (!tokens.check(TokenKind::Semicolon))
        {
            auto expr_result = parse_expression();
            if (expr_result.is_success())
            {
                return_stmt->expression = expr_result.get_node();
            }
            else
            {
                // Set expression to null since we can't cast ErrorNode to ExpressionNode
                // The error will be captured in the error collection via get_error()
                return_stmt->expression = nullptr;

            }
        }
        else
        {
            return_stmt->expression = nullptr; // void return
        }

        if (!tokens.match(TokenKind::Semicolon))
        {
            create_error<StatementNode>("Expected ';' after return statement");
        }

        return ParseResult<StatementNode>::success(return_stmt);
    }

    ParseResult<StatementNode> Parser::parse_break_statement()
    {
        // Check if we're in a loop context
        // if (!tokens.in_loop())
        // {
        //     return create_error<StatementNode>("Break statement outside loop");
        // }

        tokens.advance(); // consume 'break'

        auto *break_stmt = alloc.alloc<BreakStatementNode>();

        if (!tokens.match(TokenKind::Semicolon))
        {
            create_error<StatementNode>("Expected ';' after break statement");
        }

        return ParseResult<StatementNode>::success(break_stmt);
    }

    ParseResult<StatementNode> Parser::parse_continue_statement()
    {
        // Check if we're in a loop context
        // if (!tokens.in_loop())
        // {
        //     return create_error("Continue statement outside loop");
        // }

        tokens.advance(); // consume 'continue'

        auto *continue_stmt = alloc.alloc<ContinueStatementNode>();

        if (!tokens.match(TokenKind::Semicolon))
        {
            create_error<StatementNode>("Expected ';' after continue statement");
        }

        return ParseResult<StatementNode>::success(continue_stmt);
    }

    bool Parser::check_declaration()
    {
        // check forward until the next semicolon and see if we come across a declaration keyword
        
        if (tokens.at_end())
            return false;

        // Check for declaration keywords
        int offset = 0;
        while (true)
        {
            auto token = tokens.peek(offset);
            if (token.is(TokenKind::Semicolon) || token.is(TokenKind::RightBrace) || token.is(TokenKind::EndOfFile) || token.is(TokenKind::RightParen))
            {
                // Reached a semicolon, no declaration found
                return false;
            }

            if (token.is_eof())
            {
                // Reached end of file, no declaration found
                return false;
            }

            if (offset > 20)
            {
                // Too far ahead, stop checking
                return false;
            }

            if (token.is_any({TokenKind::Fn, TokenKind::Var, TokenKind::Type, TokenKind::Enum, TokenKind::Using, TokenKind::Namespace}))
            {
                // Found a declaration keyword
                return true;
            }

            offset++;
        }

        return false;
    }

    // Main declaration parsing entry point
    ParseResult<DeclarationNode> Parser::parse_declaration()
    {
        

        // Parse access modifiers and other modifiers first
        std::vector<ModifierKind> modifiers = parse_all_modifiers();

        // Determine declaration type by looking at the current token
        // Note: Using directives are handled at top-level, not here

        if (tokens.check(TokenKind::Namespace))
        {
            return parse_namespace_declaration();
        }

        if (tokens.check(TokenKind::Fn))
        {
            return parse_function_declaration();
        }

        if (tokens.check(TokenKind::New))
        {
            // Lookahead to confirm it's a constructor (has left paren)
            if (tokens.peek(1).kind == TokenKind::LeftParen)
            {
                return parse_constructor_declaration();
            }
        }

        if (tokens.check(TokenKind::Type))
        {
            return parse_type_declaration();
        }

        if (tokens.check(TokenKind::Enum))
        {
            return parse_enum_declaration();
        }

        if (tokens.check(TokenKind::Var) || (tokens.check(TokenKind::Identifier) && tokens.peek().kind == TokenKind::Identifier))
        {
            return parse_variable_declaration();
        }

        return ParseResult<DeclarationNode>::none();
    }

    // Function declaration parsing
    ParseResult<DeclarationNode> Parser::parse_function_declaration()
    {
        tokens.advance(); // consume 'fn'

        if (!tokens.check(TokenKind::Identifier))
        {
            return create_error<DeclarationNode>("Expected function name");
        }

        const Token &name_token = tokens.current();
        tokens.advance();

        auto *func_decl = alloc.alloc<FunctionDeclarationNode>();

        // Set up name
        auto *name_node = alloc.alloc<IdentifierNode>();
        name_node->name = name_token.text;
        func_decl->name = name_node;

        // Parse parameter list
        if (!tokens.match(TokenKind::LeftParen))
        {
            return create_error<DeclarationNode>("Expected '(' after function name");
        }

        // Parse parameter list and store the parameters
        std::vector<ParameterNode *> params;

        // Parse parameter list: param1: Type1, param2: Type2, ...
        while (!tokens.check(TokenKind::RightParen) && !tokens.at_end())
        {
            auto param_result = parse_parameter();
            if (param_result.is_success())
            {
                params.push_back(param_result.get_node());
            }
            else
            {
                // Error recovery: skip to next parameter or end
                recover_to_safe_point();
                if (tokens.check(TokenKind::RightParen))
                    break;
            }

            // Check for comma separator
            if (tokens.check(TokenKind::Comma))
            {
                tokens.advance();
            }
            else if (!tokens.check(TokenKind::RightParen))
            {
                create_error<FunctionDeclarationNode>("Expected ',' or ')' in parameter list");
                break;
            }
        }

        // Store parameters in the function declaration
        if (!params.empty())
        {
            auto *param_array = alloc.alloc_array<AstNode *>(params.size());
            for (size_t i = 0; i < params.size(); ++i)
            {
                param_array[i] = params[i];
            }
            func_decl->parameters.values = param_array;
            func_decl->parameters.size = static_cast<int>(params.size());
        }

        if (!tokens.match(TokenKind::RightParen))
        {
            create_error<FunctionDeclarationNode>("Expected ')' after parameters");
        }

        // Parse optional return type: fn test(): i32
        if (tokens.match(TokenKind::Colon))
        {
            auto return_type_result = parse_type_expression();
            if (return_type_result.is_success())
            {
                func_decl->returnType = return_type_result.get_node();
            }
        }

        // Parse function body
        if (tokens.check(TokenKind::LeftBrace))
        {
            auto body_result = parse_block_statement();
            if (body_result.is_success())
            {
                func_decl->body = static_cast<BlockStatementNode *>(body_result.get_node());
            }
        }
        else
        {
            // Missing body - create error but continue
            create_error<FunctionDeclarationNode>("Expected '{' for function body");
        }

        return ParseResult<DeclarationNode>::success(func_decl);
    }

    ParseResult<DeclarationNode> Parser::parse_constructor_declaration()
    {
        

        // Store the 'new' keyword
        auto *new_keyword = alloc.alloc<TokenNode>();
        new_keyword->text = tokens.current().text;
        tokens.advance(); // consume 'new'

        auto *ctor_decl = alloc.alloc<ConstructorDeclarationNode>();
        ctor_decl->newKeyword = new_keyword;

        // Parse opening paren
        if (!tokens.check(TokenKind::LeftParen))
        {
            return create_error<DeclarationNode>("Expected '(' after 'new' in constructor");
        }

        auto *open_paren = alloc.alloc<TokenNode>();
        open_paren->text = tokens.current().text;
        ctor_decl->openParen = open_paren;
        tokens.advance(); // consume '('

        // Parse parameters (reuse existing parameter parsing logic)
        std::vector<ParameterNode *> params;
        while (!tokens.check(TokenKind::RightParen) && !tokens.at_end())
        {
            auto param_result = parse_parameter();
            if (param_result.is_success())
            {
                params.push_back(param_result.get_node());
            }
            else
            {
                recover_to_safe_point();
                if (tokens.check(TokenKind::RightParen))
                    break;
            }

            if (tokens.check(TokenKind::Comma))
            {
                tokens.advance();
            }
            else if (!tokens.check(TokenKind::RightParen))
            {
                create_error<DeclarationNode>("Expected ',' or ')' in parameter list");
                break;
            }
        }

        // Store parameters
        if (!params.empty())
        {
            auto *param_array = alloc.alloc_array<ParameterNode *>(params.size());
            for (size_t i = 0; i < params.size(); ++i)
            {
                param_array[i] = params[i];
            }
            ctor_decl->parameters.values = param_array;
            ctor_decl->parameters.size = static_cast<int>(params.size());
        }

        // Parse closing paren
        if (!tokens.check(TokenKind::RightParen))
        {
            create_error<DeclarationNode>("Expected ')' after constructor parameters");
            ctor_decl->closeParen = nullptr;
        }
        else
        {
            auto *close_paren = alloc.alloc<TokenNode>();
            close_paren->text = tokens.current().text;
            ctor_decl->closeParen = close_paren;
            tokens.advance(); // consume ')'
        }

        // Parse constructor body
        if (!tokens.check(TokenKind::LeftBrace))
        {
            return create_error<DeclarationNode>("Expected '{' for constructor body");
        }

        // Parse body (can reuse block statement parsing)
        auto body_result = parse_block_statement();
        if (body_result.is_success())
        {
            ctor_decl->body = static_cast<BlockStatementNode *>(body_result.get_node());
        }

        return ParseResult<DeclarationNode>::success(ctor_decl);
    }

    // Type declaration parsing
    ParseResult<DeclarationNode> Parser::parse_type_declaration()
    {
        

        // Store the type keyword token
        auto *type_keyword = alloc.alloc<TokenNode>();
        type_keyword->text = tokens.current().text;

        tokens.advance(); // consume 'type'

        if (!tokens.check(TokenKind::Identifier))
        {
            return create_error<DeclarationNode>("Expected type name");
        }

        const Token &name_token = tokens.current();
        tokens.advance();

        auto *type_decl = alloc.alloc<TypeDeclarationNode>();
        type_decl->typeKeyword = type_keyword;

        // Set up name
        auto *name_node = alloc.alloc<IdentifierNode>();
        name_node->name = name_token.text;
        type_decl->name = name_node;

        // Parse type body
        if (!tokens.match(TokenKind::LeftBrace))
        {
            return create_error<DeclarationNode>("Expected '{' for type body");
        }

        // Store the opening brace
        auto *open_brace = alloc.alloc<TokenNode>();
        open_brace->text = "{";
        type_decl->openBrace = open_brace;

        std::vector<AstNode *> members;

        while (!tokens.check(TokenKind::RightBrace) && !tokens.at_end())
        {
            auto member_result = parse_declaration();

            // consume semicolon if present
            if (tokens.check(TokenKind::Semicolon))
            {
                tokens.advance();
            }

            if (member_result.is_success())
            {
                members.push_back(member_result.get_node());
            }
            else if (member_result.is_error())
            {
                // Add error node and continue
                members.push_back(member_result.get_error());
                recover_to_safe_point();
            }
            else
            {
                // Fatal error
                break;
            }
        }

        if (!tokens.match(TokenKind::RightBrace))
        {
            create_error<TypeDeclarationNode>("Expected '}' to close type");

            type_decl->closeBrace = nullptr;
        }
        else
        {
            // Store the closing brace
            auto *close_brace = alloc.alloc<TokenNode>();
            close_brace->text = "}";

            type_decl->closeBrace = close_brace;
        }

        // Allocate member array
        if (!members.empty())
        {
            auto *member_array = alloc.alloc_array<AstNode *>(members.size());
            for (size_t i = 0; i < members.size(); ++i)
            {
                member_array[i] = members[i];
            }
            type_decl->members.values = member_array;
            type_decl->members.size = static_cast<int>(members.size());
        }

        return ParseResult<DeclarationNode>::success(type_decl);
    }

    // Enum declaration parsing
    ParseResult<DeclarationNode> Parser::parse_enum_declaration()
    {
        tokens.advance(); // consume 'enum'

        if (!tokens.check(TokenKind::Identifier))
        {
            return create_error<DeclarationNode>("Expected enum name");
        }

        const Token &name_token = tokens.current();
        tokens.advance();

        auto *enum_decl = alloc.alloc<EnumDeclarationNode>();


        // Set up name
        auto *name_node = alloc.alloc<IdentifierNode>();
        name_node->name = name_token.text;

        enum_decl->name = name_node;

        if (!tokens.match(TokenKind::LeftBrace))
        {
            return create_error<DeclarationNode>("Expected '{' for enum body");
        }

        std::vector<EnumCaseNode *> cases;

        while (!tokens.check(TokenKind::RightBrace) && !tokens.at_end())
        {
            auto case_result = parse_enum_case();

            if (case_result.is_success())
            {
                cases.push_back(case_result.get_node());
            }
            else
            {
                // Simple recovery: skip to next case or end
                recover_to_safe_point();
                if (tokens.check(TokenKind::RightBrace))
                    break;
            }

            // Optional comma between cases
            if (tokens.check(TokenKind::Comma))
            {
                tokens.advance();
            }
        }

        if (!tokens.match(TokenKind::RightBrace))
        {
            create_error<EnumDeclarationNode>("Expected '}' to close enum");

        }

        // Allocate cases array
        if (!cases.empty())
        {
            auto *case_array = alloc.alloc_array<EnumCaseNode *>(cases.size());
            for (size_t i = 0; i < cases.size(); ++i)
            {
                case_array[i] = cases[i];
            }
            enum_decl->cases.values = case_array;
            enum_decl->cases.size = static_cast<int>(cases.size());
        }

        return ParseResult<DeclarationNode>::success(enum_decl);
    }

    // Using directive parsing
    ParseResult<StatementNode> Parser::parse_using_directive()
    {
        

        tokens.advance(); // consume 'using'

        auto type_result = parse_qualified_name();
        if (!type_result.is_success())
        {
            return create_error<StatementNode>("Expected qualified name after 'using'");
        }

        auto *using_stmt = alloc.alloc<UsingDirectiveNode>();
        using_stmt->namespaceName = type_result.get_node();

        if (!tokens.match(TokenKind::Semicolon))
        {
            create_error<StatementNode>("Expected ';' after using directive");
        }

        return ParseResult<StatementNode>::success(using_stmt);
    }

    // Helper method implementations
    ParseResult<AstNode> Parser::parse_parameter_list()
    {
        std::vector<ParameterNode *> params;
        

        // Parse parameter list: param1: Type1, param2: Type2, ...
        while (!tokens.check(TokenKind::RightParen) && !tokens.at_end())
        {
            auto param_result = parse_parameter();
            if (param_result.is_success())
            {
                params.push_back(param_result.get_node());
            }
            else
            {
                // Error recovery: skip to next parameter or end
                recover_to_safe_point();
                if (tokens.check(TokenKind::RightParen))
                    break;
            }

            // Check for comma separator
            if (tokens.check(TokenKind::Comma))
            {
                tokens.advance();
            }
            else if (!tokens.check(TokenKind::RightParen))
            {
                create_error<FunctionDeclarationNode>("Expected ',' or ')' in parameter list");
                break;
            }
        }

        // We'll return a wrapper node that contains the parameter list
        // For now, just return success to indicate we processed parameters
        return ParseResult<AstNode>::success(nullptr);
    }

    ParseResult<EnumCaseNode> Parser::parse_enum_case()
    {
        

        if (!tokens.check(TokenKind::Identifier))
        {
            return create_error<EnumCaseNode>("Expected case name");
        }

        const Token &name_token = tokens.current();
        tokens.advance();

        auto *case_node = alloc.alloc<EnumCaseNode>();


        // Set up name
        auto *name_node = alloc.alloc<IdentifierNode>();
        name_node->name = name_token.text;

        case_node->name = name_node;

        // Check for associated data: Case(param1: Type1, param2: Type2)
        if (tokens.check(TokenKind::LeftParen))
        {
            tokens.advance(); // consume '('

            std::vector<ParameterNode *> params;

            while (!tokens.check(TokenKind::RightParen) && !tokens.at_end())
            {
                auto param_result = parse_enum_parameter();
                if (param_result.is_success())
                {
                    params.push_back(param_result.get_node());
                }
                else
                {

                    break;
                }

                if (tokens.check(TokenKind::Comma))
                {
                    tokens.advance();
                }
                else if (!tokens.check(TokenKind::RightParen))
                {
                    create_error<FunctionDeclarationNode>("Expected ',' or ')' in parameter list");

                    break;
                }
            }

            if (!tokens.match(TokenKind::RightParen))
            {
                create_error<EnumCaseNode>("Expected ')' after parameters");

            }

            // Allocate parameter array
            if (!params.empty())
            {
                auto *param_array = alloc.alloc_array<ParameterNode *>(params.size());
                for (size_t i = 0; i < params.size(); ++i)
                {
                    param_array[i] = params[i];
                }
                case_node->associatedData.values = param_array;
                case_node->associatedData.size = static_cast<int>(params.size());
            }
        }

        return ParseResult<EnumCaseNode>::success(case_node);
    }

    ParseResult<ParameterNode> Parser::parse_parameter()
    {
        

        // Myre syntax: Type name (not name: Type)
        // First parse the type
        auto type_result = parse_type_expression();
        if (!type_result.is_success())
        {
            return create_error<ParameterNode>("Expected parameter type");
        }

        // Then parse the parameter name
        if (!tokens.check(TokenKind::Identifier))
        {
            return create_error<ParameterNode>("Expected parameter name after type");
        }

        const Token &name_token = tokens.current();
        tokens.advance();

        auto *param = alloc.alloc<ParameterNode>();


        // Set up name
        auto *name_node = alloc.alloc<IdentifierNode>();
        name_node->name = name_token.text;

        param->name = name_node;

        // Set up type
        param->type = type_result.get_node();

        // Check for default value: = value
        if (tokens.match(TokenKind::Assign))
        {
            auto default_result = parse_expression();
            if (default_result.is_success())
            {
                param->defaultValue = default_result.get_node();
            }
            else
            {

            }
        }

        return ParseResult<ParameterNode>::success(param);
    }

    ParseResult<ParameterNode> Parser::parse_enum_parameter()
    {
        // Parse type first
        auto type_result = parse_type_expression();
        if (!type_result.is_success())
        {
            return create_error<ParameterNode>("Expected parameter type");
        }

        auto *param = alloc.alloc<ParameterNode>();

        param->type = type_result.get_node();
        param->defaultValue = nullptr; // No default values for enum parameters

        // Check if there's a parameter name after the type
        if (tokens.check(TokenKind::Identifier))
        {
            // This is the "Type name" syntax: i32 priority, string message
            const Token &name_token = tokens.current();
            tokens.advance();

            auto *name_node = alloc.alloc<IdentifierNode>();
            name_node->name = name_token.text;

            param->name = name_node;
        }
        else
        {
            // This is the "Type only" syntax: i32, i32, i32
            param->name = nullptr;
        }

        return ParseResult<ParameterNode>::success(param);
    }

    // Helper method implementations
    std::vector<ModifierKind> Parser::parse_all_modifiers()
    {
        std::vector<ModifierKind> modifiers;
        

        while (true)
        {
            auto token = tokens.current();
            if (token.is_modifier())
            {
                tokens.advance(); // consume modifier
                modifiers.push_back(token.to_modifier_kind());
            }
            else
            {
                break;
            }
        }

        return modifiers;
    }

    ParseResult<DeclarationNode> Parser::parse_namespace_declaration()
    {
        

        tokens.advance(); // consume 'namespace'

        if (!tokens.check(TokenKind::Identifier))
        {
            return create_error<DeclarationNode>("Expected namespace name");
        }

        // Set up name
        auto name_node = parse_qualified_name();
        if (name_node.is_error())
        {
            return create_error<DeclarationNode>("Failed to parse namespace name");
        }

        auto *namespace_decl = alloc.alloc<NamespaceDeclarationNode>();
        namespace_decl->name = name_node.get_node();

        // Parse namespace body
        if (!tokens.check(TokenKind::LeftBrace))
        {
            // look for a semicolon instead because it may be a file-level namespace
            if (!tokens.match(TokenKind::Semicolon))
            {
                create_error<DeclarationNode>("Expected '{' for namespace body or ';' for file-scoped namespace");
            }

            return ParseResult<DeclarationNode>::success(namespace_decl);
        } 

        // use statement parser for block
        auto body_result = parse_block_statement();
        if (body_result.is_success())
        {
            namespace_decl->body = static_cast<BlockStatementNode *>(body_result.get_node());
        }
        else
        {

        }

        return ParseResult<DeclarationNode>::success(namespace_decl);
    }

    ParseResult<AstNode> Parser::parse_generic_parameters()
    {
        return create_error<AstNode>("Generic parameters not implemented yet");
    }

    ParseResult<AstNode> Parser::parse_generic_constraints()
    {
        return create_error<AstNode>("Generic constraints not implemented yet");
    }

    // Helper method for parsing variable declarations in for-in context
    ParseResult<StatementNode> Parser::parse_for_variable_declaration()
    {
        

        // Check for 'var' keyword
        if (tokens.check(TokenKind::Var))
        {
            // Store var keyword
            auto *var_keyword = alloc.alloc<TokenNode>();
            var_keyword->text = tokens.current().text;
            var_keyword->location = tokens.current().location;


            tokens.advance(); // consume 'var'

            if (!tokens.check(TokenKind::Identifier))
            {
                return create_error<StatementNode>("Expected identifier after 'var'");
            }

            // Create a simplified variable declaration node
            auto *var_decl = alloc.alloc<VariableDeclarationNode>();
            var_decl->varKeyword = var_keyword;
            var_decl->type = nullptr; // var declarations don't have explicit type

            // Store identifier in names array (single element)
            auto *id_node = alloc.alloc<IdentifierNode>();
            id_node->name = tokens.current().text;
            id_node->location = tokens.current().location;


            auto *names_array = alloc.alloc_array<IdentifierNode *>(1);
            names_array[0] = id_node;
            var_decl->names.values = names_array;
            var_decl->names.size = 1;

            tokens.advance(); // consume identifier

            // No initializer in for-in context
            var_decl->initializer = nullptr;
            var_decl->equalsToken = nullptr;
            var_decl->semicolon = nullptr;


            return ParseResult<StatementNode>::success(var_decl);
        }

        // Check for TypeName identifier pattern or just identifier
        if (tokens.check(TokenKind::Identifier))
        {
            // Save position in case we need to backtrack
            auto saved_pos = tokens.save_checkpoint();

            // Try to parse as type + identifier
            auto type_result = parse_type_expression();
            if (type_result.is_success() && tokens.check(TokenKind::Identifier))
            {
                // Create typed variable declaration
                auto *var_decl = alloc.alloc<VariableDeclarationNode>();
                var_decl->varKeyword = nullptr;
                var_decl->type = type_result.get_node();

                // Store identifier in names array
                auto *id_node = alloc.alloc<IdentifierNode>();
                id_node->name = tokens.current().text;
                id_node->location = tokens.current().location;


                auto *names_array = alloc.alloc_array<IdentifierNode *>(1);
                names_array[0] = id_node;
                var_decl->names.values = names_array;
                var_decl->names.size = 1;

                tokens.advance(); // consume identifier

                var_decl->initializer = nullptr;
                var_decl->equalsToken = nullptr;
                var_decl->semicolon = nullptr;


                return ParseResult<StatementNode>::success(var_decl);
            }

            // Reset position if type parsing failed or no identifier follows
            tokens.restore_checkpoint(saved_pos);

            // Treat as just an identifier reference (pre-declared variable)
            auto *id_node = alloc.alloc<IdentifierNode>();
            id_node->name = tokens.current().text;
            id_node->location = tokens.current().location;

            tokens.advance(); // consume identifier

            // Create an IdentifierExpressionNode for consistency
            auto *id_expr = alloc.alloc<IdentifierExpressionNode>();
            id_expr->identifier = id_node;


            // Wrap in an expression statement to match StatementNode type
            auto *expr_stmt = alloc.alloc<ExpressionStatementNode>();
            expr_stmt->expression = id_expr;


            return ParseResult<StatementNode>::success(expr_stmt);
        }

        return create_error<StatementNode>("Expected variable declaration in for-in statement");
    }

    ParseResult<DeclarationNode> Parser::parse_variable_declaration()
    {
        
        TokenNode *var_keyword = nullptr;
        TypeNameNode *type = nullptr;
        if (tokens.check(TokenKind::Var))
        {
            // Store the var keyword token
            var_keyword = alloc.alloc<TokenNode>();
            var_keyword->text = tokens.current().text;


            tokens.advance(); // consume 'var'
        }
        else if (tokens.check(TokenKind::Identifier) && tokens.peek().kind == TokenKind::Identifier)
        {
            // Typed variable declaration: Type name = value
            auto type_name = parse_type_expression();
            if (type_name.is_success())
            {
                type = type_name.get_node();
            }
        }
        else
        {
            return create_error<DeclarationNode>("Unsupported variable declaration syntax");
        }

        // Parse variable names (can be multiple: var x, y, z = 0)
        std::vector<IdentifierNode *> names;

        if (!tokens.check(TokenKind::Identifier))
        {
            return create_error<DeclarationNode>("Expected identifier after 'var'");
        }

        // Parse first name
        const Token &first_name_token = tokens.current();
        tokens.advance();

        auto *first_name = alloc.alloc<IdentifierNode>();
        first_name->name = first_name_token.text;

        names.push_back(first_name);

        // Parse additional names if comma-separated
        while (tokens.check(TokenKind::Comma))
        {
            tokens.advance(); // consume comma

            if (!tokens.check(TokenKind::Identifier))
            {
                return create_error<DeclarationNode>("Expected identifier after ','");
            }

            const Token &name_token = tokens.current();
            tokens.advance();

            auto *name_node = alloc.alloc<IdentifierNode>();
            name_node->name = name_token.text;

            names.push_back(name_node);
        }

        auto *var_decl = alloc.alloc<VariableDeclarationNode>();

        var_decl->varKeyword = var_keyword;
        var_decl->type = type;

        // Allocate and populate names array
        auto *names_array = alloc.alloc_array<IdentifierNode *>(names.size());
        for (size_t i = 0; i < names.size(); ++i)
        {
            names_array[i] = names[i];
        }
        var_decl->names.values = names_array;
        var_decl->names.size = static_cast<int>(names.size());

        // Parse optional initializer
        var_decl->equalsToken = nullptr;
        var_decl->initializer = nullptr;

        if (tokens.check(TokenKind::Assign))
        {
            auto *equals_token = alloc.alloc<TokenNode>();
            equals_token->text = tokens.current().text;

            var_decl->equalsToken = equals_token;
            tokens.advance(); // consume '='

            auto init_result = parse_expression();
            if (init_result.is_success())
            {
                var_decl->initializer = init_result.get_node();
            }
        }

        // Note: Semicolon handling is done by the caller
        var_decl->semicolon = nullptr;

        return ParseResult<DeclarationNode>::success(var_decl);
    }

    
    // Main expression parsing entry point
    ParseResult<ExpressionNode> Parser::parse_expression(int min_precedence)
    {
        // Handle assignment expressions first (right-associative, lowest precedence)
        auto left_result = parse_binary_expression(min_precedence);
        if (!left_result.is_success())
        {
            return left_result;
        }

        auto *left = left_result.get_node();

        // Check for assignment operators
        if (tokens.check(TokenKind::Assign))
        {
            const Token &assign_token = tokens.current();
            tokens.advance(); // consume '='

            // Parse right-hand side (right-associative)
            auto right_result = parse_expression(0); // Start from lowest precedence for right-associativity

            if (right_result.is_fatal())
            {
                return create_error<ExpressionNode>("Expected expression after '='");
            }

            auto *assign_expr = alloc.alloc<AssignmentExpressionNode>();
            assign_expr->target = left;
            assign_expr->source = right_result.get_node();
            assign_expr->opKind = assign_token.to_assignment_operator_kind();


            return ParseResult<ExpressionNode>::success(assign_expr);
        }

        return left_result;
    }

    // Pratt parser implementation for binary expressions
    ParseResult<ExpressionNode> Parser::parse_binary_expression(int min_precedence)
    {
        auto left_result = parse_primary();
        if (!left_result.is_success())
        {
            return left_result;
        }

        auto *left = left_result.get_node();

        while (!tokens.at_end())
        {
            const Token &op_token = tokens.current();
            TokenKind op_kind = op_token.kind;
            
            // Special handling for range operators
            if (op_kind == TokenKind::DotDot || op_kind == TokenKind::DotDotEquals)
            {
                return parse_range_expression(left);
            }
            
            int precedence = get_precedence(op_kind);

            if (precedence == 0 || precedence < min_precedence)
                break;

            tokens.advance(); // consume operator

            auto right_result = parse_binary_expression(precedence + 1);
            if (!right_result.is_success())
            {
                // Error recovery: create binary expr with error on right side
                auto error = create_error<AstNode>("Expected right operand");
                auto *binary = alloc.alloc<BinaryExpressionNode>();
                binary->left = left;
                binary->right = error.get_ast_node();
                binary->opKind = op_token.to_binary_operator_kind();

                return ParseResult<ExpressionNode>::success(binary);
            }

            auto *binary = alloc.alloc<BinaryExpressionNode>();
            binary->left = left;
            binary->right = right_result.get_node();
            binary->opKind = op_token.to_binary_operator_kind();


            left = binary;
        }

        return ParseResult<ExpressionNode>::success(left);
    }

    // Primary expression parsing - handles literals, identifiers, parentheses, unary expressions
    ParseResult<ExpressionNode> Parser::parse_primary()
    {
        

        // Handle prefix range expressions (..end)
        if (tokens.check(TokenKind::DotDot) || tokens.check(TokenKind::DotDotEquals))
        {
            return parse_prefix_range_expression();
        }

        // Handle unary expressions first
        if (tokens.check(TokenKind::Not) || tokens.check(TokenKind::Minus) ||
            tokens.check(TokenKind::Increment) || tokens.check(TokenKind::Decrement))
        {
            return parse_unary_expression();
        }

        if (tokens.check(TokenKind::LiteralI32))
        {
            return parse_integer_literal();
        }

        if (tokens.check(TokenKind::LiteralF32))
        {
            return parse_float_literal();
        }

        if (tokens.check(TokenKind::LiteralF64))
        {
            return parse_double_literal();
        }

        if (tokens.check(TokenKind::LiteralString))
        {
            return parse_string_literal();
        }

        if (tokens.check(TokenKind::LiteralBool))
        {
            return parse_boolean_literal();
        }

        if (tokens.check(TokenKind::Identifier))
        {
            return parse_identifier_or_call();
        }

        if (tokens.check(TokenKind::LeftParen))
        {
            return parse_parenthesized_expression();
        }

        if (tokens.check(TokenKind::New))
        {
            return parse_new_expression();
        }

        return ParseResult<ExpressionNode>::none();
    }

    // Range expression parsing
    ParseResult<ExpressionNode> Parser::parse_range_expression(ExpressionNode* left)
    {
        
        
        // Check for .. or ..= operators
        if (!tokens.check(TokenKind::DotDot) && !tokens.check(TokenKind::DotDotEquals))
        {
            return ParseResult<ExpressionNode>::success(left);
        }
        
        auto *range_expr = alloc.alloc<RangeExpressionNode>();

        
        // Store range operator
        auto *range_op = alloc.alloc<TokenNode>();
        range_op->text = tokens.current().text;
        range_op->location = tokens.current().location;

        range_expr->rangeOp = range_op;
        
        tokens.advance(); // consume .. or ..=
        
        // Set start (could be null for ..end syntax)
        range_expr->start = left;
        
        // Parse end expression (could be null for start.. syntax)
        range_expr->end = nullptr;
        if (!tokens.check(TokenKind::By) && !tokens.at_end() && 
            !tokens.check(TokenKind::RightParen) && !tokens.check(TokenKind::RightBracket) &&
            !tokens.check(TokenKind::Comma) && !tokens.check(TokenKind::Semicolon))
        {
            // Parse the end expression with appropriate precedence
            // Range has precedence 5, so we parse at precedence 6 (additive)
            auto end_result = parse_binary_expression(6);
            if (end_result.is_success())
            {
                range_expr->end = end_result.get_node();
            }
        }
        
        // Check for optional 'by' clause
        range_expr->byKeyword = nullptr;
        range_expr->stepExpression = nullptr;
        if (tokens.check(TokenKind::By))
        {
            auto *by_token = alloc.alloc<TokenNode>();
            by_token->text = tokens.current().text;
            by_token->location = tokens.current().location;

            range_expr->byKeyword = by_token;
            
            tokens.advance(); // consume 'by'
            
            // Parse step expression at additive precedence
            auto step_result = parse_binary_expression(6);
            if (!step_result.is_success())
            {
                return create_error<ExpressionNode>("Expected step expression after 'by'");
            }
            range_expr->stepExpression = step_result.get_node();
        }
        
        return ParseResult<ExpressionNode>::success(range_expr);
    }

    // For prefix range syntax (..end)
    ParseResult<ExpressionNode> Parser::parse_prefix_range_expression()
    {
        
        
        if (!tokens.check(TokenKind::DotDot) && !tokens.check(TokenKind::DotDotEquals))
        {
            return create_error<ExpressionNode>("Expected '..' or '..=' for prefix range expression");
        }
        
        auto *range_expr = alloc.alloc<RangeExpressionNode>();

        
        // Store range operator
        auto *range_op = alloc.alloc<TokenNode>();
        range_op->text = tokens.current().text;
        range_op->location = tokens.current().location;

        range_expr->rangeOp = range_op;
        
        tokens.advance(); // consume .. or ..=
        
        // No start for prefix range
        range_expr->start = nullptr;
        
        // Parse end expression at additive precedence (6)
        auto end_result = parse_binary_expression(6);
        if (!end_result.is_success())
        {
            return create_error<ExpressionNode>("Expected end expression after range operator");
        }
        range_expr->end = end_result.get_node();
        
        // Prefix ranges don't support 'by' clause
        range_expr->byKeyword = nullptr;
        range_expr->stepExpression = nullptr;
        

        
        return ParseResult<ExpressionNode>::success(range_expr);
    }

    // Literal parsing methods
    ParseResult<ExpressionNode> Parser::parse_integer_literal()
    {
        const Token &token = tokens.current();
        tokens.advance();

        auto *literal = alloc.alloc<LiteralExpressionNode>();
        literal->kind = LiteralKind::I32;


        auto *token_node = alloc.alloc<TokenNode>();
        token_node->text = token.text;

        literal->token = token_node;

        return ParseResult<ExpressionNode>::success(literal);
    }

    ParseResult<ExpressionNode> Parser::parse_string_literal()
    {
        const Token &token = tokens.current();
        tokens.advance();

        auto *literal = alloc.alloc<LiteralExpressionNode>();
        literal->kind = LiteralKind::String;


        auto *token_node = alloc.alloc<TokenNode>();
        token_node->text = token.text;

        literal->token = token_node;

        return ParseResult<ExpressionNode>::success(literal);
    }

    ParseResult<ExpressionNode> Parser::parse_boolean_literal()
    {
        const Token &token = tokens.current();
        tokens.advance();

        auto *literal = alloc.alloc<LiteralExpressionNode>();
        literal->kind = LiteralKind::Bool;


        auto *token_node = alloc.alloc<TokenNode>();
        token_node->text = token.text;

        literal->token = token_node;

        return ParseResult<ExpressionNode>::success(literal);
    }

    ParseResult<ExpressionNode> Parser::parse_float_literal()
    {
        const Token &token = tokens.current();
        tokens.advance();

        auto *literal = alloc.alloc<LiteralExpressionNode>();
        literal->kind = LiteralKind::F32;


        auto *token_node = alloc.alloc<TokenNode>();
        token_node->text = token.text;

        literal->token = token_node;

        return ParseResult<ExpressionNode>::success(literal);
    }

    ParseResult<ExpressionNode> Parser::parse_double_literal()
    {
        const Token &token = tokens.current();
        tokens.advance();

        auto *literal = alloc.alloc<LiteralExpressionNode>();
        literal->kind = LiteralKind::F64;


        auto *token_node = alloc.alloc<TokenNode>();
        token_node->text = token.text;

        literal->token = token_node;

        return ParseResult<ExpressionNode>::success(literal);
    }

    ParseResult<ExpressionNode> Parser::parse_unary_expression()
    {
        const Token &op_token = tokens.current();
        tokens.advance(); // consume unary operator

        auto operand_result = parse_primary();
        if (operand_result.is_fatal())
        {
            return create_error<ExpressionNode>("Expected operand after unary operator");
        }

        auto *unary_expr = alloc.alloc<UnaryExpressionNode>();
        unary_expr->operand = operand_result.get_node();
        unary_expr->opKind = op_token.to_unary_operator_kind();
        unary_expr->isPostfix = false;


        return ParseResult<ExpressionNode>::success(unary_expr);
    }

    ParseResult<ExpressionNode> Parser::parse_identifier_or_call()
    {
        const Token &token = tokens.current();
        tokens.advance();

        auto *identifier_expr = alloc.alloc<IdentifierExpressionNode>();

        auto *identifier = alloc.alloc<IdentifierNode>();
        identifier->name = token.text;

        identifier_expr->identifier = identifier;

        // Start with the identifier, then handle postfix operations
        ExpressionNode *current = identifier_expr;

        // Handle postfix operations: calls, member access, array indexing, increment/decrement
        while (true)
        {
            if (tokens.check(TokenKind::LeftParen))
            {
                // Function call: expr()
                auto call_result = parse_call_suffix(current);
                if (!call_result.is_success())
                    break;
                current = call_result.get_node();
            }
            else if (tokens.check(TokenKind::Dot))
            {
                // Member access: expr.member
                auto member_result = parse_member_access_suffix(current);
                if (!member_result.is_success())
                    break;
                current = member_result.get_node();
            }
            else if (tokens.check(TokenKind::LeftBracket))
            {
                // Array indexing: expr[index]
                auto index_result = parse_indexer_suffix(current);
                if (!index_result.is_success())
                    break;
                current = index_result.get_node();
            }
            else if (tokens.check(TokenKind::Increment) || tokens.check(TokenKind::Decrement))
            {
                // Postfix increment/decrement: expr++ or expr--
                const Token &op_token = tokens.current();
                tokens.advance(); // consume ++ or --

                auto *unary_expr = alloc.alloc<UnaryExpressionNode>();
                unary_expr->operand = current;
                unary_expr->opKind = op_token.to_unary_operator_kind();
                unary_expr->isPostfix = true;


                current = unary_expr;
            }
            else
            {
                break; // No more postfix operations
            }
        }

        return ParseResult<ExpressionNode>::success(current);
    }

    ParseResult<ExpressionNode> Parser::parse_parenthesized_expression()
    {
        tokens.advance(); // consume '('
        auto expr_result = parse_expression();
        if (!tokens.match(TokenKind::RightParen))
        {
            create_error<ExpressionNode>("Expected ')' after expression");
        }
        return expr_result;
    }

    // Operator precedence table for Myre language (following design document)
    int Parser::get_precedence(TokenKind op)
    {
        switch (op)
        {
        case TokenKind::Or:
            return 1; // ||
        case TokenKind::And:
            return 2; // &&
        case TokenKind::Equal:
        case TokenKind::NotEqual:
            return 3; // ==, !=
        case TokenKind::Less:
        case TokenKind::LessEqual:
        case TokenKind::Greater:
        case TokenKind::GreaterEqual:
            return 4; // <, <=, >, >=
        case TokenKind::DotDot:
        case TokenKind::DotDotEquals:
            return 5; // .. (range) - handled specially in parse_binary_expression
        case TokenKind::Plus:
        case TokenKind::Minus:
            return 6; // +, -
        case TokenKind::Asterisk:
        case TokenKind::Slash:
        case TokenKind::Percent:
            return 7; // *, /, %
        case TokenKind::Dot:
            return 9; // . (member access)
        case TokenKind::LeftBracket:
            return 10; // [] (indexing)
        case TokenKind::LeftParen:
            return 10; // () (function call)
        default:
            return 0; // Not a binary operator
        }
    }

    // Postfix expression parsing helpers
    ParseResult<ExpressionNode> Parser::parse_call_suffix(ExpressionNode *target)
    {
        tokens.advance(); // consume '('

        auto *call_expr = alloc.alloc<CallExpressionNode>();
        call_expr->target = target;


        std::vector<AstNode *> arguments;

        // Parse argument list
        while (!tokens.check(TokenKind::RightParen) && !tokens.at_end())
        {
            auto arg_result = parse_expression();

            if (arg_result.is_fatal())
            {

                arguments.push_back(create_error<AstNode>("Invalid argument in call").get_ast_node());


                // Try to recover to next argument or end of call
                recover_to_safe_point();
                break;
            }

            arguments.push_back(arg_result.get_ast_node());

            // Check for comma separator
            if (tokens.check(TokenKind::Comma))
            {
                tokens.advance();
            }
            else if (!tokens.check(TokenKind::RightParen))
            {
                create_error<CallExpressionNode>("Expected ',' or ')' in argument list");

                break;
            }
        }

        if (!tokens.match(TokenKind::RightParen))
        {
            create_error<CallExpressionNode>("Expected ')' to close function call");
        }

        // Allocate argument array
        if (!arguments.empty())
        {
            auto *arg_array = alloc.alloc_array<AstNode *>(arguments.size());
            for (size_t i = 0; i < arguments.size(); ++i)
            {
                arg_array[i] = arguments[i];
            }
            call_expr->arguments.values = arg_array;
            call_expr->arguments.size = static_cast<int>(arguments.size());
        }

        return ParseResult<ExpressionNode>::success(call_expr);
    }

    ParseResult<ExpressionNode> Parser::parse_member_access_suffix(ExpressionNode *target)
    {
        tokens.advance(); // consume '.'

        if (!tokens.check(TokenKind::Identifier))
        {
            return create_error<ExpressionNode>("Expected identifier after '.'");
        }

        const Token &member_token = tokens.current();
        tokens.advance();

        auto *member_expr = alloc.alloc<MemberAccessExpressionNode>();
        member_expr->target = target;

        auto *member_identifier = alloc.alloc<IdentifierNode>();
        member_identifier->name = member_token.text;

        member_expr->member = member_identifier;

        return ParseResult<ExpressionNode>::success(member_expr);
    }

    ParseResult<ExpressionNode> Parser::parse_indexer_suffix(ExpressionNode *target)
    {
        if (!tokens.check(TokenKind::LeftBracket))
        {
            return ParseResult<ExpressionNode>::none();
        }

        tokens.advance(); // consume '['

        auto index_result = parse_expression();
        if (index_result.is_fatal())
        {
            return create_error<ExpressionNode>("Expected index expression");
        }

        if (!tokens.match(TokenKind::RightBracket))
        {
            create_error<IndexerExpressionNode>("Expected ']' to close index expression");
        }

        auto *indexer_expr = alloc.alloc<IndexerExpressionNode>();
        indexer_expr->target = target;
        indexer_expr->index = index_result.get_node();


        return ParseResult<ExpressionNode>::success(indexer_expr);
    }

    // Parse new expressions: new TypeName() or new TypeName
    ParseResult<ExpressionNode> Parser::parse_new_expression()
    {
        if (!tokens.check(TokenKind::New))
        {
            return ParseResult<ExpressionNode>::none();
        }
        
        const Token &new_token = tokens.current();
        tokens.advance(); // consume 'new'

        // Parse the type name
        auto type_result = parse_type_expression();
        if (!type_result.is_success())
        {
            return create_error<ExpressionNode>("Expected type name after 'new'");
        }

        auto *new_expr = alloc.alloc<NewExpressionNode>();


        // Create token node for the 'new' keyword
        auto *new_keyword = alloc.alloc<TokenNode>();
        new_keyword->text = new_token.text;

        new_expr->newKeyword = new_keyword;

        new_expr->type = type_result.get_node();

        // Check for optional constructor call: ()
        if (tokens.check(TokenKind::LeftParen))
        {
            // Parse constructor call as a call expression with the type as target
            // For now, we'll create a simple call expression
            auto call_result = parse_call_suffix(nullptr); // We'll fix the target afterwards
            if (call_result.is_success())
            {
                auto *call_expr = static_cast<CallExpressionNode *>(call_result.get_node());
                call_expr->target = nullptr; // Constructor calls don't have a target expression
                new_expr->constructorCall = call_expr;
            }
            else
            {
                // Failed to parse constructor call - treat as error

                new_expr->constructorCall = nullptr;
            }
        }
        else
        {
            // No constructor call
            new_expr->constructorCall = nullptr;
        }

        return ParseResult<ExpressionNode>::success(new_expr);
    }

    ParseResult<ExpressionNode> Parser::parse_match_expression()
    {
        return create_error<ExpressionNode>("Match expressions not implemented yet");
    }

    ParseResult<ExpressionNode> Parser::parse_enum_variant()
    {
        return create_error<ExpressionNode>("Enum variants not implemented yet");
    }

} // namespace Myre