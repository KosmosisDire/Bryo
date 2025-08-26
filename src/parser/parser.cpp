#include "parser/parser.hpp"

// #define REQUIRE_SEMI
#ifdef REQUIRE_SEMI
#define HANDLE_SEMI \
    expect(TokenKind::Semicolon, "Expected ';' to end statement");
#else
#define HANDLE_SEMI \
    requireSemicolonIfSameLine();
#endif

namespace Bryo
{
    Parser::Parser(TokenStream &tokens) : tokens(tokens)
    {
        contextStack.push_back(Context::TOP_LEVEL);
    }

    Parser::~Parser() = default;

    // ================== Public API ==================

    CompilationUnit *Parser::parse()
    {
        auto unit = arena.make<CompilationUnit>();
        if (tokens.at_end())
        {
            unit->location = {{0, 1, 1}, 0};
            unit->topLevelStatements = arena.emptyList<Statement *>();
            return unit;
        }

        auto startToken = tokens.current();
        std::vector<Statement *> statements;

        while (!tokens.at_end())
        {
            auto stmt = parseTopLevelStatement();
            if (stmt)
            {
                statements.push_back(stmt);
            }
        }

        unit->topLevelStatements = arena.makeList(statements);
        // Use previous token unless the file was empty to begin with.
        unit->location = SourceRange(startToken.location.start, tokens.previous().location.end());
        return unit;
    }

    const std::vector<Parser::ParseError> &Parser::getErrors() const
    {
        return errors;
    }

    bool Parser::hasErrors() const
    {
        return !errors.empty();
    }

    // ================== Error Handling ==================

    void Parser::error(const std::string &msg)
    {
        static int lastPos = 0;
        if (lastPos == tokens.position())
        {
            synchronize();
            return;
        }
        lastPos = tokens.position();

        errors.push_back({msg, tokens.current().location, ParseError::ERROR});
    }

    void Parser::warning(const std::string &msg)
    {
        errors.push_back({msg, tokens.current().location, ParseError::WARNING});
    }

    ErrorExpression *Parser::errorExpr(const std::string &msg)
    {
        error(msg);
        auto err = arena.makeErrorExpr(msg);
        err->location = tokens.previous().location;
        return err;
    }

    ErrorStatement *Parser::errorStmt(const std::string &msg)
    {
        error(msg);
        auto err = arena.makeErrorStmt(msg);
        err->location = tokens.previous().location;
        return err;
    }

    void Parser::synchronize()
    {
        while (!tokens.at_end())
        {
            tokens.advance();

            if (consume(TokenKind::Semicolon))
            {
                return;
            }

            if (tokens.current().starts_declaration() || tokens.current().starts_statement())
            {
                return;
            }
        }
    }

    // ================== Context Management ==================

    bool Parser::inLoop() const
    {
        for (auto it = contextStack.rbegin(); it != contextStack.rend(); ++it)
        {
            if (*it == Context::LOOP)
                return true;
        }
        return false;
    }

    bool Parser::inFunction() const
    {
        for (auto it = contextStack.rbegin(); it != contextStack.rend(); ++it)
        {
            if (*it == Context::FUNCTION)
                return true;
        }
        return false;
    }

    bool Parser::inGetter() const
    {
        for (auto it = contextStack.rbegin(); it != contextStack.rend(); ++it)
        {
            if (*it == Context::PROPERTY_GETTER)
                return true;
        }
        return false;
    }

    bool Parser::inSetter() const
    {
        for (auto it = contextStack.rbegin(); it != contextStack.rend(); ++it)
        {
            if (*it == Context::PROPERTY_SETTER)
                return true;
        }
        return false;
    }

    bool Parser::inTypeBody() const
    {
        if (contextStack.back() == Context::TYPE_BODY)
            return true;
        return false;
    }

    // ================== Utility Helpers ==================

    bool Parser::check(TokenKind kind)
    {
        return tokens.check(kind);
    }

    bool Parser::checkAny(std::initializer_list<TokenKind> kinds)
    {
        return tokens.check_any(kinds);
    }

    bool Parser::consume(TokenKind kind)
    {
        return tokens.consume(kind);
    }

    bool Parser::expect(TokenKind kind, const std::string &msg)
    {
        if (!consume(kind))
        {
            error(msg);
            return false;
        }
        return true;
    }

    Token Parser::previous()
    {
        return tokens.previous();
    }

    TokenKind Parser::peekNext()
    {
        auto checkpoint = tokens.checkpoint();
        tokens.advance();
        TokenKind next = tokens.current().kind;
        tokens.restore(checkpoint);
        return next;
    }

    // ================== Top Level Parsing ==================

    Statement *Parser::parseTopLevelStatement()
    {
        if (check(TokenKind::Using))
        {
            return parseUsingDirective();
        }
        if (check(TokenKind::Namespace))
        {
            return parseNamespaceDecl(tokens.current());
        }
        if (tokens.current().is_modifier() || checkDeclarationStart())
        {
            return parseDeclaration();
        }
        if (tokens.current().starts_expression())
        {
            return parseExpressionStatement();
        }
        error("Unexpected token at top level");
        synchronize();
        return errorStmt("Invalid top level construct");
    }

    // ================== Declarations ==================

    bool Parser::checkDeclarationStart()
    {
        // 'new' should only be treated as a declaration start inside type bodies
        if (check(TokenKind::New))
        {
            return inTypeBody();
        }

        return checkAny({TokenKind::Type, TokenKind::Enum, TokenKind::Fn,
                         TokenKind::Var, TokenKind::Ref, TokenKind::Namespace});
    }

    Declaration *Parser::parseDeclaration()
    {
        auto startToken = tokens.current();
        ModifierKindFlags modifiers = parseModifiers();

        if (check(TokenKind::Namespace))
        {
            auto ns = parseNamespaceDecl(startToken);
            ns->modifiers = modifiers;
            return ns;
        }
        if (checkAny({TokenKind::Type, TokenKind::Ref, TokenKind::Enum, TokenKind::Static}))
        {
            return parseTypeDecl(modifiers, startToken);
        }
        if (check(TokenKind::Fn))
        {
            return parseFunctionDecl(modifiers, startToken);
        }
        if (check(TokenKind::New))
        {
            return parseConstructorDecl(modifiers, startToken);
        }
        if (check(TokenKind::Var))
        {
            return parseVarDeclaration(modifiers, startToken);
        }

        auto checkpoint = tokens.checkpoint();
        auto type = parseTypeExpression();
        if (type && check(TokenKind::Identifier))
        {
            // Parse all comma-separated declarations with the same type
            auto allDeclarations = parseTypedMemberDeclarations(modifiers, type, startToken);
            // TODO: Return the first one for now (caller will need to handle multiple)
            return allDeclarations.empty() ? nullptr : allDeclarations[0];
        }

        tokens.restore(checkpoint);
        error("Expected declaration");
        return nullptr;
    }

