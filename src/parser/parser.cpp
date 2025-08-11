#include "parser/parser.hpp"
#include <algorithm>
#include <sstream>
#include <type_traits>

namespace Myre
{
    // ================================================================================
    // #region Operator Table
    // ================================================================================

    const std::map<TokenKind, Parser::OpInfo> Parser::operator_table = {
        // Precedence 0: Assignments (right-associative)
        {TokenKind::Assign, {0, true, false, true}},
        {TokenKind::PlusAssign, {0, true, false, true}},
        {TokenKind::MinusAssign, {0, true, false, true}},
        {TokenKind::StarAssign, {0, true, false, true}},
        {TokenKind::SlashAssign, {0, true, false, true}},

        // Precedence 1: Logical OR
        {TokenKind::Or, {1, false}},

        // Precedence 2: Logical AND
        {TokenKind::And, {2, false}},

        // Precedence 3: Equality
        {TokenKind::Equal, {3, false}},
        {TokenKind::NotEqual, {3, false}},

        // Precedence 4: Relational
        {TokenKind::Less, {4, false}},
        {TokenKind::LessEqual, {4, false}},
        {TokenKind::Greater, {4, false}},
        {TokenKind::GreaterEqual, {4, false}},

        // Precedence 5: Range (special handling)
        {TokenKind::DotDot, {5, false, true}},
        {TokenKind::DotDotEquals, {5, false, true}},

        // Precedence 6: Additive
        {TokenKind::Plus, {6, false}},
        {TokenKind::Minus, {6, false}},

        // Precedence 7: Multiplicative
        {TokenKind::Asterisk, {7, false}},
        {TokenKind::Slash, {7, false}},
        {TokenKind::Percent, {7, false}},
    };

    // #endregion

    // ================================================================================
    // #region Constructor and Main Entry
    // ================================================================================

    Parser::Parser(TokenStream &tokens) : tokens(tokens) {}
    Parser::~Parser() = default;

    ParseResult<CompilationUnitNode> Parser::parse()
    {
        return parse_compilation_unit();
    }

    ParseResult<CompilationUnitNode> Parser::parse_compilation_unit()
    {
        auto *unit = alloc.alloc<CompilationUnitNode>();
        std::vector<AstNode *> statements;

        while (!tokens.at_end())
        {
            auto stmt = parse_top_level_construct();
            if (stmt)
            {
                statements.push_back(stmt.get());
            }
            else
            {
                // Create error node and try to recover
                statements.push_back(create_error_node("Invalid top-level construct"));
                synchronize();
            }
        }

        unit->statements = make_sized_array(statements);
        return ParseResult<CompilationUnitNode>::ok(unit);
    }

    // #endregion

    // ================================================================================
    // #region Error Handling
    // ================================================================================

    void Parser::error(const std::string &msg)
    {
        errors.push_back({msg, tokens.location(), ParseError::ERROR});
    }

    void Parser::warning(const std::string &msg)
    {
        errors.push_back({msg, tokens.location(), ParseError::WARNING});
    }

    ErrorNode *Parser::create_error_node(const char *msg)
    {
        error(msg);
        auto *node = alloc.alloc<ErrorNode>();
        node->location = tokens.location();
        return node;
    }

    void Parser::synchronize()
    {
        static int last_sync_pos = 0;

        // Skip to next likely statement/declaration start
        tokens.skip_to_any({TokenKind::Fn, TokenKind::Type, TokenKind::Enum,
                            TokenKind::If, TokenKind::While, TokenKind::For,
                            TokenKind::Return, TokenKind::Var, TokenKind::Using,
                            TokenKind::Namespace, TokenKind::Public, TokenKind::Private,
                            TokenKind::Protected, TokenKind::Static, TokenKind::Abstract});

        if (last_sync_pos == tokens.position())
        {
            tokens.advance();
        }

        last_sync_pos = tokens.position();
    }

    // #endregion

    // ================================================================================
    // #region Utility Helpers
    // ================================================================================

    TokenNode *Parser::create_token_node(const Token &token)
    {
        auto *node = alloc.alloc<TokenNode>();
        node->text = token.text;
        node->location = token.location;
        return node;
    }

    TokenNode *Parser::consume_token(TokenKind kind)
    {
        if (tokens.check(kind))
        {
            auto *node = create_token_node(tokens.current());
            tokens.advance();
            return node;
        }
        return nullptr;
    }

    template <typename T>
    SizedArray<T> Parser::make_sized_array(const std::vector<T> &vec)
    {
        SizedArray<T> array;
        if (!vec.empty())
        {
            auto *data = alloc.alloc_array<T>(vec.size());
            for (size_t i = 0; i < vec.size(); ++i)
            {
                data[i] = vec[i];
            }
            array.values = data;
            array.size = static_cast<int>(vec.size());
        }
        return array;
    }

    const Parser::OpInfo *Parser::get_operator_info(TokenKind kind) const
    {
        auto it = operator_table.find(kind);
        return it != operator_table.end() ? &it->second : nullptr;
    }

    bool Parser::is_expression_terminator() const
    {
        return tokens.check_any({TokenKind::Semicolon, TokenKind::RightParen, TokenKind::RightBracket,
                                 TokenKind::RightBrace, TokenKind::Comma, TokenKind::Colon,
                                 TokenKind::Arrow, TokenKind::EndOfFile});
    }

    bool Parser::is_statement_terminator() const
    {
        return tokens.check_any({TokenKind::Semicolon, TokenKind::RightBrace, TokenKind::EndOfFile});
    }

    // #endregion

    // ================================================================================
    // #region Pattern Helpers
    // ================================================================================

    ParseResult<TypedIdentifier> Parser::try_parse_typed_identifier()
    {
        auto checkpoint = tokens.checkpoint();

        // Try to parse a type
        auto type = try_parse_type();
        if (!type)
        {
            return ParseResult<TypedIdentifier>::fail();
        }

        // Must be followed by identifier
        if (!tokens.check(TokenKind::Identifier))
        {
            tokens.restore(checkpoint);
            return ParseResult<TypedIdentifier>::fail();
        }

        auto name = parse_identifier();
        if (!name)
        {
            tokens.restore(checkpoint);
            return ParseResult<TypedIdentifier>::fail();
        }

        // Create a temporary TypedIdentifier - this is a bit hacky but works for our pattern
        auto *result = alloc.alloc<TypedIdentifier>();
        result->type = type.get();
        result->name = name.get();
        return ParseResult<TypedIdentifier>::ok(reinterpret_cast<TypedIdentifier *>(result));
    }

    ParseResult<IdentifierNode> Parser::parse_var_identifier()
    {
        if (!tokens.consume(TokenKind::Var))
        {
            return ParseResult<IdentifierNode>::fail();
        }

        if (!tokens.check(TokenKind::Identifier))
        {
            error("Expected identifier after 'var'");
            return ParseResult<IdentifierNode>::fail();
        }

        return parse_identifier();
    }

    ParseResult<VariablePattern> Parser::parse_variable_pattern(bool allow_untyped)
    {
        // Try: Type identifier
        if (auto typed = try_parse_typed_identifier())
        {
            auto *pattern = alloc.alloc<VariablePattern>();
            pattern->pattern = VarPattern::TYPED;
            pattern->type = typed->type;
            pattern->name = typed->name;
            return ParseResult<VariablePattern>::ok(reinterpret_cast<VariablePattern *>(pattern));
        }

        // Try: var identifier
        auto checkpoint = tokens.checkpoint();
        if (tokens.check(TokenKind::Var))
        {
            auto *var_token = create_token_node(tokens.current());
            tokens.advance();

            if (auto name = parse_identifier())
            {
                auto *pattern = alloc.alloc<VariablePattern>();
                pattern->pattern = VarPattern::VAR;
                pattern->varKeyword = var_token;
                pattern->name = name.get();
                return ParseResult<VariablePattern>::ok(reinterpret_cast<VariablePattern *>(pattern));
            }
            tokens.restore(checkpoint);
        }

        // Try: just identifier (if allowed)
        if (allow_untyped && tokens.check(TokenKind::Identifier))
        {
            if (auto name = parse_identifier())
            {
                auto *pattern = alloc.alloc<VariablePattern>();
                pattern->pattern = VarPattern::IDENTIFIER;
                pattern->name = name.get();
                return ParseResult<VariablePattern>::ok(reinterpret_cast<VariablePattern *>(pattern));
            }
        }

        return ParseResult<VariablePattern>::fail();
    }

