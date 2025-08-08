#include "parser/declaration_parser.hpp"
#include "parser/parser.hpp"
#include "parser/statement_parser.hpp"
#include "parser/expression_parser.hpp"
#include <vector>
#include <iostream>

namespace Myre
{

    DeclarationParser::DeclarationParser(Parser *parser) : ParserBase(parser)
    {
    }

    bool DeclarationParser::check_declaration()
    {
        // check forward until the next semicolon and see if we come across a declaration keyword
        auto &ctx = context();
        if (ctx.at_end())
            return false;

        // Check for declaration keywords
        int offset = 0;
        while (true)
        {
            auto token = ctx.peek(offset);
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
    ParseResult<DeclarationNode> DeclarationParser::parse_declaration()
    {
        auto &ctx = context();

        // Parse access modifiers and other modifiers first
        std::vector<ModifierKind> modifiers = parse_all_modifiers();

        // Determine declaration type by looking at the current token
        // Note: Using directives are handled at top-level, not here

        if (ctx.check(TokenKind::Namespace))
        {
            return parse_namespace_declaration();
        }

        if (ctx.check(TokenKind::Fn))
        {
            return parse_function_declaration();
        }

        if (ctx.check(TokenKind::New))
        {
            // Lookahead to confirm it's a constructor (has left paren)
            if (ctx.peek(1).kind == TokenKind::LeftParen)
            {
                return parse_constructor_declaration();
            }
        }

        if (ctx.check(TokenKind::Type))
        {
            return parse_type_declaration();
        }

        if (ctx.check(TokenKind::Enum))
        {
            return parse_enum_declaration();
        }

        if (ctx.check(TokenKind::Var) || (ctx.check(TokenKind::Identifier) && ctx.peek().kind == TokenKind::Identifier))
        {
            return parse_variable_declaration();
        }

        return ParseResult<DeclarationNode>::none();
    }

    // Function declaration parsing
    ParseResult<DeclarationNode> DeclarationParser::parse_function_declaration()
    {
        auto &ctx = context();

        ctx.advance(); // consume 'fn'

        if (!ctx.check(TokenKind::Identifier))
        {
            return ParseResult<DeclarationNode>::error(
                create_error("Expected function name"));
        }

        const Token &name_token = ctx.current();
        ctx.advance();

        auto *func_decl = parser_->get_allocator().alloc<FunctionDeclarationNode>();

        // Set up name
        auto *name_node = parser_->get_allocator().alloc<IdentifierNode>();
        name_node->name = name_token.text;
        func_decl->name = name_node;

        // Parse parameter list
        if (!parser_->match(TokenKind::LeftParen))
        {
            return ParseResult<DeclarationNode>::error(
                create_error("Expected '(' after function name"));
        }

        // Parse parameter list and store the parameters
        std::vector<ParameterNode *> params;

        // Parse parameter list: param1: Type1, param2: Type2, ...
        while (!ctx.check(TokenKind::RightParen) && !ctx.at_end())
        {
            auto param_result = parse_parameter();
            if (param_result.is_success())
            {
                params.push_back(param_result.get_node());
            }
            else
            {
                // Error recovery: skip to next parameter or end
                parser_->recover_to_safe_point(ctx);
                if (ctx.check(TokenKind::RightParen))
                    break;
            }

            // Check for comma separator
            if (ctx.check(TokenKind::Comma))
            {
                ctx.advance();
            }
            else if (!ctx.check(TokenKind::RightParen))
            {
                create_error("Expected ',' or ')' in parameter list");
                break;
            }
        }

        // Store parameters in the function declaration
        if (!params.empty())
        {
            auto *param_array = parser_->get_allocator().alloc_array<AstNode *>(params.size());
            for (size_t i = 0; i < params.size(); ++i)
            {
                param_array[i] = params[i];
            }
            func_decl->parameters.values = param_array;
            func_decl->parameters.size = static_cast<int>(params.size());
        }

        if (!parser_->match(TokenKind::RightParen))
        {
            create_error("Expected ')' after parameters");
        }

        // Parse optional return type: fn test(): i32
        if (parser_->match(TokenKind::Colon))
        {
            auto return_type_result = parser_->parse_type_expression();
            if (return_type_result.is_success())
            {
                func_decl->returnType = return_type_result.get_node();
            }
        }

        // Parse function body
        if (ctx.check(TokenKind::LeftBrace))
        {
            // Enter function context for parsing body
            auto function_guard = ctx.save_context();
            ctx.set_function_context(true);

            auto body_result = parser_->get_statement_parser().parse_block_statement();
            if (body_result.is_success())
            {
                func_decl->body = static_cast<BlockStatementNode *>(body_result.get_node());
            }
        }
        else
        {
            // Missing body - create error but continue
            create_error("Expected '{' for function body");
        }

        return ParseResult<DeclarationNode>::success(func_decl);
    }

    ParseResult<DeclarationNode> DeclarationParser::parse_constructor_declaration()
    {
        auto &ctx = context();

        // Store the 'new' keyword
        auto *new_keyword = parser_->get_allocator().alloc<TokenNode>();
        new_keyword->text = ctx.current().text;
        ctx.advance(); // consume 'new'

        auto *ctor_decl = parser_->get_allocator().alloc<ConstructorDeclarationNode>();
        ctor_decl->newKeyword = new_keyword;

        // Parse opening paren
        if (!ctx.check(TokenKind::LeftParen))
        {
            return ParseResult<DeclarationNode>::error(
                create_error("Expected '(' after 'new' in constructor"));
        }

        auto *open_paren = parser_->get_allocator().alloc<TokenNode>();
        open_paren->text = ctx.current().text;
        ctor_decl->openParen = open_paren;
        ctx.advance(); // consume '('

        // Parse parameters (reuse existing parameter parsing logic)
        std::vector<ParameterNode *> params;
        while (!ctx.check(TokenKind::RightParen) && !ctx.at_end())
        {
            auto param_result = parse_parameter();
            if (param_result.is_success())
            {
                params.push_back(param_result.get_node());
            }
            else
            {
                parser_->recover_to_safe_point(ctx);
                if (ctx.check(TokenKind::RightParen))
                    break;
            }

            if (ctx.check(TokenKind::Comma))
            {
                ctx.advance();
            }
            else if (!ctx.check(TokenKind::RightParen))
            {
                create_error("Expected ',' or ')' in parameter list");
                break;
            }
        }

        // Store parameters
        if (!params.empty())
        {
            auto *param_array = parser_->get_allocator().alloc_array<ParameterNode *>(params.size());
            for (size_t i = 0; i < params.size(); ++i)
            {
                param_array[i] = params[i];
            }
            ctor_decl->parameters.values = param_array;
            ctor_decl->parameters.size = static_cast<int>(params.size());
        }

        // Parse closing paren
        if (!ctx.check(TokenKind::RightParen))
        {
            create_error("Expected ')' after constructor parameters");
            ctor_decl->closeParen = nullptr;
        }
        else
        {
            auto *close_paren = parser_->get_allocator().alloc<TokenNode>();
            close_paren->text = ctx.current().text;
            ctor_decl->closeParen = close_paren;
            ctx.advance(); // consume ')'
        }

        // Parse constructor body
        if (!ctx.check(TokenKind::LeftBrace))
        {
            return ParseResult<DeclarationNode>::error(
                create_error("Expected '{' for constructor body"));
        }

        // Parse body (can reuse block statement parsing)
        auto body_result = parser_->get_statement_parser().parse_block_statement();
        if (body_result.is_success())
        {
            ctor_decl->body = static_cast<BlockStatementNode *>(body_result.get_node());
        }

        return ParseResult<DeclarationNode>::success(ctor_decl);
    }

    // Type declaration parsing
    ParseResult<DeclarationNode> DeclarationParser::parse_type_declaration()
    {
        auto &ctx = context();

        // Store the type keyword token
        auto *type_keyword = parser_->get_allocator().alloc<TokenNode>();
        type_keyword->text = ctx.current().text;

        ctx.advance(); // consume 'type'

        if (!ctx.check(TokenKind::Identifier))
        {
            return ParseResult<DeclarationNode>::error(
                create_error("Expected type name"));
        }

        const Token &name_token = ctx.current();
        ctx.advance();

        auto *type_decl = parser_->get_allocator().alloc<TypeDeclarationNode>();
        type_decl->typeKeyword = type_keyword;

        // Set up name
        auto *name_node = parser_->get_allocator().alloc<IdentifierNode>();
        name_node->name = name_token.text;
        type_decl->name = name_node;

        // Parse type body
        if (!parser_->match(TokenKind::LeftBrace))
        {
            return ParseResult<DeclarationNode>::error(
                create_error("Expected '{' for type body"));
        }

        // Store the opening brace
        auto *open_brace = parser_->get_allocator().alloc<TokenNode>();
        open_brace->text = "{";
        type_decl->openBrace = open_brace;

        std::vector<AstNode *> members;

        while (!ctx.check(TokenKind::RightBrace) && !ctx.at_end())
        {
            auto member_result = parse_declaration();

            // consume semicolon if present
            if (ctx.check(TokenKind::Semicolon))
            {
                ctx.advance();
            }

            if (member_result.is_success())
            {
                members.push_back(member_result.get_node());
            }
            else if (member_result.is_error())
            {
                // Add error node and continue
                members.push_back(member_result.get_error());


                // Simple recovery: skip to next member or end
                parser_->recover_to_safe_point(ctx);
            }
            else
            {
                // Fatal error
                break;
            }
        }

        if (!parser_->match(TokenKind::RightBrace))
        {
            create_error("Expected '}' to close type");

            type_decl->closeBrace = nullptr;
        }
        else
        {
            // Store the closing brace
            auto *close_brace = parser_->get_allocator().alloc<TokenNode>();
            close_brace->text = "}";

            type_decl->closeBrace = close_brace;
        }

        // Allocate member array
        if (!members.empty())
        {
            auto *member_array = parser_->get_allocator().alloc_array<AstNode *>(members.size());
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
    ParseResult<DeclarationNode> DeclarationParser::parse_enum_declaration()
    {
        auto &ctx = context();

        ctx.advance(); // consume 'enum'

        if (!ctx.check(TokenKind::Identifier))
        {
            return ParseResult<DeclarationNode>::error(
                create_error("Expected enum name"));
        }

        const Token &name_token = ctx.current();
        ctx.advance();

        auto *enum_decl = parser_->get_allocator().alloc<EnumDeclarationNode>();


        // Set up name
        auto *name_node = parser_->get_allocator().alloc<IdentifierNode>();
        name_node->name = name_token.text;

        enum_decl->name = name_node;

        if (!parser_->match(TokenKind::LeftBrace))
        {
            return ParseResult<DeclarationNode>::error(
                create_error("Expected '{' for enum body"));
        }

        std::vector<EnumCaseNode *> cases;

        while (!ctx.check(TokenKind::RightBrace) && !ctx.at_end())
        {
            auto case_result = parse_enum_case();

            if (case_result.is_success())
            {
                cases.push_back(case_result.get_node());
            }
            else
            {

                // Simple recovery: skip to next case or end
                parser_->recover_to_safe_point(ctx);
                if (ctx.check(TokenKind::RightBrace))
                    break;
            }

            // Optional comma between cases
            if (ctx.check(TokenKind::Comma))
            {
                ctx.advance();
            }
        }

        if (!parser_->match(TokenKind::RightBrace))
        {
            create_error("Expected '}' to close enum");

        }

        // Allocate cases array
        if (!cases.empty())
        {
            auto *case_array = parser_->get_allocator().alloc_array<EnumCaseNode *>(cases.size());
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
    ParseResult<StatementNode> DeclarationParser::parse_using_directive()
    {
        auto &ctx = context();

        ctx.advance(); // consume 'using'

        auto type_result = parser_->parse_qualified_name();
        if (!type_result.is_success())
        {
            return ParseResult<StatementNode>::error(
                create_error("Expected qualified name after 'using'"));
        }

        auto *using_stmt = parser_->get_allocator().alloc<UsingDirectiveNode>();
        using_stmt->namespaceName = type_result.get_node();


        parser_->expect(TokenKind::Semicolon, "Expected ';' after using directive");

        return ParseResult<StatementNode>::success(using_stmt);
    }

    // Helper method implementations
    ParseResult<AstNode> DeclarationParser::parse_parameter_list()
    {
        std::vector<ParameterNode *> params;
        auto &ctx = context();

        // Parse parameter list: param1: Type1, param2: Type2, ...
        while (!ctx.check(TokenKind::RightParen) && !ctx.at_end())
        {
            auto param_result = parse_parameter();
            if (param_result.is_success())
            {
                params.push_back(param_result.get_node());
            }
            else
            {
                // Error recovery: skip to next parameter or end
                parser_->recover_to_safe_point(ctx);
                if (ctx.check(TokenKind::RightParen))
                    break;
            }

            // Check for comma separator
            if (ctx.check(TokenKind::Comma))
            {
                ctx.advance();
            }
            else if (!ctx.check(TokenKind::RightParen))
            {
                create_error("Expected ',' or ')' in parameter list");
                break;
            }
        }

        // We'll return a wrapper node that contains the parameter list
        // For now, just return success to indicate we processed parameters
        return ParseResult<AstNode>::success(nullptr);
    }

    ParseResult<EnumCaseNode> DeclarationParser::parse_enum_case()
    {
        auto &ctx = context();

        if (!ctx.check(TokenKind::Identifier))
        {
            return ParseResult<EnumCaseNode>::error(
                create_error("Expected case name"));
        }

        const Token &name_token = ctx.current();
        ctx.advance();

        auto *case_node = parser_->get_allocator().alloc<EnumCaseNode>();


        // Set up name
        auto *name_node = parser_->get_allocator().alloc<IdentifierNode>();
        name_node->name = name_token.text;

        case_node->name = name_node;

        // Check for associated data: Case(param1: Type1, param2: Type2)
        if (ctx.check(TokenKind::LeftParen))
        {
            ctx.advance(); // consume '('

            std::vector<ParameterNode *> params;

            while (!ctx.check(TokenKind::RightParen) && !ctx.at_end())
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

                if (ctx.check(TokenKind::Comma))
                {
                    ctx.advance();
                }
                else if (!ctx.check(TokenKind::RightParen))
                {
                    create_error("Expected ',' or ')' in parameter list");

                    break;
                }
            }

            if (!parser_->match(TokenKind::RightParen))
            {
                create_error("Expected ')' after parameters");

            }

            // Allocate parameter array
            if (!params.empty())
            {
                auto *param_array = parser_->get_allocator().alloc_array<ParameterNode *>(params.size());
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

    ParseResult<ParameterNode> DeclarationParser::parse_parameter()
    {
        auto &ctx = context();

        // Myre syntax: Type name (not name: Type)
        // First parse the type
        auto type_result = parser_->parse_type_expression();
        if (!type_result.is_success())
        {
            return ParseResult<ParameterNode>::error(
                create_error("Expected parameter type"));
        }

        // Then parse the parameter name
        if (!ctx.check(TokenKind::Identifier))
        {
            return ParseResult<ParameterNode>::error(
                create_error("Expected parameter name after type"));
        }

        const Token &name_token = ctx.current();
        ctx.advance();

        auto *param = parser_->get_allocator().alloc<ParameterNode>();


        // Set up name
        auto *name_node = parser_->get_allocator().alloc<IdentifierNode>();
        name_node->name = name_token.text;

        param->name = name_node;

        // Set up type
        param->type = type_result.get_node();

        // Check for default value: = value
        if (parser_->match(TokenKind::Assign))
        {
            auto default_result = parser_->get_expression_parser().parse_expression();
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

    ParseResult<ParameterNode> DeclarationParser::parse_enum_parameter()
    {
        auto &ctx = context();

        // Parse type first
        auto type_result = parser_->parse_type_expression();
        if (!type_result.is_success())
        {
            return ParseResult<ParameterNode>::error(
                create_error("Expected parameter type"));
        }

        auto *param = parser_->get_allocator().alloc<ParameterNode>();

        param->type = type_result.get_node();
        param->defaultValue = nullptr; // No default values for enum parameters

        // Check if there's a parameter name after the type
        if (ctx.check(TokenKind::Identifier))
        {
            // This is the "Type name" syntax: i32 priority, string message
            const Token &name_token = ctx.current();
            ctx.advance();

            auto *name_node = parser_->get_allocator().alloc<IdentifierNode>();
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
    std::vector<ModifierKind> DeclarationParser::parse_all_modifiers()
    {
        std::vector<ModifierKind> modifiers;
        auto &ctx = context();

        while (true)
        {
            auto token = ctx.current();
            if (token.is_modifier())
            {
                ctx.advance(); // consume modifier
                modifiers.push_back(token.to_modifier_kind());
            }
            else
            {
                break;
            }
        }

        return modifiers;
    }

    ParseResult<DeclarationNode> DeclarationParser::parse_namespace_declaration()
    {
        auto &ctx = context();

        ctx.advance(); // consume 'namespace'

        if (!ctx.check(TokenKind::Identifier))
        {
            return ParseResult<DeclarationNode>::error(
                create_error("Expected namespace name"));
        }

        // Set up name
        auto name_node = parser_->parse_qualified_name();
        if (name_node.is_error())
        {
            return ParseResult<DeclarationNode>::error(create_error("Failed to parse namespace name"));
        }

        auto *namespace_decl = parser_->get_allocator().alloc<NamespaceDeclarationNode>();
        namespace_decl->name = name_node.get_node();

        // Parse namespace body
        if (!ctx.check(TokenKind::LeftBrace))
        {
            // look for a semicolon instead because it may be a file-level namespace
            if (parser_->match(TokenKind::Semicolon))
            {
                // File-level namespace, no body
                return ParseResult<DeclarationNode>::success(namespace_decl);
            }
            else
            {
                return ParseResult<DeclarationNode>::error(
                    create_error("Expected '{' for namespace body or ';' for file-scoped namespace"));
            }
        }

        // use statement parser for block
        auto body_result = parser_->get_statement_parser().parse_block_statement();
        if (body_result.is_success())
        {
            namespace_decl->body = static_cast<BlockStatementNode *>(body_result.get_node());
        }
        else
        {

        }

        return ParseResult<DeclarationNode>::success(namespace_decl);
    }

    ParseResult<AstNode> DeclarationParser::parse_generic_parameters()
    {
        return ParseResult<AstNode>::error(
            create_error("Generic parameters not implemented yet"));
    }

    ParseResult<AstNode> DeclarationParser::parse_generic_constraints()
    {
        return ParseResult<AstNode>::error(
            create_error("Generic constraints not implemented yet"));
    }

    // Helper method for parsing variable declarations in for-in context
    ParseResult<StatementNode> DeclarationParser::parse_for_variable_declaration()
    {
        auto &ctx = context();

        // Check for 'var' keyword
        if (ctx.check(TokenKind::Var))
        {
            // Store var keyword
            auto *var_keyword = parser_->get_allocator().alloc<TokenNode>();
            var_keyword->text = ctx.current().text;
            var_keyword->location = ctx.current().location;


            ctx.advance(); // consume 'var'

            if (!ctx.check(TokenKind::Identifier))
            {
                return ParseResult<StatementNode>::error(
                    create_error("Expected identifier after 'var'"));
            }

            // Create a simplified variable declaration node
            auto *var_decl = parser_->get_allocator().alloc<VariableDeclarationNode>();
            var_decl->varKeyword = var_keyword;
            var_decl->type = nullptr; // var declarations don't have explicit type

            // Store identifier in names array (single element)
            auto *id_node = parser_->get_allocator().alloc<IdentifierNode>();
            id_node->name = ctx.current().text;
            id_node->location = ctx.current().location;


            auto *names_array = parser_->get_allocator().alloc_array<IdentifierNode *>(1);
            names_array[0] = id_node;
            var_decl->names.values = names_array;
            var_decl->names.size = 1;

            ctx.advance(); // consume identifier

            // No initializer in for-in context
            var_decl->initializer = nullptr;
            var_decl->equalsToken = nullptr;
            var_decl->semicolon = nullptr;


            return ParseResult<StatementNode>::success(var_decl);
        }

        // Check for TypeName identifier pattern or just identifier
        if (ctx.check(TokenKind::Identifier))
        {
            // Save position in case we need to backtrack
            size_t saved_pos = ctx.position;

            // Try to parse as type + identifier
            auto type_result = parser_->parse_type_expression();
            if (type_result.is_success() && ctx.check(TokenKind::Identifier))
            {
                // Create typed variable declaration
                auto *var_decl = parser_->get_allocator().alloc<VariableDeclarationNode>();
                var_decl->varKeyword = nullptr;
                var_decl->type = type_result.get_node();

                // Store identifier in names array
                auto *id_node = parser_->get_allocator().alloc<IdentifierNode>();
                id_node->name = ctx.current().text;
                id_node->location = ctx.current().location;


                auto *names_array = parser_->get_allocator().alloc_array<IdentifierNode *>(1);
                names_array[0] = id_node;
                var_decl->names.values = names_array;
                var_decl->names.size = 1;

                ctx.advance(); // consume identifier

                var_decl->initializer = nullptr;
                var_decl->equalsToken = nullptr;
                var_decl->semicolon = nullptr;


                return ParseResult<StatementNode>::success(var_decl);
            }

            // Reset position if type parsing failed or no identifier follows
            ctx.position = saved_pos;

            // Treat as just an identifier reference (pre-declared variable)
            auto *id_node = parser_->get_allocator().alloc<IdentifierNode>();
            id_node->name = ctx.current().text;
            id_node->location = ctx.current().location;

            ctx.advance(); // consume identifier

            // Create an IdentifierExpressionNode for consistency
            auto *id_expr = parser_->get_allocator().alloc<IdentifierExpressionNode>();
            id_expr->identifier = id_node;


            // Wrap in an expression statement to match StatementNode type
            auto *expr_stmt = parser_->get_allocator().alloc<ExpressionStatementNode>();
            expr_stmt->expression = id_expr;


            return ParseResult<StatementNode>::success(expr_stmt);
        }

        return ParseResult<StatementNode>::error(
            create_error("Expected variable declaration in for-in statement"));
    }

    ParseResult<DeclarationNode> DeclarationParser::parse_variable_declaration()
    {
        auto &ctx = context();
        TokenNode *var_keyword = nullptr;
        TypeNameNode *type = nullptr;
        if (ctx.check(TokenKind::Var))
        {
            // Store the var keyword token
            var_keyword = parser_->get_allocator().alloc<TokenNode>();
            var_keyword->text = ctx.current().text;


            ctx.advance(); // consume 'var'
        }
        else if (ctx.check(TokenKind::Identifier) && ctx.peek().kind == TokenKind::Identifier)
        {
            // Typed variable declaration: Type name = value
            auto type_name = parser_->parse_type_expression();
            if (type_name.is_success())
            {
                type = type_name.get_node();
            }
        }
        else
        {
            return ParseResult<DeclarationNode>::error(
                create_error("Unsupported variable declaration syntax"));
        }

        // Parse variable names (can be multiple: var x, y, z = 0)
        std::vector<IdentifierNode *> names;

        if (!ctx.check(TokenKind::Identifier))
        {
            return ParseResult<DeclarationNode>::error(
                create_error("Expected identifier after 'var'"));
        }

        // Parse first name
        const Token &first_name_token = ctx.current();
        ctx.advance();

        auto *first_name = parser_->get_allocator().alloc<IdentifierNode>();
        first_name->name = first_name_token.text;

        names.push_back(first_name);

        // Parse additional names if comma-separated
        while (ctx.check(TokenKind::Comma))
        {
            ctx.advance(); // consume comma

            if (!ctx.check(TokenKind::Identifier))
            {
                return ParseResult<DeclarationNode>::error(
                    create_error("Expected identifier after ','"));
            }

            const Token &name_token = ctx.current();
            ctx.advance();

            auto *name_node = parser_->get_allocator().alloc<IdentifierNode>();
            name_node->name = name_token.text;

            names.push_back(name_node);
        }

        auto *var_decl = parser_->get_allocator().alloc<VariableDeclarationNode>();

        var_decl->varKeyword = var_keyword;
        var_decl->type = type;

        // Allocate and populate names array
        auto *names_array = parser_->get_allocator().alloc_array<IdentifierNode *>(names.size());
        for (size_t i = 0; i < names.size(); ++i)
        {
            names_array[i] = names[i];
        }
        var_decl->names.values = names_array;
        var_decl->names.size = static_cast<int>(names.size());

        // Parse optional initializer
        var_decl->equalsToken = nullptr;
        var_decl->initializer = nullptr;

        if (ctx.check(TokenKind::Assign))
        {
            auto *equals_token = parser_->get_allocator().alloc<TokenNode>();
            equals_token->text = ctx.current().text;

            var_decl->equalsToken = equals_token;
            ctx.advance(); // consume '='

            auto init_result = parser_->get_expression_parser().parse_expression();
            if (init_result.is_success())
            {
                var_decl->initializer = init_result.get_node();
            }
            else
            {

            }
        }

        // Note: Semicolon handling is done by the caller
        var_decl->semicolon = nullptr;

        return ParseResult<DeclarationNode>::success(var_decl);
    }

} // namespace Myre