    ModifierKindFlags Parser::parseModifiers()
    {
        ModifierKindFlags mods = ModifierKindFlags::None;
        while (tokens.current().is_modifier())
        {
            mods |= tokens.current().to_modifier_kind();
            tokens.advance();
        }
        return mods;
    }

    TypeDecl *Parser::parseTypeDecl(ModifierKindFlags modifiers, const Token &startToken)
    {
        auto decl = arena.make<TypeDecl>();
        decl->modifiers = modifiers;

        if (consume(TokenKind::Static))
        {
            expect(TokenKind::Type, "Expected 'type' after 'static'");
            decl->kind = TypeDecl::Kind::StaticType;
        }
        else if (consume(TokenKind::Ref))
        {
            expect(TokenKind::Type, "Expected 'type' after 'ref'");
            decl->kind = TypeDecl::Kind::RefType;
        }
        else if (consume(TokenKind::Enum))
        {
            decl->kind = TypeDecl::Kind::Enum;
        }
        else if (consume(TokenKind::Type))
        {
            decl->kind = TypeDecl::Kind::Type;
        }
        else
        {
            error("Expected type declaration keyword");
            decl->name = arena.makeIdentifier("");
            decl->location = startToken.location;
            return decl;
        }

        if (check(TokenKind::Identifier))
        {
            decl->name = parseIdentifier();
        }
        else
        {
            error("Expected type name");
            decl->name = arena.makeIdentifier("");
        }

        // Parse generic type parameters: <T, U, V>
        if (check(TokenKind::Less))
        {
            decl->typeParameters = parseTypeParameterList();
        }
        else
        {
            decl->typeParameters = arena.emptyList<TypeParameterDecl *>();
        }

        if (consume(TokenKind::Colon))
        {
            decl->baseTypes = parseBaseTypeList();
        }
        else
        {
            decl->baseTypes = arena.emptyList<Expression *>();
        }

        expect(TokenKind::LeftBrace, "Expected '{' after type declaration");

        std::vector<Declaration *> members;
        withContext(Context::TYPE_BODY, [&]()
                    {
        while (!check(TokenKind::RightBrace) && !tokens.at_end()) {
            if (decl->kind == TypeDecl::Kind::Enum) {
                if (tokens.current().is_modifier() || check(TokenKind::Fn)) {
                    if (auto member = parseDeclaration()) members.push_back(member);
                } else if (check(TokenKind::Identifier)) {
                    if (auto enumCase = parseEnumCase()) members.push_back(enumCase);
                } else {
                    error("Unexpected token in enum body");
                    tokens.advance();
                }
            } else {
                // Check if this is a typed declaration that could have multiple comma-separated variables
                auto checkpoint = tokens.checkpoint();
                auto type = parseTypeExpression();
                if (type && check(TokenKind::Identifier))
                {
                    // Parse all comma-separated declarations with the same type
                    auto declarations = parseTypedMemberDeclarations(ModifierKindFlags::None, type, previous());
                    for (auto decl : declarations) {
                        members.push_back(decl);
                    }
                } else {
                    tokens.restore(checkpoint);
                    if (auto member = parseDeclaration()) {
                        members.push_back(member);
                    }
                }
            }
            consume(TokenKind::Comma);
            consume(TokenKind::Semicolon);
        } });

        decl->members = arena.makeList(members);
        expect(TokenKind::RightBrace, "Expected '}' to close type declaration");

        decl->location = SourceRange(startToken.location.start, previous().location.end());
        return decl;
    }

    EnumCaseDecl *Parser::parseEnumCase()
    {
        auto startToken = tokens.current();
        auto decl = arena.make<EnumCaseDecl>();
        decl->name = parseIdentifier();

        if (check(TokenKind::LeftParen))
        {
            decl->associatedData = parseParameterList();
        }
        else
        {
            decl->associatedData = arena.emptyList<ParameterDecl *>();
        }

        decl->location = SourceRange(startToken.location.start, previous().location.end());
        return decl;
    }

    FunctionDecl *Parser::parseFunctionDecl(ModifierKindFlags modifiers, const Token &startToken)
    {
        auto decl = arena.make<FunctionDecl>();
        decl->modifiers = modifiers;

        consume(TokenKind::Fn);
        decl->name = parseIdentifier();
        decl->parameters = parseParameterList();

        if (consume(TokenKind::Colon))
        {
            decl->returnType = parseTypeExpression();
            if (!decl->returnType)
            {
                decl->returnType = errorExpr("Expected return type");
            }
        }
        else
        {
            decl->returnType = nullptr; // void
        }

        if (check(TokenKind::LeftBrace))
        {
            decl->body = withContext(Context::FUNCTION, [this]()
                                     { return parseBlock(); });
        }
        else if (consume(TokenKind::Semicolon))
        {
            decl->body = nullptr; // abstract or interface
        }
        else
        {
            error("Expected '{' or ';' after function declaration");
            decl->body = nullptr;
        }

        decl->location = SourceRange(startToken.location.start, previous().location.end());
        return decl;
    }

    ConstructorDecl *Parser::parseConstructorDecl(ModifierKindFlags modifiers, const Token &startToken)
    {
        auto decl = arena.make<ConstructorDecl>();
        decl->modifiers = modifiers;

        consume(TokenKind::New);
        decl->parameters = parseParameterList();

        expect(TokenKind::LeftBrace, "Expected '{' after constructor parameters");
        decl->body = withContext(Context::FUNCTION, [this]()
                                 { return parseBlock(); });

        if (!decl->body)
        {
            auto block = arena.make<Block>();
            block->location = previous().location;
            block->statements = arena.emptyList<Statement *>();
            decl->body = block;
        }

        decl->location = SourceRange(startToken.location.start, previous().location.end());
        return decl;
    }