    // #endregion

    // ================================================================================
    // #region Context Management
    // ================================================================================

    template <typename F>
    auto Parser::with_context(Context::Type type, F &&func)
    {
        context_stack.push_back({type, tokens.position()});
        if constexpr (std::is_void_v<std::invoke_result_t<F>>)
        {
            func();
            context_stack.pop_back();
        }
        else
        {
            auto result = func();
            context_stack.pop_back();
            return result;
        }
    }

    bool Parser::in_loop() const
    {
        return std::any_of(context_stack.begin(), context_stack.end(),
                           [](const Context &c)
                           { return c.type == Context::LOOP; });
    }

    bool Parser::in_function() const
    {
        return std::any_of(context_stack.begin(), context_stack.end(),
                           [](const Context &c)
                           { return c.type == Context::FUNCTION; });
    }

    bool Parser::in_property_accessor() const
    {
        return std::any_of(context_stack.begin(), context_stack.end(),
                           [](const Context &c)
                           { return c.type == Context::PROPERTY_GETTER || c.type == Context::PROPERTY_SETTER; });
    }

    // #endregion

    // ================================================================================
    // #region Top Level Parsing
    // ================================================================================

    ParseResult<StatementNode> Parser::parse_top_level_construct()
    {
        // Handle using directives
        if (tokens.check(TokenKind::Using))
        {
            return parse_using_directive();
        }

        // Handle namespace declarations
        if (tokens.check(TokenKind::Namespace))
        {
            return parse_namespace_declaration().as<StatementNode>();
        }

        // Try declaration (with modifiers)
        if (check_declaration())
        {
            auto decl = parse_declaration();
            if (decl)
            {
                return ParseResult<StatementNode>::ok(decl.get());
            }
        }

        // Try statement
        return parse_statement();
    }

    // #endregion

    // ================================================================================
    // #region Statements
    // ================================================================================

    ParseResult<StatementNode> Parser::parse_statement()
    {
        // Control flow statements
        if (tokens.check(TokenKind::If))
            return parse_if_statement();
        if (tokens.check(TokenKind::While))
            return parse_while_statement();
        if (tokens.check(TokenKind::For))
            return parse_for_statement();
        if (tokens.check(TokenKind::Return))
            return parse_return_statement();
        if (tokens.check(TokenKind::Break))
            return parse_break_statement();
        if (tokens.check(TokenKind::Continue))
            return parse_continue_statement();
        if (tokens.check(TokenKind::LeftBrace))
            return parse_block_statement();

        // Try declaration
        if (check_declaration())
        {
            auto decl = parse_declaration();
            if (decl)
            {
                return ParseResult<StatementNode>::ok(decl.get());
            }
        }

        // Expression statement
        return parse_expression_statement();
    }

    ParseResult<StatementNode> Parser::parse_block_statement()
    {
        auto *block = alloc.alloc<BlockStatementNode>();
        block->openBrace = consume_token(TokenKind::LeftBrace);

        if (!block->openBrace)
        {
            return ParseResult<StatementNode>::fail();
        }

        std::vector<AstNode *> statements;

        while (!tokens.check(TokenKind::RightBrace) && !tokens.at_end())
        {
            auto stmt = parse_statement();
            if (stmt)
            {
                statements.push_back(stmt.get());
            }
            else
            {
                statements.push_back(create_error_node("Invalid statement"));
                synchronize();
            }
        }

        block->closeBrace = consume_token(TokenKind::RightBrace);
        if (!block->closeBrace)
        {
            error("Expected '}' to close block");
        }

        block->statements = make_sized_array(statements);
        return ParseResult<StatementNode>::ok(block);
    }

    ParseResult<StatementNode> Parser::parse_expression_statement()
    {
        auto expr = parse_expression();
        if (!expr)
        {
            return ParseResult<StatementNode>::fail();
        }

        auto *stmt = alloc.alloc<ExpressionStatementNode>();
        stmt->expression = expr.get();
        stmt->semicolon = consume_token(TokenKind::Semicolon);

        if (!stmt->semicolon)
        {
            error("Expected ';' after expression");
        }

        return ParseResult<StatementNode>::ok(stmt);
    }

    ParseResult<StatementNode> Parser::parse_if_statement()
    {
        auto *if_stmt = alloc.alloc<IfStatementNode>();

        if_stmt->ifKeyword = consume_token(TokenKind::If);
        if (!if_stmt->ifKeyword)
        {
            return ParseResult<StatementNode>::fail();
        }

        if_stmt->openParen = consume_token(TokenKind::LeftParen);
        if (!if_stmt->openParen)
        {
            error("Expected '(' after 'if'");
        }

        auto condition = parse_expression();
        if_stmt->condition = condition.get_or_error("Expected condition expression", this);

        if_stmt->closeParen = consume_token(TokenKind::RightParen);
        if (!if_stmt->closeParen)
        {
            error("Expected ')' after condition");
        }

        auto then_stmt = parse_statement();
        if_stmt->thenStatement = then_stmt.get_or_error("Expected then statement", this);

        // Optional else clause
        if (tokens.check(TokenKind::Else))
        {
            if_stmt->elseKeyword = consume_token(TokenKind::Else);
            auto else_stmt = parse_statement();
            if_stmt->elseStatement = else_stmt.get_or_error("Expected else statement", this)->as<StatementNode>();
        }

        return ParseResult<StatementNode>::ok(if_stmt);
    }

    ParseResult<StatementNode> Parser::parse_while_statement()
    {
        auto *while_stmt = alloc.alloc<WhileStatementNode>();

        while_stmt->whileKeyword = consume_token(TokenKind::While);
        if (!while_stmt->whileKeyword)
        {
            return ParseResult<StatementNode>::fail();
        }

        while_stmt->openParen = consume_token(TokenKind::LeftParen);
        if (!while_stmt->openParen)
        {
            error("Expected '(' after 'while'");
        }

        auto condition = parse_expression();
        while_stmt->condition = condition.get_or_error("Expected condition expression", this);

        while_stmt->closeParen = consume_token(TokenKind::RightParen);
        if (!while_stmt->closeParen)
        {
            error("Expected ')' after condition");
        }

        auto body = with_context(Context::LOOP, [this]()
                                 { return parse_statement(); });
        while_stmt->body = body.get_or_error("Expected loop body", this);

        return ParseResult<StatementNode>::ok(while_stmt);
    }