    Declaration *Parser::parseVarDeclaration(ModifierKindFlags modifiers, const Token &startToken)
    {
        auto checkpoint = tokens.checkpoint();
        consume(TokenKind::Var);

        if (!check(TokenKind::Identifier))
        {
            error("Expected identifier after 'var'");
            tokens.restore(checkpoint);
            return nullptr;
        }
        auto name = parseIdentifier();

        // Check if this is a property (has arrow syntax or braces after optional initializer)
        bool isProperty = false;
        Expression *initializer = nullptr;

        if (checkAny({TokenKind::FatArrow, TokenKind::LeftBrace}))
        {
            isProperty = true;
        }
        else if (check(TokenKind::Assign))
        {
            auto initCheckpoint = tokens.checkpoint();
            tokens.advance();
            parseExpression();
            if (check(TokenKind::LeftBrace))
            {
                isProperty = true;
                tokens.restore(initCheckpoint);
                consume(TokenKind::Assign);
                initializer = parseExpression();
            }
            else
            {
                tokens.restore(initCheckpoint);
            }
        }

        if (isProperty)
        {
            // Create PropertyDecl with embedded VariableDecl
            auto prop = arena.make<PropertyDecl>();
            prop->modifiers = modifiers;

            // Create the underlying variable
            auto varDecl = arena.make<VariableDecl>();
            varDecl->modifiers = modifiers;

            auto ti = arena.make<TypedIdentifier>();
            ti->name = name;
            ti->type = nullptr; // Type inference for var
            ti->location = SourceRange(startToken.location.start, name->location.end());

            varDecl->variable = ti;
            varDecl->initializer = initializer;
            varDecl->location = SourceRange(startToken.location.start, previous().location.end());

            prop->variable = varDecl;

            if (consume(TokenKind::FatArrow))
            {
                auto getter = arena.make<PropertyAccessor>();
                getter->kind = PropertyAccessor::Kind::Get;
                getter->body = parseExpression();
                prop->getter = getter;
                prop->setter = nullptr;
                HANDLE_SEMI
            }
            else if (check(TokenKind::LeftBrace))
            {
                parsePropertyAccessors(prop);
            }

            prop->location = SourceRange(startToken.location.start, previous().location.end());
            return prop;
        }

        // Always create VariableDecl - the context doesn't matter for the AST structure
        auto decl = arena.make<VariableDecl>();
        decl->modifiers = modifiers;

        auto ti = arena.make<TypedIdentifier>();
        ti->name = name;

        // Only type inference allowed for var declarations
        ti->type = nullptr; // Type inference

        ti->location = SourceRange(startToken.location.start, previous().location.end());
        decl->variable = ti;

        if (consume(TokenKind::Assign))
        {
            decl->initializer = parseExpression();
            if (!decl->initializer)
            {
                decl->initializer = errorExpr("Expected initializer");
            }
        }
        else
        {
            decl->initializer = nullptr;
        }

        HANDLE_SEMI
        decl->location = SourceRange(startToken.location.start, previous().location.end());
        return decl;
    }

    Expression *Parser::convertToArrayTypeIfNeeded(Expression *expr)
    {
        if (auto indexer = expr->as<IndexerExpr>())
        {
            // Convert IndexerExpr to ArrayTypeExpr for type declarations
            // Check if the index is a literal (array size)
            if (auto literal = indexer->index->as<LiteralExpr>())
            {
                if (literal->kind == LiteralKind::I32 ||
                    literal->kind == LiteralKind::I64 ||
                    literal->kind == LiteralKind::I8)
                {
                    auto arrayType = arena.make<ArrayTypeExpr>();
                    arrayType->baseType = indexer->object;
                    arrayType->size = literal;
                    arrayType->location = indexer->location;
                    return arrayType;
                }
            }
            // For non-literal indices, still convert but without size
            auto arrayType = arena.make<ArrayTypeExpr>();
            arrayType->baseType = indexer->object;
            arrayType->size = nullptr;
            arrayType->location = indexer->location;
            return arrayType;
        }
        return expr;
    }

    std::vector<Declaration *> Parser::parseTypedMemberDeclarations(ModifierKindFlags modifiers, Expression *type, const Token &startToken)
    {
        type = convertToArrayTypeIfNeeded(type);
        std::vector<Declaration *> declarations;
        bool hasProperties = false;

        do
        {
            auto fieldStartToken = tokens.current();
            auto name = parseIdentifier();
            Expression *initializer = nullptr;

            if (consume(TokenKind::Assign))
            {
                initializer = parseExpression();
            }

            if (checkAny({TokenKind::FatArrow, TokenKind::LeftBrace}))
            {
                // Create PropertyDecl with embedded VariableDecl
                auto prop = arena.make<PropertyDecl>();
                prop->modifiers = modifiers;

                // Create the underlying variable
                auto varDecl = arena.make<VariableDecl>();
                varDecl->modifiers = modifiers;

                auto ti = arena.make<TypedIdentifier>();
                ti->name = name;
                ti->type = type;
                ti->location = SourceRange(fieldStartToken.location.start, name->location.end());

                varDecl->variable = ti;
                varDecl->initializer = initializer;
                varDecl->location = SourceRange(fieldStartToken.location.start, previous().location.end());

                prop->variable = varDecl;

                if (consume(TokenKind::FatArrow))
                {
                    auto getter = arena.make<PropertyAccessor>();
                    getter->kind = PropertyAccessor::Kind::Get;
                    getter->body = parseExpression();
                    prop->getter = getter;
                    prop->setter = nullptr;
                    HANDLE_SEMI
                }
                else if (check(TokenKind::LeftBrace))
                {
                    parsePropertyAccessors(prop);
                }
                prop->location = SourceRange(fieldStartToken.location.start, previous().location.end());
                declarations.push_back(prop);
                hasProperties = true;
            }
            else
            {
                // Create regular VariableDecl for fields
                auto field = arena.make<VariableDecl>();
                field->modifiers = modifiers;

                auto ti = arena.make<TypedIdentifier>();
                ti->name = name;
                ti->type = type;
                ti->location = SourceRange(fieldStartToken.location.start, name->location.end());

                field->variable = ti;
                field->initializer = initializer;
                field->location = SourceRange(fieldStartToken.location.start, previous().location.end());
                declarations.push_back(field);
            }
        } while (consume(TokenKind::Comma));

        // Only expect semicolon for regular fields, not properties
        if (!hasProperties)
        {
            HANDLE_SEMI
        }

        // Set location for all declarations to span from the type to the end
        for (auto decl : declarations)
        {
            decl->location = SourceRange(startToken.location.start, previous().location.end());
        }

        return declarations;
    }

    void Parser::parsePropertyAccessors(PropertyDecl *prop)
    {
        consume(TokenKind::LeftBrace);
        while (!check(TokenKind::RightBrace) && !tokens.at_end())
        {
            auto accessorStartToken = tokens.current();
            ModifierKindFlags accessorMods = parseModifiers();

            if (consume(TokenKind::Get))
            {
                auto getter = arena.make<PropertyAccessor>();
                getter->kind = PropertyAccessor::Kind::Get;
                getter->modifiers = accessorMods;

                if (consume(TokenKind::FatArrow))
                {
                    if (check(TokenKind::LeftBrace))
                    {
                        error("Unexpected '{' after '=>' in property getter. Use either '=> expression' or '{ statements }'");
                        getter->body = withContext(Context::PROPERTY_GETTER, [this]()
                                                   { return parseBlock(); });
                    }
                    else
                    {
                        getter->body = withContext(Context::PROPERTY_GETTER, [this]()
                                                   { return parseExpression(); });
                    }
                }
                else if (check(TokenKind::LeftBrace))
                {
                    getter->body = withContext(Context::PROPERTY_GETTER, [this]()
                                               { return parseBlock(); });
                }
                else
                {
                    getter->body = std::monostate{};
                }
                getter->location = SourceRange(accessorStartToken.location.start, previous().location.end());
                prop->getter = getter;
            }
            else if (consume(TokenKind::Set))
            {
                auto setter = arena.make<PropertyAccessor>();
                setter->kind = PropertyAccessor::Kind::Set;
                setter->modifiers = accessorMods;

                if (consume(TokenKind::FatArrow))
                {
                    if (check(TokenKind::LeftBrace))
                    {
                        error("Unexpected '{' after '=>' in property setter. Use either '=> expression' or '{ statements }'");
                        setter->body = withContext(Context::PROPERTY_SETTER, [this]()
                                                   { return parseBlock(); });
                    }
                    else
                    {
                        setter->body = withContext(Context::PROPERTY_SETTER, [this]()
                                                   { return parseExpression(); });
                    }
                }
                else if (check(TokenKind::LeftBrace))
                {
                    setter->body = withContext(Context::PROPERTY_SETTER, [this]()
                                               { return parseBlock(); });
                }
                else
                {
                    setter->body = std::monostate{};
                }
                setter->location = SourceRange(accessorStartToken.location.start, previous().location.end());
                prop->setter = setter;
            }
            else
            {
                error("Expected 'get' or 'set' in property accessor");
                while (!tokens.at_end() &&
                       !check(TokenKind::Semicolon) &&
                       !check(TokenKind::RightBrace) &&
                       !check(TokenKind::Get) &&
                       !check(TokenKind::Set))
                {
                    tokens.advance();
                }
            }
            consume(TokenKind::Semicolon);
        }
        expect(TokenKind::RightBrace, "Expected '}' after property accessors");
    }

    NamespaceDecl *Parser::parseNamespaceDecl(const Token &startToken)
    {
        auto decl = arena.make<NamespaceDecl>();
        consume(TokenKind::Namespace);

        // Parse the namespace name as an expression (can be qualified)
        decl->name = parseNameExpression(); // Single identifier for now
        if (consume(TokenKind::Dot))
        {
            // Handle qualified names like "System.Collections"
            auto memberAccess = arena.make<MemberAccessExpr>();
            memberAccess->object = decl->name;
            memberAccess->member = parseIdentifier();
            decl->name = memberAccess;
            // Continue parsing additional dots
            while (consume(TokenKind::Dot))
            {
                auto nextMember = arena.make<MemberAccessExpr>();
                nextMember->object = decl->name;
                nextMember->member = parseIdentifier();
                decl->name = nextMember;
            }
        }

        if (consume(TokenKind::Semicolon))
        {
            decl->isFileScoped = true;
            decl->body = std::nullopt;
        }
        else if (consume(TokenKind::LeftBrace))
        {
            decl->isFileScoped = false;
            std::vector<Statement *> statements;
            withContext(Context::NAMESPACE, [&]()
                        {
            while (!check(TokenKind::RightBrace) && !tokens.at_end()) {
                if (auto stmt = parseTopLevelStatement()) statements.push_back(stmt);
            } });
            decl->body = arena.makeList(statements);
            expect(TokenKind::RightBrace, "Expected '}' after namespace body");
        }
        else
        {
            error("Expected ';' or '{' after namespace declaration");
            decl->isFileScoped = true;
            decl->body = std::nullopt;
        }
        decl->location = SourceRange(startToken.location.start, previous().location.end());
        return decl;
    }

    // ================== Statements ==================

    Statement *Parser::parseStatement()
    {
        auto startToken = tokens.current();
        if (check(TokenKind::If))
            return parseIfStatement();
        if (check(TokenKind::While))
            return parseWhileStatement();
        if (check(TokenKind::For))
            return parseForStatement();
        if (check(TokenKind::Return))
            return parseReturnStatement();
        if (check(TokenKind::Break))
            return parseBreakStatement();
        if (check(TokenKind::Continue))
            return parseContinueStatement();
        if (check(TokenKind::LeftBrace))
            return parseBlock();
        if (tokens.current().is_modifier() || checkDeclarationStart())
        {
            return parseDeclaration();
        }

        if (check(TokenKind::Identifier))
        {
            auto checkpoint = tokens.checkpoint();
            auto type = parseTypeExpression();
            if (type && check(TokenKind::Identifier))
            {
                // TODO: This could be a typed variable declaration with multiple comma-separated variables
                // For now, just return the first one, but we should handle this better
                tokens.restore(checkpoint);
                return parseDeclaration();
            }
            tokens.restore(checkpoint);
        }

        return parseExpressionStatement();
    }

    Block *Parser::parseBlock()
    {
        auto startToken = tokens.current();
        auto block = arena.make<Block>();
        consume(TokenKind::LeftBrace);

        std::vector<Statement *> statements;
        while (!check(TokenKind::RightBrace) && !tokens.at_end())
        {
            // Check for typed declarations that might have multiple comma-separated variables
            if (check(TokenKind::Identifier))
            {
                auto checkpoint = tokens.checkpoint();
                auto type = parseTypeExpression();
                if (type && check(TokenKind::Identifier))
                {
                    // Parse all comma-separated declarations with the same type
                    auto declarations = parseTypedMemberDeclarations(ModifierKindFlags::None, type, previous());
                    for (auto decl : declarations)
                    {
                        statements.push_back(decl);
                    }
                }
                else
                {
                    tokens.restore(checkpoint);
                    if (auto stmt = parseStatement())
                    {
                        statements.push_back(stmt);
                    }
                }
            }
            else if (auto stmt = parseStatement())
            {
                statements.push_back(stmt);
            }
        }
        block->statements = arena.makeList(statements);

        expect(TokenKind::RightBrace, "Expected '}' to close block");
        block->location = SourceRange(startToken.location.start, previous().location.end());
        return block;
    }

    Statement *Parser::parseIfStatement()
    {
        auto startToken = tokens.current();
        consume(TokenKind::If);

        // Check if parentheses are present
        bool hasParens = consume(TokenKind::LeftParen);

        auto condition = parseExpression();
        if (!condition)
            condition = errorExpr("Expected condition");

        // If we had an opening paren, expect a closing one
        if (hasParens)
        {
            expect(TokenKind::RightParen, "Expected ')' after condition");
        }

        auto thenStmt = parseStatement();
        if (!thenStmt)
            thenStmt = errorStmt("Expected then statement");

        Statement *elseStmt = nullptr;
        if (consume(TokenKind::Else))
        {
            elseStmt = parseStatement();
            if (!elseStmt)
                elseStmt = errorStmt("Expected else statement");
        }

        auto ifExpr = arena.make<IfExpr>();
        ifExpr->condition = condition;
        ifExpr->thenBranch = thenStmt;
        ifExpr->elseBranch = elseStmt;
        ifExpr->location = SourceRange(startToken.location.start, previous().location.end());

        return ifExpr;
    }

    WhileStmt *Parser::parseWhileStatement()
    {
        auto startToken = tokens.current();
        auto stmt = arena.make<WhileStmt>();
        consume(TokenKind::While);

        // Check if parentheses are present
        bool hasParens = consume(TokenKind::LeftParen);

        stmt->condition = parseExpression();
        if (!stmt->condition)
            stmt->condition = errorExpr("Expected condition");

        // If we had an opening paren, expect a closing one
        if (hasParens)
        {
            expect(TokenKind::RightParen, "Expected ')' after condition");
        }

        stmt->body = withContext(Context::LOOP, [this]()
                                 { return parseStatement(); });
        if (!stmt->body)
            stmt->body = errorStmt("Expected loop body");

        stmt->location = SourceRange(startToken.location.start, previous().location.end());
        return stmt;
    }