    ParseResult<StatementNode> Parser::parse_for_statement()
    {
        // Check if this is a for-in loop
        auto checkpoint = tokens.checkpoint();
        tokens.advance(); // skip 'for'
        tokens.consume(TokenKind::LeftParen);

        // Look for 'in' keyword to determine loop type
        bool is_for_in = false;
        int paren_depth = 1;
        while (!tokens.at_end() && paren_depth > 0)
        {
            if (tokens.check(TokenKind::LeftParen))
                paren_depth++;
            else if (tokens.check(TokenKind::RightParen))
                paren_depth--;
            else if (tokens.check(TokenKind::In) && paren_depth == 1)
            {
                is_for_in = true;
                break;
            }
            tokens.advance();
        }

        tokens.restore(checkpoint);

        if (is_for_in)
        {
            return parse_for_in_statement();
        }

        // Traditional for loop
        auto *for_stmt = alloc.alloc<ForStatementNode>();

        for_stmt->forKeyword = consume_token(TokenKind::For);
        for_stmt->openParen = consume_token(TokenKind::LeftParen);
        if (!for_stmt->openParen)
        {
            error("Expected '(' after 'for'");
        }

        // Parse initializer
        bool parsed_declaration = false;
        if (!tokens.check(TokenKind::Semicolon))
        {
            if (check_declaration())
            {
                auto init = parse_declaration();  // This consumes the semicolon
                if (init)
                {
                    for_stmt->initializer = init.get();
                    parsed_declaration = true;
                }
            }
            else
            {
                auto expr = parse_expression();
                if (expr)
                {
                    auto *expr_stmt = alloc.alloc<ExpressionStatementNode>();
                    expr_stmt->expression = expr.get();
                    for_stmt->initializer = expr_stmt;
                }
            }
        }

        // Only consume semicolon if we didn't parse a declaration (which already consumed it)
        if (!parsed_declaration)
        {
            for_stmt->firstSemicolon = consume_token(TokenKind::Semicolon);
            if (!for_stmt->firstSemicolon)
            {
                error("Expected ';' after for initializer");
            }
        }

        // Parse condition
        if (!tokens.check(TokenKind::Semicolon))
        {
            auto condition = parse_expression();
            for_stmt->condition = condition.get_or_error("Expected condition", this);
        }

        for_stmt->secondSemicolon = consume_token(TokenKind::Semicolon);
        if (!for_stmt->secondSemicolon)
        {
            error("Expected ';' after for condition");
        }

        // Parse incrementors
        std::vector<ExpressionNode *> incrementors;
        while (!tokens.check(TokenKind::RightParen) && !tokens.at_end())
        {
            auto incr = parse_expression();
            if (incr)
            {
                incrementors.push_back(incr.get());
            }
            else
            {
                break;
            }

            if (!tokens.consume(TokenKind::Comma))
            {
                break;
            }
        }
        for_stmt->incrementors = make_sized_array(incrementors);

        for_stmt->closeParen = consume_token(TokenKind::RightParen);
        if (!for_stmt->closeParen)
        {
            error("Expected ')' after for clauses");
        }

        auto body = with_context(Context::LOOP, [this]()
                                 { return parse_statement(); });
        for_stmt->body = body.get_or_error("Expected loop body", this);

        return ParseResult<StatementNode>::ok(for_stmt);
    }

    ParseResult<StatementNode> Parser::parse_for_in_statement()
    {
        auto *for_in = alloc.alloc<ForInStatementNode>();

        for_in->forKeyword = consume_token(TokenKind::For);
        for_in->openParen = consume_token(TokenKind::LeftParen);
        if (!for_in->openParen)
        {
            error("Expected '(' after 'for'");
        }

        // Parse loop variable
        auto var_pattern = parse_variable_pattern(true);
        if (!var_pattern)
        {
            error("Expected loop variable");
            return ParseResult<StatementNode>::fail();
        }
        for_in->mainVariable = convert_to_statement(*var_pattern.get());

        for_in->inKeyword = consume_token(TokenKind::In);
        if (!for_in->inKeyword)
        {
            error("Expected 'in' in for-in loop");
        }

        // Parse iterable expression
        auto iterable = parse_expression();
        for_in->iterable = iterable.get_or_error("Expected iterable expression", this);

        // Optional 'at' clause for index
        if (tokens.check(TokenKind::At))
        {
            for_in->atKeyword = consume_token(TokenKind::At);
            auto index_pattern = parse_variable_pattern(true);
            if (index_pattern)
            {
                for_in->indexVariable = convert_to_statement(*index_pattern.get());
            }
            else
            {
                error("Expected index variable after 'at'");
            }
        }

        for_in->closeParen = consume_token(TokenKind::RightParen);
        if (!for_in->closeParen)
        {
            error("Expected ')' after for-in clauses");
        }

        auto body = with_context(Context::LOOP, [this]()
                                 { return parse_statement(); });
        for_in->body = body.get_or_error("Expected loop body", this);

        return ParseResult<StatementNode>::ok(for_in);
    }

    ParseResult<StatementNode> Parser::parse_return_statement()
    {
        auto *ret_stmt = alloc.alloc<ReturnStatementNode>();

        ret_stmt->returnKeyword = consume_token(TokenKind::Return);
        if (!ret_stmt->returnKeyword)
        {
            return ParseResult<StatementNode>::fail();
        }

        if (!in_function() && !in_property_accessor())
        {
            warning("Return statement outside function");
        }

        // Optional return expression
        if (!tokens.check(TokenKind::Semicolon))
        {
            auto expr = parse_expression();
            if (expr)
            {
                ret_stmt->expression = expr.get();
            }
        }

        ret_stmt->semicolon = consume_token(TokenKind::Semicolon);
        if (!ret_stmt->semicolon)
        {
            error("Expected ';' after return statement");
        }

        return ParseResult<StatementNode>::ok(ret_stmt);
    }

    ParseResult<StatementNode> Parser::parse_break_statement()
    {
        auto *break_stmt = alloc.alloc<BreakStatementNode>();

        break_stmt->breakKeyword = consume_token(TokenKind::Break);
        if (!break_stmt->breakKeyword)
        {
            return ParseResult<StatementNode>::fail();
        }

        if (!in_loop())
        {
            warning("Break statement outside loop");
        }

        break_stmt->semicolon = consume_token(TokenKind::Semicolon);
        if (!break_stmt->semicolon)
        {
            error("Expected ';' after break statement");
        }

        return ParseResult<StatementNode>::ok(break_stmt);
    }

    ParseResult<StatementNode> Parser::parse_continue_statement()
    {
        auto *cont_stmt = alloc.alloc<ContinueStatementNode>();

        cont_stmt->continueKeyword = consume_token(TokenKind::Continue);
        if (!cont_stmt->continueKeyword)
        {
            return ParseResult<StatementNode>::fail();
        }

        if (!in_loop())
        {
            warning("Continue statement outside loop");
        }

        cont_stmt->semicolon = consume_token(TokenKind::Semicolon);
        if (!cont_stmt->semicolon)
        {
            error("Expected ';' after continue statement");
        }

        return ParseResult<StatementNode>::ok(cont_stmt);
    }

    ParseResult<StatementNode> Parser::parse_using_directive()
    {
        auto *using_dir = alloc.alloc<UsingDirectiveNode>();

        using_dir->usingKeyword = consume_token(TokenKind::Using);
        if (!using_dir->usingKeyword)
        {
            return ParseResult<StatementNode>::fail();
        }

        auto name = parse_qualified_name();
        using_dir->namespaceName = name.get_or_error("Expected namespace name", this);

        using_dir->semicolon = consume_token(TokenKind::Semicolon);
        if (!using_dir->semicolon)
        {
            error("Expected ';' after using directive");
        }

        return ParseResult<StatementNode>::ok(using_dir);
    }

    StatementNode *Parser::convert_to_statement(const VariablePattern &pattern)
    {
        switch (pattern.pattern)
        {
        case VarPattern::TYPED:
        {
            auto *var_decl = alloc.alloc<VariableDeclarationNode>();
            var_decl->type = pattern.type;
            var_decl->names = make_sized_array(std::vector<IdentifierNode *>{pattern.name});
            return var_decl;
        }
        case VarPattern::VAR:
        {
            auto *var_decl = alloc.alloc<VariableDeclarationNode>();
            var_decl->varKeyword = pattern.varKeyword;
            var_decl->names = make_sized_array(std::vector<IdentifierNode *>{pattern.name});
            return var_decl;
        }
        case VarPattern::IDENTIFIER:
        {
            auto *id_expr = alloc.alloc<IdentifierExpressionNode>();
            id_expr->identifier = pattern.name;
            auto *expr_stmt = alloc.alloc<ExpressionStatementNode>();
            expr_stmt->expression = id_expr;
            return expr_stmt;
        }
        }
        return reinterpret_cast<StatementNode *>(create_error_node("Invalid variable pattern"));
    }

    // #endregion

    // ================================================================================
    // #region Declarations
    // ================================================================================