    Statement *Parser::parseForStatement()
    {
        return parseTraditionalForStatement();
    }

    ForStmt *Parser::parseTraditionalForStatement()
    {
        auto startToken = tokens.current();
        auto stmt = arena.make<ForStmt>();
        consume(TokenKind::For);

        // Check if parentheses are present
        bool hasParens = consume(TokenKind::LeftParen);

        // Parse initializer
        if (!check(TokenKind::Semicolon))
        {
            stmt->initializer = parseStatement();
        }

        // Parse condition
        if (!check(TokenKind::Semicolon))
        {
            stmt->condition = parseExpression();
        }
        else
        {
            stmt->condition = nullptr;
        }
        HANDLE_SEMI

        // Parse updates
        std::vector<Expression *> updates;

        // If we have parentheses, parse until we hit the closing paren
        // Otherwise, parse a single expression (or none if we hit the loop body)
        if (hasParens)
        {
            while (!check(TokenKind::RightParen) && !tokens.at_end())
            {
                if (auto update = parseExpression())
                {
                    updates.push_back(update);
                }
                if (!consume(TokenKind::Comma))
                    break;
            }
        }
        else
        {
            // Without parens, only parse updates if we don't immediately see a statement starter
            if (!check(TokenKind::LeftBrace) && !tokens.current().starts_statement())
            {
                if (auto update = parseExpression())
                {
                    updates.push_back(update);
                    while (consume(TokenKind::Comma))
                    {
                        if (auto nextUpdate = parseExpression())
                        {
                            updates.push_back(nextUpdate);
                        }
                    }
                }
            }
        }
        stmt->updates = arena.makeList(updates);

        // If we had an opening paren, expect a closing one
        if (hasParens)
        {
            expect(TokenKind::RightParen, "Expected ')' after for clauses");
        }

        stmt->body = withContext(Context::LOOP, [this]()
                                 { return parseStatement(); });
        if (!stmt->body)
            stmt->body = errorStmt("Expected loop body");

        stmt->location = SourceRange(startToken.location.start, previous().location.end());
        return stmt;
    }

    ReturnStmt *Parser::parseReturnStatement()
    {
        auto startToken = tokens.current();
        auto stmt = arena.make<ReturnStmt>();
        consume(TokenKind::Return);

        if (!check(TokenKind::Semicolon))
        {
            stmt->value = parseExpression();
        }
        else
        {
            stmt->value = nullptr;
        }

        HANDLE_SEMI
        if (!inFunction() && !inGetter() && !inSetter())
        {
            warning("Return statement outside function or property");
        }
        stmt->location = SourceRange(startToken.location.start, previous().location.end());
        return stmt;
    }

    BreakStmt *Parser::parseBreakStatement()
    {
        auto startToken = tokens.current();
        auto stmt = arena.make<BreakStmt>();
        consume(TokenKind::Break);
        HANDLE_SEMI
        if (!inLoop())
        {
            warning("Break statement outside loop");
        }
        stmt->location = SourceRange(startToken.location.start, previous().location.end());
        return stmt;
    }

    ContinueStmt *Parser::parseContinueStatement()
    {
        auto startToken = tokens.current();
        auto stmt = arena.make<ContinueStmt>();
        consume(TokenKind::Continue);
        HANDLE_SEMI
        if (!inLoop())
        {
            warning("Continue statement outside loop");
        }
        stmt->location = SourceRange(startToken.location.start, previous().location.end());
        return stmt;
    }

    ExpressionStmt *Parser::parseExpressionStatement()
    {
        auto stmt = arena.make<ExpressionStmt>();
        stmt->expression = parseExpression();
        if (!stmt->expression)
        {
            stmt->expression = errorExpr("Expected expression");
        }
        HANDLE_SEMI
        stmt->location = SourceRange(stmt->expression->location.start, previous().location.end());
        return stmt;
    }

    UsingDirective *Parser::parseUsingDirective()
    {
        auto startToken = tokens.current();
        auto directive = arena.make<UsingDirective>();
        consume(TokenKind::Using);

        auto checkpoint = tokens.checkpoint();
        if (check(TokenKind::Identifier))
        {
            auto firstId = parseIdentifier();
            if (consume(TokenKind::Assign))
            {
                directive->kind = UsingDirective::Kind::Alias;
                directive->alias = firstId;
                directive->aliasedType = parseExpression();
                if (!directive->aliasedType)
                {
                    directive->aliasedType = errorExpr("Expected type after '='");
                }
                directive->target = nullptr;
            }
            else
            {
                tokens.restore(checkpoint);
                directive->kind = UsingDirective::Kind::Namespace;
                directive->target = parseNameExpression(); // Parse the target namespace
                if (consume(TokenKind::Dot))
                {
                    // Handle qualified names like "System.Collections"
                    auto memberAccess = arena.make<MemberAccessExpr>();
                    memberAccess->object = directive->target;
                    memberAccess->member = parseIdentifier();
                    directive->target = memberAccess;
                    // Continue parsing additional dots
                    while (consume(TokenKind::Dot))
                    {
                        auto nextMember = arena.make<MemberAccessExpr>();
                        nextMember->object = directive->target;
                        nextMember->member = parseIdentifier();
                        directive->target = nextMember;
                    }
                }
                directive->alias = nullptr;
                directive->aliasedType = nullptr;
            }
        }

        HANDLE_SEMI
        directive->location = SourceRange(startToken.location.start, previous().location.end());
        return directive;
    }

    // ================== Expressions ==================

    Expression *Parser::parseExpression(int minPrecedence)
    {
        auto left = parsePrimaryExpression();
        if (!left)
        {
            return nullptr;
        }
        return parseBinaryExpression(left, minPrecedence);
    }

    Expression *Parser::parseBinaryExpression(Expression *left, int minPrecedence)
    {
        while (!tokens.at_end())
        {
            Token op = tokens.current();
            int precedence = op.get_binary_precedence();

            if (op.kind == TokenKind::Question)
            {
                if (minPrecedence > precedence)
                    break;

                tokens.advance();

                auto conditional = arena.make<ConditionalExpr>();
                conditional->condition = left;

                conditional->thenExpr = parseExpression(precedence + 1);
                if (!conditional->thenExpr)
                    conditional->thenExpr = errorExpr("Expected expression after '?'");

                expect(TokenKind::Colon, "Expected ':' in conditional expression");

                conditional->elseExpr = parseExpression(precedence);
                if (!conditional->elseExpr)
                    conditional->elseExpr = errorExpr("Expected expression after ':'");

                conditional->location = SourceRange(left->location.start, conditional->elseExpr->location.end());
                return conditional;
            }

            if (precedence < minPrecedence || precedence == 0)
                break;

            if (op.is_assignment_operator())
            {
                tokens.advance();
                auto assign = arena.make<AssignmentExpr>();
                assign->target = left;
                assign->op = op.to_assignment_operator_kind();
                assign->value = parseExpression(precedence);
                if (!assign->value)
                    assign->value = errorExpr("Expected value in assignment");

                assign->location = SourceRange(left->location.start, assign->value->location.end());
                return assign;
            }

            tokens.advance();
            int nextPrecedence = op.is_right_associative() ? precedence : precedence + 1;

            auto right = parseExpression(nextPrecedence);
            if (!right)
                right = errorExpr("Expected right operand");

            auto binary = arena.make<BinaryExpr>();
            binary->left = left;
            binary->op = op.to_binary_operator_kind();
            binary->right = right;
            binary->location = SourceRange(left->location.start, right->location.end());
            left = binary;
        }
        return left;
    }

    Expression *Parser::parsePrimaryExpression()
    {
        Expression *expr = nullptr;
        if (tokens.current().is_unary_operator())
        {
            expr = parseUnaryExpression();
        }
        else if (tokens.current().is_literal())
        {
            expr = parseLiteral();
        }
        else if (check(TokenKind::Identifier))
        {
            auto checkpoint = tokens.checkpoint();
            tokens.advance();
            if (check(TokenKind::FatArrow))
            {
                tokens.restore(checkpoint);
                expr = parseLambdaExpression();
            }
            else
            {
                tokens.restore(checkpoint);
                expr = parseNameExpression();
            }
        }
        else if (check(TokenKind::This))
        {
            auto startToken = tokens.current();
            auto thisExpr = arena.make<ThisExpr>();
            tokens.advance();
            thisExpr->location = startToken.location;
            expr = thisExpr;
        }
        else if (check(TokenKind::LeftParen))
        {
            expr = parseParenthesizedOrLambda();
        }
        else if (check(TokenKind::LeftBracket))
        {
            expr = parseArrayLiteral();
        }
        else if (check(TokenKind::New))
        {
            expr = parseNewExpression();
        }
        else if (check(TokenKind::Typeof))
        {
            expr = parseTypeOfExpression();
        }
        else if (check(TokenKind::Sizeof))
        {
            expr = parseSizeOfExpression();
        }

        if (!expr)
        {
            return nullptr;
        }

        return parsePostfixExpression(expr);
    }

    Expression *Parser::parsePostfixExpression(Expression *expr)
    {
        while (true)
        {
            if (check(TokenKind::LeftParen))
            {
                tokens.advance();
                auto call = arena.make<CallExpr>();
                call->callee = expr;

                std::vector<Expression *> args;
                while (!check(TokenKind::RightParen) && !tokens.at_end())
                {
                    auto arg = parseExpression();
                    if (!arg)
                        break;
                    args.push_back(arg);
                    if (!consume(TokenKind::Comma))
                        break;
                    if (check(TokenKind::RightParen))
                        break;
                }
                call->arguments = arena.makeList(args);

                expect(TokenKind::RightParen, "Expected ')' after arguments");
                call->location = SourceRange(expr->location.start, previous().location.end());
                expr = call;
            }
            else if (check(TokenKind::Dot))
            {
                tokens.advance();
                auto member = arena.make<MemberAccessExpr>();
                member->object = expr;
                member->member = parseIdentifier();
                member->location = SourceRange(expr->location.start, member->member->location.end());
                expr = member;
            }
            else if (check(TokenKind::LeftBracket))
            {
                tokens.advance();

                // Regular indexer expression (arr[index])
                auto indexer = arena.make<IndexerExpr>();
                indexer->object = expr;
                indexer->index = parseExpression();
                if (!indexer->index)
                    indexer->index = errorExpr("Expected index expression");

                expect(TokenKind::RightBracket, "Expected ']' after index");
                indexer->location = SourceRange(expr->location.start, previous().location.end());
                expr = indexer;
            }
            else if (checkAny({TokenKind::Increment, TokenKind::Decrement}))
            {
                auto unary = arena.make<UnaryExpr>();
                unary->operand = expr;
                unary->op = tokens.current().to_unary_operator_kind();
                unary->isPostfix = true;
                tokens.advance();
                unary->location = SourceRange(expr->location.start, previous().location.end());
                expr = unary;
            }
            else
            {
                break;
            }
        }
        return expr;
    }

    Expression *Parser::parseUnaryExpression()
    {
        auto startToken = tokens.current();
        auto unary = arena.make<UnaryExpr>();
        Token op = tokens.current();
        tokens.advance();
        unary->op = op.to_unary_operator_kind();
        unary->isPostfix = false;
        unary->operand = parseExpression(op.get_unary_precedence());
        if (!unary->operand)
        {
            unary->operand = errorExpr("Expected operand after unary operator");
        }
        unary->location = SourceRange(startToken.location.start, unary->operand->location.end());
        return unary;
    }

    LiteralExpr *Parser::parseLiteral()
    {
        auto lit = arena.make<LiteralExpr>();
        Token tok = tokens.current();
        lit->location = tok.location;
        lit->value = tok.text;
        lit->kind = tok.to_literal_kind();
        tokens.advance();
        return lit;
    }

    Expression *Parser::parseNameExpression()
    {
        auto nameExpr = arena.make<NameExpr>();
        nameExpr->name = parseIdentifier();
        nameExpr->location = nameExpr->name->location;
        return nameExpr;
    }

    List<Expression *> Parser::parseGenericArgs()
    {
        auto cp = tokens.checkpoint();
        if (!check(TokenKind::Less))
            return {};

        consume(TokenKind::Less);
        std::vector<Expression *> typeArgs;

        while (!check(TokenKind::Greater) && !tokens.at_end())
        {
            auto typeArg = parseTypeExpression();
            if (!typeArg)
                break;
            typeArgs.push_back(typeArg);

            if (!consume(TokenKind::Comma))
                break;
        }

        if (!(expect(TokenKind::Greater, "Expected '>' to close generic type arguments")))
        {
            tokens.restore(cp);
            return {};
        }

        return arena.makeList(typeArgs);
    }

    Expression *Parser::parseTypeExpression()
    {
        if (!check(TokenKind::Identifier))
        {
            return nullptr; // Don't error, just return null
        }

        auto nameExpr = parseNameExpression();
        List<Expression *> typeArgs = parseGenericArgs();
        Expression *baseType = nameExpr;

        if (!typeArgs.empty())
        {
            auto genericType = arena.make<GenericTypeExpr>();
            genericType->baseType = baseType;
            genericType->typeArguments = typeArgs;
            baseType = genericType;
        }

        // Speculatively check for array type syntax
        if (check(TokenKind::LeftBracket))
        {
            auto checkpoint = tokens.checkpoint();
            tokens.advance(); // consume [

            // Check if this looks like array type syntax (empty or literal)
            bool isArrayType = false;
            if (check(TokenKind::RightBracket))
            {
                isArrayType = true; // Empty brackets: Type[]
            }
            else if (check(TokenKind::LiteralI32) || check(TokenKind::LiteralI64) || check(TokenKind::LiteralI8))
            {
                tokens.advance(); // consume literal
                if (check(TokenKind::RightBracket))
                {
                    isArrayType = true; // Literal size: Type[10]
                }
            }

            if (!isArrayType)
            {
                // Not array type syntax, restore and return base type
                tokens.restore(checkpoint);
                return baseType;
            }

            // It is array type syntax, restore and parse properly
            tokens.restore(checkpoint);
            consume(TokenKind::LeftBracket);

            auto arrayType = arena.make<ArrayTypeExpr>();
            arrayType->baseType = baseType;

            if (check(TokenKind::LiteralI32) || check(TokenKind::LiteralI64) || check(TokenKind::LiteralI8))
            {
                arrayType->size = parseLiteral();
            }

            if (!consume(TokenKind::RightBracket))
            {
                // If we determined it was array type syntax but can't parse it,
                // return null to indicate parse failure
                return nullptr;
            }

            arrayType->location = SourceRange(baseType->location.start, previous().location.end());
            baseType = arrayType;
        }

        if (consume(TokenKind::Asterisk))
        {
            auto pointerType = arena.make<PointerTypeExpr>();
            pointerType->baseType = baseType;
            baseType = pointerType;
        }

        return baseType;
    }