    bool Parser::check_declaration()
    {
        // Check for modifiers
        if (tokens.current().is_modifier())
        {
            return true;
        }

        // Check for declaration keywords
        if (tokens.check_any({TokenKind::Fn, TokenKind::Type, TokenKind::Enum,
                             TokenKind::Var, TokenKind::Namespace}))
        {
            return true;
        }
        
        // Special handling for 'new' - distinguish between constructor declarations and new expressions
        if (tokens.check(TokenKind::New))
        {
            auto checkpoint = tokens.checkpoint();
            tokens.advance(); // skip 'new'
            bool is_constructor = tokens.check(TokenKind::LeftParen); // new( = constructor, new Identifier = expression
            tokens.restore(checkpoint);
            return is_constructor;
        }

        // Try to speculatively parse a typed identifier (for typed fields/properties)
        auto checkpoint = tokens.checkpoint();
        auto typed = try_parse_typed_identifier();
        tokens.restore(checkpoint);
        return typed.success();
    }

    ParseResult<DeclarationNode> Parser::parse_declaration()
    {
        // Parse modifiers
        auto modifiers = parse_modifiers();

        // Parse based on keyword
        if (tokens.check(TokenKind::Namespace))
        {
            return parse_namespace_declaration();
        }

        if (tokens.check(TokenKind::Type))
        {
            auto type_decl = parse_type_declaration();
            if (type_decl && !modifiers.empty())
            {
                type_decl->modifiers = make_sized_array(modifiers);
            }
            return type_decl;
        }

        if (tokens.check(TokenKind::Enum))
        {
            auto enum_decl = parse_enum_declaration();
            if (enum_decl && !modifiers.empty())
            {
                enum_decl->modifiers = make_sized_array(modifiers);
            }
            return enum_decl;
        }

        if (tokens.check(TokenKind::Fn))
        {
            auto func_decl = parse_function_declaration();
            if (func_decl && !modifiers.empty())
            {
                func_decl->modifiers = make_sized_array(modifiers);
            }
            return func_decl;
        }

        if (tokens.check(TokenKind::New))
        {
            auto ctor_decl = parse_constructor_declaration();
            if (ctor_decl && !modifiers.empty())
            {
                ctor_decl->modifiers = make_sized_array(modifiers);
            }
            return ctor_decl;
        }

        // Variable, field, or property declaration
        if (tokens.check(TokenKind::Var))
        {
            // Check if this is a var property (var name => expr or var name { get; set; })
            auto checkpoint = tokens.checkpoint();
            tokens.advance(); // skip 'var'
            
            if (tokens.check(TokenKind::Identifier))
            {
                tokens.advance(); // skip identifier
                
                // Check for property syntax after var identifier
                if (tokens.check(TokenKind::FatArrow) || tokens.check(TokenKind::LeftBrace))
                {
                    // This is a property: var name => expr or var name { get; set; }
                    tokens.restore(checkpoint);
                    auto prop_decl = parse_property_declaration();
                    if (prop_decl && !modifiers.empty())
                    {
                        prop_decl->modifiers = make_sized_array(modifiers);
                    }
                    return prop_decl;
                }
                else if (tokens.check(TokenKind::Assign))
                {
                    // Could be var name = value { get; set; }
                    tokens.advance(); // skip '='
                    auto init_expr = parse_expression();
                    if (init_expr && tokens.check(TokenKind::LeftBrace))
                    {
                        // This is a property with initializer
                        tokens.restore(checkpoint);
                        auto prop_decl = parse_property_declaration();
                        if (prop_decl && !modifiers.empty())
                        {
                            prop_decl->modifiers = make_sized_array(modifiers);
                        }
                        return prop_decl;
                    }
                }
            }
            
            // Regular var declaration
            tokens.restore(checkpoint);
            auto var_decl = parse_variable_declaration();
            if (var_decl && !modifiers.empty())
            {
                var_decl->modifiers = make_sized_array(modifiers);
            }
            return var_decl;
        }
        
        // Try typed declaration (could be field or property)
        auto checkpoint = tokens.checkpoint();
        if (auto typed = try_parse_typed_identifier())
        {
            bool is_property = false;
            
            // Check what follows the typed identifier
            if (tokens.check(TokenKind::FatArrow))
            {
                // Arrow property: Type name => expression
                is_property = true;
            }
            else if (tokens.check(TokenKind::LeftBrace))
            {
                // Property with accessor block: Type name { get; set; }
                is_property = true;
            }
            else if (tokens.check(TokenKind::Assign))
            {
                // Need to look ahead after the initializer
                auto lookahead_checkpoint = tokens.checkpoint();
                tokens.advance(); // skip '='
                
                // Try to parse the initializer expression
                auto init_expr = parse_expression();
                if (init_expr && tokens.check(TokenKind::LeftBrace))
                {
                    // Property with initializer and accessor block: Type name = value { get; set; }
                    is_property = true;
                }
                tokens.restore(lookahead_checkpoint);
            }
            
            tokens.restore(checkpoint);
            
            if (is_property)
            {
                auto prop_decl = parse_property_declaration();
                if (prop_decl && !modifiers.empty())
                {
                    prop_decl->modifiers = make_sized_array(modifiers);
                }
                return prop_decl;
            }
            else
            {
                // Regular typed field
                auto var_decl = parse_variable_declaration();
                if (var_decl && !modifiers.empty())
                {
                    var_decl->modifiers = make_sized_array(modifiers);
                }
                return var_decl;
            }
        }

        return ParseResult<DeclarationNode>::fail();
    }

    std::vector<ModifierKind> Parser::parse_modifiers()
    {
        std::vector<ModifierKind> modifiers;

        while (!tokens.at_end())
        {
            ModifierKind mod = ModifierKind::Invalid;
            if (!tokens.current().is_modifier()) return modifiers;
            
            mod = tokens.current().to_modifier_kind();
            modifiers.push_back(mod);
            tokens.advance();
        }

        return modifiers;
    }

    ParseResult<DeclarationNode> Parser::parse_namespace_declaration()
    {
        auto *ns_decl = alloc.alloc<NamespaceDeclarationNode>();

        ns_decl->namespaceKeyword = consume_token(TokenKind::Namespace);
        if (!ns_decl->namespaceKeyword)
        {
            return ParseResult<DeclarationNode>::fail();
        }

        auto name = parse_qualified_name();
        ns_decl->name = name.get_or_error("Expected namespace name", this);

        // Check for file-scoped namespace (ends with semicolon)
        if (tokens.check(TokenKind::Semicolon))
        {
            tokens.advance();
            return ParseResult<DeclarationNode>::ok(ns_decl);
        }

        // Block-scoped namespace
        if (tokens.check(TokenKind::LeftBrace))
        {
            auto body = with_context(Context::NAMESPACE, [this]()
                                     { return parse_block_statement(); });
            if (body)
            {
                ns_decl->body = static_cast<BlockStatementNode *>(body.get());
            }
        }
        else
        {
            error("Expected '{' or ';' after namespace declaration");
        }

        return ParseResult<DeclarationNode>::ok(ns_decl);
    }

    ParseResult<DeclarationNode> Parser::parse_type_declaration()
    {
        auto *type_decl = alloc.alloc<TypeDeclarationNode>();

        type_decl->typeKeyword = consume_token(TokenKind::Type);
        if (!type_decl->typeKeyword)
        {
            return ParseResult<DeclarationNode>::fail();
        }

        auto name = parse_identifier();
        type_decl->name = name.get_or_error("Expected type name", this);

        type_decl->openBrace = consume_token(TokenKind::LeftBrace);
        if (!type_decl->openBrace)
        {
            error("Expected '{' after type name");
            return ParseResult<DeclarationNode>::ok(type_decl);
        }

        std::vector<AstNode *> members;

        with_context(Context::TYPE_BODY, [&]()
                     {
            while (!tokens.check(TokenKind::RightBrace) && !tokens.at_end()) {
                // Try to parse any member declaration
                auto member = parse_declaration();
                if (member) {
                    members.push_back(member.get());
                } else {
                    error("Expected member declaration");
                    members.push_back(create_error_node("Invalid member declaration"));
                    synchronize();
                }
            } });

        type_decl->members = make_sized_array(members);

        type_decl->closeBrace = consume_token(TokenKind::RightBrace);
        if (!type_decl->closeBrace)
        {
            error("Expected '}' to close type declaration");
        }

        return ParseResult<DeclarationNode>::ok(type_decl);
    }

    ParseResult<DeclarationNode> Parser::parse_enum_declaration()
    {
        auto *enum_decl = alloc.alloc<EnumDeclarationNode>();

        enum_decl->enumKeyword = consume_token(TokenKind::Enum);
        if (!enum_decl->enumKeyword)
        {
            return ParseResult<DeclarationNode>::fail();
        }

        auto name = parse_identifier();
        enum_decl->name = name.get_or_error("Expected enum name", this);

        enum_decl->openBrace = consume_token(TokenKind::LeftBrace);
        if (!enum_decl->openBrace)
        {
            error("Expected '{' after enum name");
            return ParseResult<DeclarationNode>::ok(enum_decl);
        }

        std::vector<EnumCaseNode *> cases;
        std::vector<FunctionDeclarationNode *> methods;

        while (!tokens.check(TokenKind::RightBrace) && !tokens.at_end())
        {
            // Check for function (enum can have methods)
            if (tokens.check(TokenKind::Fn) || tokens.current().is_modifier())
            {
                auto func = parse_declaration();
                if (func)
                {
                    if (func->is_a<FunctionDeclarationNode>())
                    {
                        methods.push_back(static_cast<FunctionDeclarationNode *>(func.get()));
                    }
                    else
                    {
                        error("Expected function declaration in enum");
                        // Don't synchronize here - we might skip the closing brace
                    }
                }
                else
                {
                    error("Failed to parse declaration in enum");
                    // Skip just this one token and continue
                    tokens.advance();
                }
            }
            else if (tokens.check(TokenKind::Identifier))
            {
                // Parse enum case
                auto case_node = parse_enum_case();
                if (case_node)
                {
                    cases.push_back(case_node.get());
                }
                else
                {
                    // Skip just this identifier and continue
                    tokens.advance();
                }
            }
            else
            {
                // Unknown token in enum - skip it
                error("Unexpected token in enum declaration");
                tokens.advance();
            }

            // Optional comma or semicolon
            tokens.consume(TokenKind::Comma);
            tokens.consume(TokenKind::Semicolon);
        }

        enum_decl->cases = make_sized_array(cases);
        enum_decl->methods = make_sized_array(methods);

        enum_decl->closeBrace = consume_token(TokenKind::RightBrace);
        if (!enum_decl->closeBrace)
        {
            error("Expected '}' to close enum declaration");
        }

        return ParseResult<DeclarationNode>::ok(enum_decl);
    }

    ParseResult<EnumCaseNode> Parser::parse_enum_case()
    {
        if (!tokens.check(TokenKind::Identifier))
        {
            return ParseResult<EnumCaseNode>::fail();
        }

        auto *case_node = alloc.alloc<EnumCaseNode>();
        case_node->name = parse_identifier().get();

        // Check for associated data
        if (tokens.check(TokenKind::LeftParen))
        {
            case_node->openParen = consume_token(TokenKind::LeftParen);

            std::vector<ParameterNode *> params;
            while (!tokens.check(TokenKind::RightParen) && !tokens.at_end())
            {
                // Enum parameters can be: Type name or just Type
                auto checkpoint = tokens.checkpoint();

                if (auto typed = try_parse_typed_identifier())
                {
                    auto *param = alloc.alloc<ParameterNode>();
                    param->type = typed->type;
                    param->name = typed->name;
                    params.push_back(param);
                }
                else if (auto type = try_parse_type())
                {
                    auto *param = alloc.alloc<ParameterNode>();
                    param->type = type.get();
                    param->name = nullptr; // No name, just type
                    params.push_back(param);
                }
                else
                {
                    error("Expected parameter type");
                    break;
                }

                if (!tokens.consume(TokenKind::Comma))
                {
                    break;
                }
            }

            case_node->associatedData = make_sized_array(params);

            case_node->closeParen = consume_token(TokenKind::RightParen);
            if (!case_node->closeParen)
            {
                error("Expected ')' after enum case parameters");
            }
        }

        return ParseResult<EnumCaseNode>::ok(case_node);
    }

    ParseResult<DeclarationNode> Parser::parse_function_declaration()
    {
        auto *func_decl = alloc.alloc<FunctionDeclarationNode>();

        func_decl->fnKeyword = consume_token(TokenKind::Fn);
        if (!func_decl->fnKeyword)
        {
            return ParseResult<DeclarationNode>::fail();
        }

        auto name = parse_identifier();
        func_decl->name = name.get_or_error("Expected function name", this);

        // Parse parameter list
        func_decl->openParen = consume_token(TokenKind::LeftParen);
        if (!func_decl->openParen)
        {
            error("Expected '(' after function name");
            return ParseResult<DeclarationNode>::ok(func_decl);
        }

        // parse parameters
        std::vector<AstNode *> params;
        while (!tokens.check(TokenKind::RightParen) && !tokens.at_end())
        {
            auto param = parse_parameter();
            if (param)
            {
                params.push_back(param.get());
            }
            else
            {
                break;
            }

            if (!tokens.consume(TokenKind::Comma))
            {
                break;
            }
        }
        func_decl->parameters = make_sized_array(params);

        func_decl->closeParen = consume_token(TokenKind::RightParen);

        // Parse return type
        if (tokens.check(TokenKind::Colon))
        {
            tokens.advance();
            auto return_type = parse_type_expression();
            func_decl->returnType = return_type.get_or_error("Expected return type", this);
        }

        // Parse body or semicolon (for abstract functions)
        if (tokens.check(TokenKind::LeftBrace))
        {
            auto body = with_context(Context::FUNCTION, [this]()
                                     { return parse_block_statement(); });
            if (body)
            {
                func_decl->body = static_cast<BlockStatementNode *>(body.get());
            }
        }
        else if (tokens.check(TokenKind::Semicolon))
        {
            func_decl->semicolon = consume_token(TokenKind::Semicolon);
        }
        else
        {
            error("Expected '{' or ';' after function declaration");
        }

        return ParseResult<DeclarationNode>::ok(func_decl);
    }

    ParseResult<ParameterNode> Parser::parse_parameter()
    {
        // Parameters are always: Type name
        auto typed = try_parse_typed_identifier();
        if (!typed)
        {
            return ParseResult<ParameterNode>::fail();
        }

        auto *param = alloc.alloc<ParameterNode>();
        param->type = typed->type;
        param->name = typed->name;

        // Check for default value
        if (tokens.check(TokenKind::Assign))
        {
            param->equalsToken = consume_token(TokenKind::Assign);
            auto default_val = parse_expression();
            param->defaultValue = default_val.get_or_error("Expected default value", this);
        }

        return ParseResult<ParameterNode>::ok(param);
    }

    ParseResult<DeclarationNode> Parser::parse_constructor_declaration()
    {
        auto *ctor_decl = alloc.alloc<ConstructorDeclarationNode>();

        ctor_decl->newKeyword = consume_token(TokenKind::New);
        if (!ctor_decl->newKeyword)
        {
            return ParseResult<DeclarationNode>::fail();
        }

        ctor_decl->openParen = consume_token(TokenKind::LeftParen);
        if (!ctor_decl->openParen)
        {
            error("Expected '(' after 'new'");
            return ParseResult<DeclarationNode>::ok(ctor_decl);
        }

        // Parse parameters
        std::vector<ParameterNode *> params;
        while (!tokens.check(TokenKind::RightParen) && !tokens.at_end())
        {
            auto param = parse_parameter();
            if (param)
            {
                params.push_back(param.get());
            }
            else
            {
                break;
            }

            if (!tokens.consume(TokenKind::Comma))
            {
                break;
            }
        }
        ctor_decl->parameters = make_sized_array(params);

        ctor_decl->closeParen = consume_token(TokenKind::RightParen);
        if (!ctor_decl->closeParen)
        {
            error("Expected ')' after constructor parameters");
        }

        // Parse body
        if (tokens.check(TokenKind::LeftBrace))
        {
            auto body = with_context(Context::FUNCTION, [this]()
                                     { return parse_block_statement(); });
            if (body)
            {
                ctor_decl->body = static_cast<BlockStatementNode *>(body.get());
            }
        }
        else
        {
            error("Expected '{' for constructor body");
        }

        return ParseResult<DeclarationNode>::ok(ctor_decl);
    }