    Expression *Parser::parseParenthesizedOrLambda()
    {
        auto checkpoint = tokens.checkpoint();
        consume(TokenKind::LeftParen);

        // Check if this might be a cast expression
        // A cast looks like (Type)expr where Type is an identifier that could be a type
        bool isCast = false;
        bool isLambda = false;
        int parenDepth = 1;

        // First, check if this could be a cast by looking for a type pattern
        if (check(TokenKind::Identifier))
        {
            auto typeCheckpoint = tokens.checkpoint();

            // Try to parse as type expression
            auto potentialType = parseTypeExpression();

            // If we successfully parsed a type and the next token is ')', it's likely a cast
            if (potentialType && check(TokenKind::RightParen))
            {
                tokens.advance(); // consume the ')'

                // Check what follows - if it's an expression-starting token, it's a cast
                if (tokens.current().starts_expression() &&
                    !check(TokenKind::Semicolon) &&
                    !checkAny({TokenKind::Comma, TokenKind::RightParen, TokenKind::RightBracket}))
                {
                    isCast = true;
                }
            }

            tokens.restore(typeCheckpoint);
        }

        if (!isCast)
        {
            // Original lambda detection logic
            while (!tokens.at_end() && parenDepth > 0)
            {
                if (check(TokenKind::LeftParen))
                    parenDepth++;
                else if (check(TokenKind::RightParen))
                {
                    parenDepth--;
                    if (parenDepth == 0)
                    {
                        tokens.advance();
                        if (check(TokenKind::FatArrow))
                            isLambda = true;
                        break;
                    }
                }
                else if (check(TokenKind::FatArrow) && parenDepth == 1)
                {
                    isLambda = true;
                    break;
                }
                tokens.advance();
            }
        }

        tokens.restore(checkpoint);

        if (isCast)
        {
            return parseCastExpression();
        }
        else if (isLambda)
        {
            return parseLambdaExpression();
        }
        else
        {
            consume(TokenKind::LeftParen);
            if (check(TokenKind::RightParen))
            {
                error("Empty parentheses are not a valid expression");
                consume(TokenKind::RightParen);
                return errorExpr("Expected expression in parentheses");
            }

            auto expr = parseExpression();
            if (!expr)
                return errorExpr("Expected expression in parentheses");

            expect(TokenKind::RightParen, "Expected ')' after expression");
            return expr;
        }
    }

    Expression *Parser::parseCastExpression()
    {
        auto startToken = tokens.current();
        consume(TokenKind::LeftParen);

        // Parse the target type
        auto targetType = parseTypeExpression();
        if (!targetType)
        {
            error("Expected type in cast expression");
            targetType = errorExpr("Expected cast type");
        }

        expect(TokenKind::RightParen, "Expected ')' after cast type");

        // Parse the expression to cast - use higher precedence to ensure we don't
        // accidentally grab binary operators that should apply after the cast
        auto expr = parseExpression(14); // Use unary precedence level
        if (!expr)
        {
            expr = errorExpr("Expected expression after cast");
        }

        auto cast = arena.make<CastExpr>();
        cast->targetType = targetType;
        cast->expression = expr;
        cast->location = SourceRange(startToken.location.start, expr->location.end());
        return cast;
    }

    Expression *Parser::parseArrayLiteral()
    {
        auto startToken = tokens.current();
        auto array = arena.make<ArrayLiteralExpr>();
        consume(TokenKind::LeftBracket);

        std::vector<Expression *> elements;
        while (!check(TokenKind::RightBracket) && !tokens.at_end())
        {
            if (auto elem = parseExpression())
            {
                elements.push_back(elem);
            }
            if (!consume(TokenKind::Comma))
                break;
        }
        array->elements = arena.makeList(elements);

        expect(TokenKind::RightBracket, "Expected ']' after array elements");
        array->location = SourceRange(startToken.location.start, previous().location.end());
        return array;
    }

    Expression *Parser::parseNewExpression()
    {
        auto startToken = tokens.current();
        auto newExpr = arena.make<NewExpr>();
        consume(TokenKind::New);

        // Parse constructor type - use parseTypeExpression to handle generics properly
        if (check(TokenKind::Identifier))
        {
            newExpr->type = parseTypeExpression();
            if (!newExpr->type)
            {
                newExpr->type = errorExpr("Expected type name after 'new'");
            }
        }
        else
        {
            newExpr->type = errorExpr("Expected type name after 'new'");
        }

        if (check(TokenKind::LeftParen))
        {
            consume(TokenKind::LeftParen);
            std::vector<Expression *> args;
            while (!check(TokenKind::RightParen) && !tokens.at_end())
            {
                if (auto arg = parseExpression())
                    args.push_back(arg);
                if (!consume(TokenKind::Comma))
                    break;
            }
            newExpr->arguments = arena.makeList(args);
            expect(TokenKind::RightParen, "Expected ')' after constructor arguments");
        }
        else
        {
            newExpr->arguments = arena.emptyList<Expression *>();
        }
        newExpr->location = SourceRange(startToken.location.start, previous().location.end());
        return newExpr;
    }

    Expression *Parser::parseLambdaExpression()
    {
        auto startToken = tokens.current();
        auto lambda = arena.make<LambdaExpr>();

        if (check(TokenKind::LeftParen))
        {
            consume(TokenKind::LeftParen);
            std::vector<ParameterDecl *> params;
            while (!check(TokenKind::RightParen) && !tokens.at_end())
            {
                auto param = arena.make<ParameterDecl>();
                auto paramStart = tokens.current();
                param->param = parseTypedIdentifier();
                if (!param->param)
                {
                    auto ti = arena.make<TypedIdentifier>();
                    ti->type = errorExpr("Expected parameter type");
                    ti->name = arena.makeIdentifier("");
                    param->param = ti;
                }
                param->defaultValue = nullptr;
                param->location = SourceRange(paramStart.location.start, previous().location.end());
                params.push_back(param);
                if (!consume(TokenKind::Comma))
                    break;
            }
            lambda->parameters = arena.makeList(params);
            expect(TokenKind::RightParen, "Expected ')' after lambda parameters");
        }
        else
        {
            std::vector<ParameterDecl *> params;
            auto param = arena.make<ParameterDecl>();
            auto paramStart = tokens.current();
            auto ti = arena.make<TypedIdentifier>();
            ti->type = nullptr;
            ti->name = parseIdentifier();
            ti->location = ti->name->location;
            param->param = ti;
            param->defaultValue = nullptr;
            param->location = paramStart.location;
            params.push_back(param);
            lambda->parameters = arena.makeList(params);
        }

        expect(TokenKind::FatArrow, "Expected '=>' after lambda parameters");

        if (check(TokenKind::LeftBrace))
        {
            lambda->body = parseBlock();
        }
        else
        {
            auto expr = parseExpression();
            if (!expr)
                expr = errorExpr("Expected lambda body");
            auto exprStmt = arena.make<ExpressionStmt>();
            exprStmt->expression = expr;
            exprStmt->location = expr->location;
            lambda->body = exprStmt;
        }
        lambda->location = SourceRange(startToken.location.start, previous().location.end());
        return lambda;
    }

    Expression *Parser::parseTypeOfExpression()
    {
        auto startToken = tokens.current();
        auto typeOf = arena.make<TypeOfExpr>();
        consume(TokenKind::Typeof);
        expect(TokenKind::LeftParen, "Expected '(' after 'typeof'");

        typeOf->type = parseTypeExpression();
        if (!typeOf->type)
            typeOf->type = errorExpr("Expected type");

        expect(TokenKind::RightParen, "Expected ')' after type");
        typeOf->location = SourceRange(startToken.location.start, previous().location.end());
        return typeOf;
    }

    Expression *Parser::parseSizeOfExpression()
    {
        auto startToken = tokens.current();
        auto sizeOf = arena.make<SizeOfExpr>();
        consume(TokenKind::Sizeof);
        expect(TokenKind::LeftParen, "Expected '(' after 'sizeof'");

        sizeOf->type = parseTypeExpression();
        if (!sizeOf->type)
            sizeOf->type = errorExpr("Expected type");

        expect(TokenKind::RightParen, "Expected ')' after type");
        sizeOf->location = SourceRange(startToken.location.start, previous().location.end());
        return sizeOf;
    }

    // ================== Types ==================

    // ================== Helper Functions ==================

    Identifier *Parser::parseIdentifier()
    {
        if (!check(TokenKind::Identifier))
        {
            error("Expected identifier");
            auto id = arena.makeIdentifier("");
            id->location = tokens.current().location;
            return id;
        }
        auto tok = tokens.current();
        auto id = arena.makeIdentifier(tok.text);
        id->location = tok.location;
        tokens.advance();
        return id;
    }

    TypedIdentifier *Parser::parseTypedIdentifier()
    {
        auto startToken = tokens.current();
        auto ti = arena.make<TypedIdentifier>();
        if (consume(TokenKind::Var))
        {
            ti->type = nullptr;
            ti->name = parseIdentifier();
        }
        else
        {
            ti->type = parseTypeExpression();
            if (!ti->type)
            {
                error("Expected type specification");
                ti->type = errorExpr("Expected type");
            }
            ti->name = parseIdentifier();
        }
        ti->location = SourceRange(startToken.location.start, previous().location.end());
        return ti;
    }

    List<ParameterDecl *> Parser::parseParameterList()
    {
        expect(TokenKind::LeftParen, "Expected '(' for parameter list");

        std::vector<ParameterDecl *> params;
        while (!check(TokenKind::RightParen) && !tokens.at_end())
        {
            auto startToken = tokens.current();
            auto param = arena.make<ParameterDecl>();
            param->param = parseTypedIdentifier();
            if (!param->param)
            {
                auto ti = arena.make<TypedIdentifier>();
                ti->type = errorExpr("Expected parameter type");
                ti->name = arena.makeIdentifier("");
                param->param = ti;
            }

            if (consume(TokenKind::Assign))
            {
                param->defaultValue = parseExpression();
            }
            else
            {
                param->defaultValue = nullptr;
            }
            param->location = SourceRange(startToken.location.start, previous().location.end());
            params.push_back(param);
            if (!consume(TokenKind::Comma))
                break;
        }

        expect(TokenKind::RightParen, "Expected ')' after parameters");
        return arena.makeList(params);
    }

    List<TypeParameterDecl *> Parser::parseTypeParameterList()
    {
        std::vector<TypeParameterDecl *> typeParams;

        expect(TokenKind::Less, "Expected '<' to start type parameter list");

        while (!check(TokenKind::Greater) && !tokens.at_end())
        {
            auto startToken = tokens.current();
            auto typeParam = arena.make<TypeParameterDecl>();
            typeParam->name = parseIdentifier();
            typeParam->location = SourceRange(startToken.location.start, previous().location.end());
            typeParams.push_back(typeParam);

            if (!consume(TokenKind::Comma))
                break;
        }

        expect(TokenKind::Greater, "Expected '>' to close type parameter list");
        return arena.makeList(typeParams);
    }

    List<Expression *> Parser::parseBaseTypeList()
    {
        std::vector<Expression *> types;
        do
        {
            if (auto type = parseTypeExpression())
            {
                types.push_back(type);
            }
        } while (consume(TokenKind::Comma));
        return arena.makeList(types);
    }

    bool Parser::isExpressionTerminator()
    {
        return checkAny({TokenKind::Semicolon, TokenKind::RightParen,
                         TokenKind::RightBracket, TokenKind::RightBrace,
                         TokenKind::Comma, TokenKind::Colon, TokenKind::Assign,
                         TokenKind::FatArrow, TokenKind::EndOfFile});
    }

    bool Parser::isPatternTerminator()
    {
        return checkAny({TokenKind::FatArrow, TokenKind::Comma,
                         TokenKind::RightParen, TokenKind::RightBrace});
    }

    bool Parser::isOnSameLine(const Token &prev, const Token &curr) const
    {
        // Check if two tokens are on the same line
        // Assuming SourceLocation has a line field or similar
        return prev.location.end().line == curr.location.start.line;
    }

    bool Parser::requireSemicolonIfSameLine()
    {
// This replaces the HANDLE_SEMI macro logic for line-aware semicolon handling
#ifdef REQUIRE_SEMI
        return expect(TokenKind::Semicolon, "Expected ';' to end statement");
#else
        // Check if we have a semicolon
        if (consume(TokenKind::Semicolon))
        {
            return true; // Semicolon was present and consumed
        }

        // No semicolon found - check if next token is on same line
        if (!tokens.at_end())
        {
            Token prev = tokens.previous();
            Token curr = tokens.current();

            // If the next token is on the same line as the end of the previous statement,
            // we require a semicolon
            if (isOnSameLine(prev, curr))
            {
                error("Expected ';' between statements on the same line");
                return false;
            }
        }
        return true; // No semicolon needed - statements on different lines
#endif
    }

} // namespace Bryo