    ParseResult<DeclarationNode> Parser::parse_variable_declaration()
    {
        auto var_pattern = parse_variable_pattern(false);
        if (!var_pattern)
        {
            return ParseResult<DeclarationNode>::fail();
        }

        auto *var_decl = alloc.alloc<VariableDeclarationNode>();

        // Set type or var keyword based on pattern
        switch (var_pattern->pattern)
        {
        case VarPattern::TYPED:
            var_decl->type = var_pattern->type;
            break;
        case VarPattern::VAR:
            var_decl->varKeyword = var_pattern->varKeyword;
            break;
        default:
            error("Invalid variable declaration");
            return ParseResult<DeclarationNode>::fail();
        }

        // Parse name(s) - could be multiple: var x, y, z
        std::vector<IdentifierNode *> names;
        names.push_back(var_pattern->name);

        while (tokens.consume(TokenKind::Comma))
        {
            if (!tokens.check(TokenKind::Identifier))
            {
                error("Expected identifier after ','");
                break;
            }
            names.push_back(parse_identifier().get());
        }

        var_decl->names = make_sized_array(names);

        // Parse optional initializer
        if (tokens.check(TokenKind::Assign))
        {
            var_decl->equalsToken = consume_token(TokenKind::Assign);
            auto init = parse_expression();
            var_decl->initializer = init.get_or_error("Expected initializer", this);
        }

        var_decl->semicolon = consume_token(TokenKind::Semicolon);
        if (!var_decl->semicolon)
        {
            error("Expected ';' after variable declaration");
        }

        return ParseResult<DeclarationNode>::ok(var_decl);
    }


    ParseResult<DeclarationNode> Parser::parse_property_declaration()
    {
        auto *prop_decl = alloc.alloc<PropertyDeclarationNode>();
        
        // Properties can start with either 'var' or a type
        if (tokens.check(TokenKind::Var))
        {
            prop_decl->varKeyword = consume_token(TokenKind::Var);
            auto name = parse_identifier();
            if (!name)
            {
                error("Expected identifier after 'var'");
                return ParseResult<DeclarationNode>::fail();
            }
            prop_decl->name = name.get();
        }
        else
        {
            auto typed = try_parse_typed_identifier();
            if (!typed)
            {
                return ParseResult<DeclarationNode>::fail();
            }
            prop_decl->type = typed->type;
            prop_decl->name = typed->name;
        }

        // Check for initializer
        if (tokens.check(TokenKind::Assign))
        {
            tokens.advance();
            auto init = parse_expression();
            prop_decl->equals = create_token_node(tokens.previous());
            prop_decl->initializer = init.get_or_error("Expected initializer", this);
        }

        // Check for arrow getter
        if (tokens.check(TokenKind::FatArrow))
        {
            prop_decl->arrow = consume_token(TokenKind::FatArrow);
            auto getter = parse_expression();
            prop_decl->getterExpression = getter.get_or_error("Expected getter expression", this);
            
            // Arrow properties require a semicolon
            prop_decl->semicolon = consume_token(TokenKind::Semicolon);
            if (!prop_decl->semicolon)
            {
                error("Expected ';' after arrow property");
            }
        }
        // Check for accessor block
        else if (tokens.check(TokenKind::LeftBrace))
        {
            prop_decl->openBrace = consume_token(TokenKind::LeftBrace);

            std::vector<PropertyAccessorNode *> accessors;
            while (!tokens.check(TokenKind::RightBrace) && !tokens.at_end())
            {
                auto accessor = parse_property_accessor();
                if (accessor)
                {
                    accessors.push_back(accessor.get());
                }
                else
                {
                    tokens.skip_to_any({TokenKind::Semicolon, TokenKind::RightBrace});
                }
                tokens.consume(TokenKind::Semicolon);
            }

            prop_decl->accessors = make_sized_array(accessors);
            prop_decl->closeBrace = consume_token(TokenKind::RightBrace);
            // No semicolon after closing brace for accessor blocks
        }

        return ParseResult<DeclarationNode>::ok(prop_decl);
    }

    ParseResult<PropertyAccessorNode> Parser::parse_property_accessor()
    {
        auto *accessor = alloc.alloc<PropertyAccessorNode>();

        // Parse modifiers (public, private, etc.)
        accessor->modifiers = make_sized_array(parse_modifiers());

        // Check for get/set keyword
        if (!tokens.check_any({TokenKind::Get, TokenKind::Set}))
        {
            return ParseResult<PropertyAccessorNode>::fail();
        }

        accessor->accessorKeyword = create_token_node(tokens.current());
        bool is_getter = tokens.check(TokenKind::Get);
        tokens.advance();

        // Determine the appropriate context
        Context::Type accessor_context = is_getter ? Context::PROPERTY_GETTER : Context::PROPERTY_SETTER;

        // Check for arrow expression
        if (tokens.check(TokenKind::FatArrow))
        {
            accessor->arrow = consume_token(TokenKind::FatArrow);
            auto expr = with_context(accessor_context, [this]() {
                return parse_expression();
            });
            accessor->expression = expr.get_or_error("Expected accessor expression", this);
        }
        // Check for block
        else if (tokens.check(TokenKind::LeftBrace))
        {
            auto body = with_context(accessor_context, [this]() {
                return parse_block_statement();
            });
            if (body)
            {
                accessor->body = static_cast<BlockStatementNode *>(body.get());
            }
        }

        return ParseResult<PropertyAccessorNode>::ok(accessor);
    }

    // #endregion

    // ================================================================================
    // #region Types
    // ================================================================================

    ParseResult<TypeNameNode> Parser::try_parse_type()
    {
        auto checkpoint = tokens.checkpoint();

        // Try to parse qualified name
        auto qname = parse_qualified_name();
        if (!qname)
        {
            return ParseResult<TypeNameNode>::fail();
        }

        auto *type_name = alloc.alloc<TypeNameNode>();
        type_name->name = qname.get();

        // Check for array suffix []
        if (tokens.consume(TokenKind::LeftBracket))
        {
            if (!tokens.consume(TokenKind::RightBracket))
            {
                tokens.restore(checkpoint);
                return ParseResult<TypeNameNode>::fail();
            }

            auto *array_type = alloc.alloc<ArrayTypeNameNode>();
            array_type->elementType = type_name;
            return ParseResult<TypeNameNode>::ok(array_type);
        }

        // Check for generics <T, U>
        if (tokens.check(TokenKind::Less))
        {
            auto generics = parse_generic_arguments();
            if (!generics)
            {
                tokens.restore(checkpoint);
                return ParseResult<TypeNameNode>::fail();
            }

            auto *generic_type = alloc.alloc<GenericTypeNameNode>();
            generic_type->baseType = type_name;
            // Set generic arguments...
            return ParseResult<TypeNameNode>::ok(generic_type);
        }

        return ParseResult<TypeNameNode>::ok(type_name);
    }

    ParseResult<TypeNameNode> Parser::parse_type_expression()
    {
        auto type = try_parse_type();
        if (!type)
        {
            error("Expected type expression");
            return ParseResult<TypeNameNode>::fail();
        }
        return type;
    }

    ParseResult<QualifiedNameNode> Parser::parse_qualified_name()
    {
        if (!tokens.check(TokenKind::Identifier))
        {
            return ParseResult<QualifiedNameNode>::fail();
        }

        auto *qname = alloc.alloc<QualifiedNameNode>();
        std::vector<IdentifierNode *> identifiers;

        while (true)
        {
            if (!tokens.check(TokenKind::Identifier))
            {
                break;
            }

            auto id = parse_identifier();
            if (id)
            {
                identifiers.push_back(id.get());
            }

            if (!tokens.consume(TokenKind::Dot))
            {
                break;
            }
        }

        qname->identifiers = make_sized_array(identifiers);
        return ParseResult<QualifiedNameNode>::ok(qname);
    }

    ParseResult<TypeNameNode> Parser::parse_generic_arguments()
    {
        // TODO: Implement generic argument parsing
        return ParseResult<TypeNameNode>::fail();
    }

    // #endregion

    // ================================================================================
    // #region Expressions - Main
    // ================================================================================

    ParseResult<ExpressionNode> Parser::parse_expression(int min_precedence)
    {
        // Parse unary/primary with postfix already handled
        auto left = parse_expression_piece();
        if (!left)
        {
            return left;
        }

        // Then handle binary operators
        return parse_binary_expression(left.get(), min_precedence);
    }

    ParseResult<ExpressionNode> Parser::parse_expression_piece()
    {
        // Handle prefix operators
        if (tokens.check_any({TokenKind::Not, TokenKind::Minus,
                              TokenKind::Increment, TokenKind::Decrement}))
        {
            auto *unary = alloc.alloc<UnaryExpressionNode>();
            unary->operatorToken = create_token_node(tokens.current());
            unary->opKind = tokens.current().to_unary_operator_kind();
            unary->isPostfix = false;
            tokens.advance();

            // Recursive call for chained unary operators
            auto operand = parse_expression_piece();
            unary->operand = operand.get_or_error("Expected operand", this);
            return ParseResult<ExpressionNode>::ok(unary);
        }

        // Handle prefix range expressions (..<10)
        if (tokens.check_any({TokenKind::DotDot, TokenKind::DotDotEquals}))
        {
            auto *range = alloc.alloc<RangeExpressionNode>();
            range->rangeOp = create_token_node(tokens.current());
            tokens.advance();
            range->start = nullptr;

            auto end = parse_expression(6); // Additive precedence
            range->end = end.get_or_error("Expected end expression", this);
            return ParseResult<ExpressionNode>::ok(range);
        }

        // Parse primary
        auto expr = parse_primary_expression();
        if (!expr)
            return expr;

        // ALWAYS handle postfix operations
        return parse_postfix_expression(expr.get());
    }

    ParseResult<ExpressionNode> Parser::parse_binary_expression(ExpressionNode *left, int min_precedence)
    {
        while (!tokens.at_end())
        {
            auto op_info = get_operator_info(tokens.current().kind);
            if (!op_info || op_info->precedence < min_precedence)
            {
                break;
            }

            // Special handling for assignment operators
            if (op_info->is_assignment)
            {
                return parse_assignment_expression(left);
            }

            auto op_token = tokens.current();
            tokens.advance();

            // Special handling for range operators
            if (op_info->is_range)
            {
                return parse_range_suffix(left, op_token);
            }

            // Regular binary expression
            int next_prec = op_info->right_assoc ? op_info->precedence : op_info->precedence + 1;

            // This recursive call to parse_expression will handle postfix on the right side
            auto right = parse_expression(next_prec);

            if (!right)
            {
                error("Expected expression after operator");
                right = ParseResult<ExpressionNode>::ok(
                    reinterpret_cast<ExpressionNode *>(create_error_node("Missing operand")));
            }

            auto *binary = alloc.alloc<BinaryExpressionNode>();
            binary->left = left;
            binary->right = right.get();
            binary->opKind = op_token.to_binary_operator_kind();
            binary->operatorToken = create_token_node(op_token);

            left = binary;
        }

        return ParseResult<ExpressionNode>::ok(left);
    }

    ParseResult<ExpressionNode> Parser::parse_assignment_expression(ExpressionNode *left)
    {
        auto op_token = tokens.current();
        tokens.advance();

        // Parse right-hand side (right-associative)
        auto right = parse_expression(0);
        if (!right)
        {
            error("Expected expression after assignment operator");
            return ParseResult<ExpressionNode>::fail();
        }

        auto *assign = alloc.alloc<AssignmentExpressionNode>();
        assign->target = left;
        assign->source = right.get();
        assign->opKind = op_token.to_assignment_operator_kind();
        assign->operatorToken = create_token_node(op_token);

        return ParseResult<ExpressionNode>::ok(assign);
    }

    ParseResult<ExpressionNode> Parser::parse_range_suffix(ExpressionNode *start, const Token &op)
    {
        auto *range = alloc.alloc<RangeExpressionNode>();
        range->start = start;
        range->rangeOp = create_token_node(op);

        // Parse end (optional for start.. syntax)
        if (!tokens.check(TokenKind::By) && !is_expression_terminator())
        {
            auto end = parse_expression(6); // Parse at additive precedence
            if (end)
            {
                range->end = end.get();
            }
        }

        // Parse optional 'by' clause
        if (tokens.check(TokenKind::By))
        {
            range->byKeyword = consume_token(TokenKind::By);
            auto step = parse_expression(6);
            range->stepExpression = step.get_or_error("Expected step expression", this);
        }

        return ParseResult<ExpressionNode>::ok(range);
    }

    // #endregion

    // ================================================================================
    // #region Expressions - Primary
    // ================================================================================

    ParseResult<ExpressionNode> Parser::parse_primary_expression()
    {
        // Literals
        if (tokens.current().is_literal())
        {
            return parse_literal();
        }

        // Identifiers
        if (tokens.check(TokenKind::Identifier))
        {
            return parse_identifier_expression();
        }

        // This expressions
        if (tokens.check(TokenKind::This))
        {
            return parse_this_expression();
        }

        // Parenthesized expressions
        if (tokens.check(TokenKind::LeftParen))
        {
            return parse_parenthesized_expression();
        }

        // New expressions
        if (tokens.check(TokenKind::New))
        {
            return parse_new_expression();
        }

        // Match expressions
        if (tokens.check(TokenKind::Match))
        {
            return parse_match_expression();
        }

        // Prefix range (..<10)
        if (tokens.check_any({TokenKind::DotDot, TokenKind::DotDotEquals}))
        {
            return parse_prefix_expression();
        }

        return ParseResult<ExpressionNode>::fail();
    }

    ParseResult<ExpressionNode> Parser::parse_literal()
    {
        auto *literal = alloc.alloc<LiteralExpressionNode>();
        literal->token = create_token_node(tokens.current());
        literal->kind = tokens.current().to_literal_kind();
        tokens.advance();
        return ParseResult<ExpressionNode>::ok(literal);
    }

    ParseResult<ExpressionNode> Parser::parse_identifier_expression()
    {
        auto id = parse_identifier();
        if (!id)
        {
            return ParseResult<ExpressionNode>::fail();
        }

        auto *id_expr = alloc.alloc<IdentifierExpressionNode>();
        id_expr->identifier = id.get();

        return ParseResult<ExpressionNode>::ok(id_expr);
    }

    ParseResult<ExpressionNode> Parser::parse_this_expression()
    {
        if (!tokens.check(TokenKind::This))
        {
            return ParseResult<ExpressionNode>::fail();
        }

        auto *this_expr = alloc.alloc<ThisExpressionNode>();
        this_expr->thisKeyword = create_token_node(tokens.current());
        tokens.advance();

        return ParseResult<ExpressionNode>::ok(this_expr);
    }

    ParseResult<ExpressionNode> Parser::parse_parenthesized_expression()
    {
        auto *paren_expr = alloc.alloc<ParenthesizedExpressionNode>();

        paren_expr->openParen = consume_token(TokenKind::LeftParen);
        if (!paren_expr->openParen)
        {
            return ParseResult<ExpressionNode>::fail();
        }

        // Check for lambda expression
        auto checkpoint = tokens.checkpoint();
        bool is_lambda = false;

        // Simple heuristic: (param) => or (Type param) =>
        if (tokens.check(TokenKind::Identifier))
        {
            tokens.advance();
            if (tokens.check(TokenKind::FatArrow))
            {
                is_lambda = true;
            }
            else if (tokens.check(TokenKind::Identifier))
            {
                tokens.advance();
                if (tokens.check_any({TokenKind::Comma, TokenKind::RightParen}))
                {
                    if (tokens.consume(TokenKind::RightParen) && tokens.check(TokenKind::FatArrow))
                    {
                        is_lambda = true;
                    }
                }
            }
        }

        tokens.restore(checkpoint);

        if (is_lambda)
        {
            return parse_lambda_expression();
        }

        auto expr = parse_expression();
        paren_expr->expression = expr.get_or_error("Expected expression", this);

        paren_expr->closeParen = consume_token(TokenKind::RightParen);
        if (!paren_expr->closeParen)
        {
            error("Expected ')' after expression");
        }

        return ParseResult<ExpressionNode>::ok(paren_expr);
    }

    ParseResult<ExpressionNode> Parser::parse_new_expression()
    {
        auto *new_expr = alloc.alloc<NewExpressionNode>();

        new_expr->newKeyword = consume_token(TokenKind::New);
        if (!new_expr->newKeyword)
        {
            return ParseResult<ExpressionNode>::fail();
        }

        auto type = parse_type_expression();
        new_expr->type = type.get_or_error("Expected type after 'new'", this);

        // Optional constructor call
        if (tokens.check(TokenKind::LeftParen))
        {
            auto call = parse_call_suffix(nullptr);
            if (call)
            {
                new_expr->constructorCall = static_cast<CallExpressionNode *>(call.get());
            }
        }

        return ParseResult<ExpressionNode>::ok(new_expr);
    }

    ParseResult<ExpressionNode> Parser::parse_match_expression()
    {
        // TODO: Implement match expression parsing
        return ParseResult<ExpressionNode>::fail();
    }

    ParseResult<ExpressionNode> Parser::parse_lambda_expression()
    {
        // TODO: Implement lambda expression parsing
        return ParseResult<ExpressionNode>::fail();
    }

    // #endregion

    // ================================================================================
    // #region Expressions - Prefix
    // ================================================================================

    ParseResult<ExpressionNode> Parser::parse_prefix_expression()
    {
        // Unary operators
        if (tokens.check_any({TokenKind::Not, TokenKind::Minus,
                              TokenKind::Increment, TokenKind::Decrement}))
        {
            return parse_unary_expression();
        }

        // Prefix range expressions (..<10)
        if (tokens.check_any({TokenKind::DotDot, TokenKind::DotDotEquals}))
        {
            auto *range = alloc.alloc<RangeExpressionNode>();
            range->rangeOp = create_token_node(tokens.current());
            tokens.advance();

            range->start = nullptr; // No start for prefix range

            // Parse end expression
            auto end = parse_expression(6); // Additive precedence
            range->end = end.get_or_error("Expected end expression", this);

            // Prefix ranges don't support 'by' clause typically
            return ParseResult<ExpressionNode>::ok(range);
        }

        return parse_primary_expression();
    }

    ParseResult<ExpressionNode> Parser::parse_unary_expression()
    {
        auto *unary = alloc.alloc<UnaryExpressionNode>();

        unary->operatorToken = create_token_node(tokens.current());
        unary->opKind = tokens.current().to_unary_operator_kind();
        unary->isPostfix = false;
        tokens.advance();

        auto operand = parse_prefix_expression();
        unary->operand = operand.get_or_error("Expected operand", this);

        return ParseResult<ExpressionNode>::ok(unary);
    }

    // #endregion

    // ================================================================================
    // #region Expressions - Postfix
    // ================================================================================

    ParseResult<ExpressionNode> Parser::parse_postfix_expression(ExpressionNode *expr)
    {
        while (true)
        {
            // Function call
            if (tokens.check(TokenKind::LeftParen))
            {
                auto call = parse_call_suffix(expr);
                if (!call)
                    break;
                expr = call.get();
            }
            // Member access
            else if (tokens.check(TokenKind::Dot))
            {
                auto member = parse_member_access_suffix(expr);
                if (!member)
                    break;
                expr = member.get();
            }
            // Array indexing
            else if (tokens.check(TokenKind::LeftBracket))
            {
                auto index = parse_index_suffix(expr);
                if (!index)
                    break;
                expr = index.get();
            }
            // Postfix increment/decrement
            else if (tokens.check_any({TokenKind::Increment, TokenKind::Decrement}))
            {
                auto *unary = alloc.alloc<UnaryExpressionNode>();
                unary->operatorToken = create_token_node(tokens.current());
                unary->opKind = tokens.current().to_unary_operator_kind();
                unary->isPostfix = true;
                unary->operand = expr;
                tokens.advance();
                expr = unary;
            }
            else
            {
                break;
            }
        }

        return ParseResult<ExpressionNode>::ok(expr);
    }

    ParseResult<ExpressionNode> Parser::parse_call_suffix(ExpressionNode *target)
    {
        auto *call = alloc.alloc<CallExpressionNode>();
        call->target = target;

        call->openParen = consume_token(TokenKind::LeftParen);
        if (!call->openParen)
        {
            return ParseResult<ExpressionNode>::fail();
        }

        std::vector<AstNode *> arguments;
        std::vector<TokenNode *> commas;

        while (!tokens.check(TokenKind::RightParen) && !tokens.at_end())
        {
            auto arg = parse_expression();
            if (arg)
            {
                arguments.push_back(arg.get());
            }
            else
            {
                arguments.push_back(create_error_node("Invalid argument"));
                tokens.skip_to_any({TokenKind::Comma, TokenKind::RightParen});
            }

            if (tokens.check(TokenKind::Comma))
            {
                commas.push_back(create_token_node(tokens.current()));
                tokens.advance();
            }
            else
            {
                break;
            }
        }

        call->arguments = make_sized_array(arguments);
        call->commas = make_sized_array(commas);

        call->closeParen = consume_token(TokenKind::RightParen);
        if (!call->closeParen)
        {
            error("Expected ')' after arguments");
        }

        return ParseResult<ExpressionNode>::ok(call);
    }

    ParseResult<ExpressionNode> Parser::parse_member_access_suffix(ExpressionNode *target)
    {
        auto *member = alloc.alloc<MemberAccessExpressionNode>();
        member->target = target;

        member->dotToken = consume_token(TokenKind::Dot);
        if (!member->dotToken)
        {
            return ParseResult<ExpressionNode>::fail();
        }

        // Check for enum pattern (.EnumCase)
        if (!tokens.check(TokenKind::Identifier))
        {
            error("Expected member name after '.'");
            return ParseResult<ExpressionNode>::fail();
        }

        auto id = parse_identifier();
        member->member = id.get_or_error("Expected member name", this);

        return ParseResult<ExpressionNode>::ok(member);
    }

    ParseResult<ExpressionNode> Parser::parse_index_suffix(ExpressionNode *target)
    {
        auto *indexer = alloc.alloc<IndexerExpressionNode>();
        indexer->target = target;

        indexer->openBracket = consume_token(TokenKind::LeftBracket);
        if (!indexer->openBracket)
        {
            return ParseResult<ExpressionNode>::fail();
        }

        auto index = parse_expression();
        indexer->index = index.get_or_error("Expected index expression", this);

        indexer->closeBracket = consume_token(TokenKind::RightBracket);
        if (!indexer->closeBracket)
        {
            error("Expected ']' after index");
        }

        return ParseResult<ExpressionNode>::ok(indexer);
    }

    // #endregion

    // ================================================================================
    // #region Identifiers
    // ================================================================================

    ParseResult<IdentifierNode> Parser::parse_identifier()
    {
        if (!tokens.check(TokenKind::Identifier))
        {
            return ParseResult<IdentifierNode>::fail();
        }

        auto *id = alloc.alloc<IdentifierNode>();
        id->name = tokens.current().text;
        id->location = tokens.current().location;
        tokens.advance();

        return ParseResult<IdentifierNode>::ok(id);
    }

    // #endregion

} // namespace Myre