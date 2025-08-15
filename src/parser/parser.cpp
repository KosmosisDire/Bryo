#include "parser/parser.hpp"

namespace Myre {

Parser::Parser(TokenStream& tokens) : tokens(tokens) {
    contextStack.push_back(Context::TOP_LEVEL);
}

Parser::~Parser() = default;

// ================== Public API ==================

CompilationUnit* Parser::parse() {
    auto* unit = arena.make<CompilationUnit>();
    if (tokens.at_end()) {
        unit->location = {{0, 1, 1}, 0};
        unit->topLevelStatements = arena.emptyList<Statement*>();
        return unit;
    }

    auto startToken = tokens.current();
    std::vector<Statement*> statements;

    while (!tokens.at_end()) {
        auto* stmt = parseTopLevelStatement();
        if (stmt) {
            statements.push_back(stmt);
        }
    }

    unit->topLevelStatements = arena.makeList(statements);
    // Use previous token unless the file was empty to begin with.
    unit->location = SourceRange(startToken.location.start, tokens.previous().location.end());
    return unit;
}

const std::vector<Parser::ParseError>& Parser::getErrors() const {
    return errors;
}

bool Parser::hasErrors() const {
    return !errors.empty();
}

// ================== Error Handling ==================

void Parser::error(const std::string& msg)
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

void Parser::warning(const std::string& msg) {
    errors.push_back({msg, tokens.current().location, ParseError::WARNING});
}

ErrorExpression* Parser::errorExpr(const std::string& msg) {
    error(msg);
    auto* err = arena.makeErrorExpr(msg);
    err->location = tokens.previous().location;
    return err;
}

ErrorStatement* Parser::errorStmt(const std::string& msg) {
    error(msg);
    auto* err = arena.makeErrorStmt(msg);
    err->location = tokens.previous().location;
    return err;
}

ErrorTypeRef* Parser::errorType(const std::string& msg) {
    error(msg);
    auto* err = arena.makeErrorType(msg);
    err->location = tokens.previous().location;
    return err;
}

void Parser::synchronize() {
    while (!tokens.at_end())
    {
        tokens.advance();

        if (consume(TokenKind::Semicolon)) {
            return;
        }

        if (tokens.current().starts_declaration() || tokens.current().starts_statement()) {
            return;
        }
    }
}

// ================== Context Management ==================

bool Parser::inLoop() const {
    for (auto it = contextStack.rbegin(); it != contextStack.rend(); ++it) {
        if (*it == Context::LOOP) return true;
    }
    return false;
}

bool Parser::inFunction() const {
    for (auto it = contextStack.rbegin(); it != contextStack.rend(); ++it) {
        if (*it == Context::FUNCTION) return true;
    }
    return false;
}

bool Parser::inGetter() const {
    for (auto it = contextStack.rbegin(); it != contextStack.rend(); ++it) {
        if (*it == Context::PROPERTY_GETTER) return true;
    }
    return false;
}

bool Parser::inSetter() const {
    for (auto it = contextStack.rbegin(); it != contextStack.rend(); ++it) {
        if (*it == Context::PROPERTY_SETTER) return true;
    }
    return false;
}

bool Parser::inTypeBody() const
{
    if (contextStack.back() == Context::TYPE_BODY) return true;
    return false;
}

// ================== Utility Helpers ==================

bool Parser::check(TokenKind kind) {
    return tokens.check(kind);
}

bool Parser::checkAny(std::initializer_list<TokenKind> kinds) {
    return tokens.check_any(kinds);
}

bool Parser::consume(TokenKind kind) {
    return tokens.consume(kind);
}

bool Parser::expect(TokenKind kind, const std::string& msg) {
    if (!consume(kind)) {
        error(msg);
        return false;
    }
    return true;
}

Token Parser::previous() {
    return tokens.previous();
}

TokenKind Parser::peekNext() {
    auto checkpoint = tokens.checkpoint();
    tokens.advance();
    TokenKind next = tokens.current().kind;
    tokens.restore(checkpoint);
    return next;
}

// ================== Top Level Parsing ==================

Statement* Parser::parseTopLevelStatement() {
    if (check(TokenKind::Using)) {
        return parseUsingDirective();
    }
    if (check(TokenKind::Namespace)) {
        return parseNamespaceDecl(tokens.current());
    }
    if (tokens.current().is_modifier() || checkDeclarationStart()) {
        return parseDeclaration();
    }
    if (tokens.current().starts_expression()) {
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
    if (check(TokenKind::New)) {
        return inTypeBody();
    }
    
    return checkAny({TokenKind::Type, TokenKind::Enum, TokenKind::Fn,
                     TokenKind::Var, TokenKind::Ref, TokenKind::Namespace});
}

Declaration* Parser::parseDeclaration() {
    auto startToken = tokens.current();
    ModifierKindFlags modifiers = parseModifiers();

    if (check(TokenKind::Namespace)) {
        auto* ns = parseNamespaceDecl(startToken);
        ns->modifiers = modifiers;
        return ns;
    }
    if (checkAny({TokenKind::Type, TokenKind::Ref, TokenKind::Enum, TokenKind::Static})) {
        return parseTypeDecl(modifiers, startToken);
    }
    if (check(TokenKind::Fn)) {
        return parseFunctionDecl(modifiers, startToken);
    }
    if (check(TokenKind::New)) {
        return parseConstructorDecl(modifiers, startToken);
    }
    if (check(TokenKind::Inherit)) {
        return parseInheritFunctionDecl(modifiers, startToken);
    }
    if (check(TokenKind::Var)) {
        return parseVarDeclaration(modifiers, startToken);
    }

    auto checkpoint = tokens.checkpoint();
    auto* type = parseTypeRef();
    if (type && check(TokenKind::Identifier)) {
        return parseTypedMemberDeclaration(modifiers, type, startToken);
    }
    
    tokens.restore(checkpoint);
    error("Expected declaration");
    return nullptr;
}

ModifierKindFlags Parser::parseModifiers() {
    ModifierKindFlags mods = ModifierKindFlags::None;
    while (tokens.current().is_modifier()) {
        mods |= tokens.current().to_modifier_kind();
        tokens.advance();
    }
    return mods;
}

TypeDecl* Parser::parseTypeDecl(ModifierKindFlags modifiers, const Token& startToken) {
    auto* decl = arena.make<TypeDecl>();
    decl->modifiers = modifiers;

    if (consume(TokenKind::Static)) {
        expect(TokenKind::Type, "Expected 'type' after 'static'");
        decl->kind = TypeDecl::Kind::StaticType;
    } else if (consume(TokenKind::Ref)) {
        expect(TokenKind::Type, "Expected 'type' after 'ref'");
        decl->kind = TypeDecl::Kind::RefType;
    } else if (consume(TokenKind::Enum)) {
        decl->kind = TypeDecl::Kind::Enum;
    } else if (consume(TokenKind::Type)) {
        decl->kind = TypeDecl::Kind::Type;
    } else {
        error("Expected type declaration keyword");
        decl->name = arena.makeIdentifier("");
        decl->location = startToken.location;
        return decl;
    }

    if (check(TokenKind::Identifier)) {
        decl->name = parseIdentifier();
    } else {
        error("Expected type name");
        decl->name = arena.makeIdentifier("");
    }

    decl->genericParams = parseGenericParams();

    if (consume(TokenKind::Colon)) {
        decl->baseTypes = parseBaseTypeList();
    } else {
        decl->baseTypes = arena.emptyList<TypeRef*>();
    }

    parseWhereConstraints(decl->genericParams);

    expect(TokenKind::LeftBrace, "Expected '{' after type declaration");
    
    std::vector<Declaration*> members;
    withContext(Context::TYPE_BODY, [&]() {
        while (!check(TokenKind::RightBrace) && !tokens.at_end()) {
            if (decl->kind == TypeDecl::Kind::Enum) {
                if (tokens.current().is_modifier() || check(TokenKind::Fn)) {
                    if (auto* member = parseDeclaration()) members.push_back(member);
                } else if (check(TokenKind::Identifier)) {
                    if (auto* enumCase = parseEnumCase()) members.push_back(enumCase);
                } else {
                    error("Unexpected token in enum body");
                    tokens.advance();
                }
            } else {
                if (auto* member = parseDeclaration()) members.push_back(member);
            }
            consume(TokenKind::Comma);
            consume(TokenKind::Semicolon);
        }
    });

    decl->members = arena.makeList(members);
    expect(TokenKind::RightBrace, "Expected '}' to close type declaration");

    decl->location = SourceRange(startToken.location.start, previous().location.end());
    return decl;
}

EnumCaseDecl* Parser::parseEnumCase() {
    auto startToken = tokens.current();
    auto* decl = arena.make<EnumCaseDecl>();
    decl->name = parseIdentifier();
    
    if (check(TokenKind::LeftParen)) {
        decl->associatedData = parseParameterList();
    } else {
        decl->associatedData = arena.emptyList<ParameterDecl*>();
    }
    
    decl->location = SourceRange(startToken.location.start, previous().location.end());
    return decl;
}

FunctionDecl* Parser::parseFunctionDecl(ModifierKindFlags modifiers, const Token& startToken) {
    auto* decl = arena.make<FunctionDecl>();
    decl->modifiers = modifiers;

    consume(TokenKind::Fn);
    decl->name = parseIdentifier();
    decl->genericParams = parseGenericParams();
    decl->parameters = parseParameterList();

    if (consume(TokenKind::Colon)) {
        decl->returnType = parseTypeRef();
        if (!decl->returnType) {
            decl->returnType = errorType("Expected return type");
        }
    } else {
        decl->returnType = nullptr; // void
    }

    parseWhereConstraints(decl->genericParams);

    if (check(TokenKind::LeftBrace)) {
        decl->body = withContext(Context::FUNCTION, [this]() { return parseBlock(); });
    } else if (consume(TokenKind::Semicolon)) {
        decl->body = nullptr; // abstract or interface
    } else {
        error("Expected '{' or ';' after function declaration");
        decl->body = nullptr;
    }

    decl->location = SourceRange(startToken.location.start, previous().location.end());
    return decl;
}

ConstructorDecl* Parser::parseConstructorDecl(ModifierKindFlags modifiers, const Token& startToken) {
    auto* decl = arena.make<ConstructorDecl>();
    decl->modifiers = modifiers;

    consume(TokenKind::New);
    decl->parameters = parseParameterList();

    expect(TokenKind::LeftBrace, "Expected '{' after constructor parameters");
    decl->body = withContext(Context::FUNCTION, [this]() { return parseBlock(); });

    if (!decl->body) {
        auto* block = arena.make<Block>();
        block->location = previous().location;
        block->statements = arena.emptyList<Statement*>();
        decl->body = block;
    }

    decl->location = SourceRange(startToken.location.start, previous().location.end());
    return decl;
}

InheritFunctionDecl* Parser::parseInheritFunctionDecl(ModifierKindFlags modifiers, const Token& startToken) {
    auto* decl = arena.make<InheritFunctionDecl>();
    decl->modifiers = modifiers;

    consume(TokenKind::Inherit);
    expect(TokenKind::Fn, "Expected 'fn' after 'inherit'");
    decl->functionName = parseIdentifier();

    if (check(TokenKind::LeftParen)) {
        tokens.advance();
        std::vector<TypeRef*> paramTypes;
        while (!check(TokenKind::RightParen) && !tokens.at_end()) {
            if (auto* type = parseTypeRef()) {
                paramTypes.push_back(type);
            }
            if (!consume(TokenKind::Comma)) break;
        }
        expect(TokenKind::RightParen, "Expected ')' after parameter types");
        decl->parameterTypes = arena.makeList(paramTypes);
    } else {
        decl->parameterTypes = arena.emptyList<TypeRef*>();
    }

    expect(TokenKind::Semicolon, "Expected ';' after inherit declaration");
    decl->location = SourceRange(startToken.location.start, previous().location.end());
    return decl;
}

Declaration* Parser::parseVarDeclaration(ModifierKindFlags modifiers, const Token& startToken) {
    auto checkpoint = tokens.checkpoint();
    consume(TokenKind::Var);
    
    if (!check(TokenKind::Identifier)) {
        error("Expected identifier after 'var'");
        tokens.restore(checkpoint);
        return nullptr;
    }
    auto* name = parseIdentifier();

    if (checkAny({TokenKind::FatArrow, TokenKind::LeftBrace})) {
        tokens.restore(checkpoint);
        return parseMemberVariableDecl(modifiers, nullptr, true, startToken);
    }
    if (check(TokenKind::Assign)) {
        auto initCheckpoint = tokens.checkpoint();
        tokens.advance();
        parseExpression();
        if (check(TokenKind::LeftBrace)) {
            tokens.restore(checkpoint);
            return parseMemberVariableDecl(modifiers, nullptr, true, startToken);
        }
        tokens.restore(initCheckpoint);
    }

    if (contextStack.back() == Context::TYPE_BODY) {
        tokens.restore(checkpoint);
        return parseMemberVariableDecl(modifiers, nullptr, true, startToken);
    } else {
        auto* decl = arena.make<VariableDecl>();
        decl->modifiers = modifiers;
        
        auto* ti = arena.make<TypedIdentifier>();
        ti->type = nullptr;
        ti->name = name;
        ti->location = SourceRange(startToken.location.start, name->location.end());
        decl->variable = ti;

        if (consume(TokenKind::Assign)) {
            decl->initializer = parseExpression();
            if (!decl->initializer) {
                decl->initializer = errorExpr("Expected initializer");
            }
        } else {
            decl->initializer = nullptr;
        }

        expect(TokenKind::Semicolon, "Expected ';' after variable declaration");
        decl->location = SourceRange(startToken.location.start, previous().location.end());
        return decl;
    }
}

Declaration* Parser::parseTypedMemberDeclaration(ModifierKindFlags modifiers, TypeRef* type, const Token& startToken) {
    std::vector<Declaration*> declarations;
    
    do {
        auto fieldStartToken = tokens.current();
        auto* name = parseIdentifier();
        Expression* initializer = nullptr;

        if (consume(TokenKind::Assign)) {
            initializer = parseExpression();
        }

        if (checkAny({TokenKind::FatArrow, TokenKind::LeftBrace})) {
            auto* prop = arena.make<MemberVariableDecl>();
            prop->modifiers = modifiers;
            prop->type = type;
            prop->name = name;
            prop->initializer = initializer;

            if (consume(TokenKind::FatArrow)) {
                auto* getter = arena.make<PropertyAccessor>();
                getter->kind = PropertyAccessor::Kind::Get;
                getter->body = parseExpression();
                prop->getter = getter;
                prop->setter = nullptr;
                expect(TokenKind::Semicolon, "Expected ';' after arrow property");
            } else if (check(TokenKind::LeftBrace)) {
                parsePropertyAccessors(prop);
            }
            prop->location = SourceRange(fieldStartToken.location.start, previous().location.end());
            declarations.push_back(prop);
        } else {
            auto* field = arena.make<MemberVariableDecl>();
            field->modifiers = modifiers;
            field->type = type;
            field->name = name;
            field->initializer = initializer;
            field->getter = nullptr;
            field->setter = nullptr;
            field->location = SourceRange(fieldStartToken.location.start, previous().location.end());
            declarations.push_back(field);
        }
    } while (consume(TokenKind::Comma));
    
    expect(TokenKind::Semicolon, "Expected ';' after field declaration");
    // The location for the entire statement (if it had multiple fields) is not represented by a single node
    // as this function only returns the first declaration.
    if (!declarations.empty()) {
        declarations[0]->location = SourceRange(startToken.location.start, previous().location.end());
    }
    return declarations.empty() ? nullptr : declarations[0];
}

MemberVariableDecl* Parser::parseMemberVariableDecl(ModifierKindFlags modifiers, TypeRef* type, bool isVar, const Token& startToken) {
    auto* decl = arena.make<MemberVariableDecl>();
    decl->modifiers = modifiers;
    
    if (isVar) {
        consume(TokenKind::Var);
        decl->type = nullptr;
    } else {
        decl->type = type;
    }
    decl->name = parseIdentifier();

    if (consume(TokenKind::Assign)) {
        decl->initializer = parseExpression();
    } else {
        decl->initializer = nullptr;
    }

    if (consume(TokenKind::FatArrow)) {
        auto* getter = arena.make<PropertyAccessor>();
        auto getStart = previous();
        getter->kind = PropertyAccessor::Kind::Get;
        getter->body = parseExpression();
        getter->location = SourceRange(getStart.location.start, previous().location.end());
        decl->getter = getter;
        decl->setter = nullptr;
        expect(TokenKind::Semicolon, "Expected ';' after arrow property");
    } else if (check(TokenKind::LeftBrace)) {
        parsePropertyAccessors(decl);
    } else {
        decl->getter = nullptr;
        decl->setter = nullptr;
        expect(TokenKind::Semicolon, "Expected ';' after field declaration");
    }
    decl->location = SourceRange(startToken.location.start, previous().location.end());
    return decl;
}

void Parser::parsePropertyAccessors(MemberVariableDecl* prop) {
    consume(TokenKind::LeftBrace);
    while (!check(TokenKind::RightBrace) && !tokens.at_end()) {
        auto accessorStartToken = tokens.current();
        ModifierKindFlags accessorMods = parseModifiers();
        
        if (consume(TokenKind::Get)) {
            auto* getter = arena.make<PropertyAccessor>();
            getter->kind = PropertyAccessor::Kind::Get;
            getter->modifiers = accessorMods;
            
            if (consume(TokenKind::FatArrow)) {
                if (check(TokenKind::LeftBrace)) {
                    error("Unexpected '{' after '=>' in property getter. Use either '=> expression' or '{ statements }'");
                    getter->body = withContext(Context::PROPERTY_GETTER, [this]() { return parseBlock(); });
                } else {
                    getter->body = withContext(Context::PROPERTY_GETTER, [this]() { return parseExpression(); });
                }
            } else if (check(TokenKind::LeftBrace)) {
                getter->body = withContext(Context::PROPERTY_GETTER, [this]() { return parseBlock(); });
            } else {
                getter->body = std::monostate{};
            }
            getter->location = SourceRange(accessorStartToken.location.start, previous().location.end());
            prop->getter = getter;
        } else if (consume(TokenKind::Set)) {
            auto* setter = arena.make<PropertyAccessor>();
            setter->kind = PropertyAccessor::Kind::Set;
            setter->modifiers = accessorMods;
            
            if (consume(TokenKind::FatArrow)) {
                if (check(TokenKind::LeftBrace)) {
                    error("Unexpected '{' after '=>' in property setter. Use either '=> expression' or '{ statements }'");
                    setter->body = withContext(Context::PROPERTY_SETTER, [this]() { return parseBlock(); });
                } else {
                    setter->body = withContext(Context::PROPERTY_SETTER, [this]() { return parseExpression(); });
                }
            } else if (check(TokenKind::LeftBrace)) {
                setter->body = withContext(Context::PROPERTY_SETTER, [this]() { return parseBlock(); });
            } else {
                setter->body = std::monostate{};
            }
            setter->location = SourceRange(accessorStartToken.location.start, previous().location.end());
            prop->setter = setter;
        } else {
            error("Expected 'get' or 'set' in property accessor");
            while (!tokens.at_end() && 
                   !check(TokenKind::Semicolon) && 
                   !check(TokenKind::RightBrace) &&
                   !check(TokenKind::Get) &&
                   !check(TokenKind::Set)) {
                tokens.advance();
            }
        }
        consume(TokenKind::Semicolon);
    }
    expect(TokenKind::RightBrace, "Expected '}' after property accessors");
}

NamespaceDecl* Parser::parseNamespaceDecl(const Token& startToken) {
    auto* decl = arena.make<NamespaceDecl>();
    consume(TokenKind::Namespace);
    
    std::vector<Identifier*> path;
    do {
        path.push_back(parseIdentifier());
    } while (consume(TokenKind::Dot));
    decl->path = arena.makeList(path);

    if (consume(TokenKind::Semicolon)) {
        decl->isFileScoped = true;
        decl->body = std::nullopt;
    } else if (consume(TokenKind::LeftBrace)) {
        decl->isFileScoped = false;
        std::vector<Statement*> statements;
        withContext(Context::NAMESPACE, [&]() {
            while (!check(TokenKind::RightBrace) && !tokens.at_end()) {
                if (auto* stmt = parseTopLevelStatement()) statements.push_back(stmt);
            }
        });
        decl->body = arena.makeList(statements);
        expect(TokenKind::RightBrace, "Expected '}' after namespace body");
    } else {
        error("Expected ';' or '{' after namespace declaration");
        decl->isFileScoped = true;
        decl->body = std::nullopt;
    }
    decl->location = SourceRange(startToken.location.start, previous().location.end());
    return decl;
}

// ================== Statements ==================

Statement* Parser::parseStatement() {
    auto startToken = tokens.current();
    if (check(TokenKind::If)) return parseIfStatement();
    if (check(TokenKind::While)) return parseWhileStatement();
    if (check(TokenKind::For)) return parseForStatement();
    if (check(TokenKind::Return)) return parseReturnStatement();
    if (check(TokenKind::Break)) return parseBreakStatement();
    if (check(TokenKind::Continue)) return parseContinueStatement();
    if (check(TokenKind::LeftBrace)) return parseBlock();
    if (tokens.current().is_modifier() || checkDeclarationStart()) {
        return parseDeclaration();
    }
    
    if (check(TokenKind::Identifier)) {
        auto checkpoint = tokens.checkpoint();
        auto* type = parseTypeRef();
        if (type && check(TokenKind::Identifier)) {
            tokens.restore(checkpoint);
            return parseDeclaration();
        }
        tokens.restore(checkpoint);
    }
    
    return parseExpressionStatement();
}

Block* Parser::parseBlock() {
    auto startToken = tokens.current();
    auto* block = arena.make<Block>();
    consume(TokenKind::LeftBrace);
    
    std::vector<Statement*> statements;
    while (!check(TokenKind::RightBrace) && !tokens.at_end()) {
        if (auto* stmt = parseStatement()) {
            statements.push_back(stmt);
        }
    }
    block->statements = arena.makeList(statements);
    
    expect(TokenKind::RightBrace, "Expected '}' to close block");
    block->location = SourceRange(startToken.location.start, previous().location.end());
    return block;
}

Statement* Parser::parseIfStatement() {
    auto startToken = tokens.current();
    consume(TokenKind::If);
    expect(TokenKind::LeftParen, "Expected '(' after 'if'");

    auto* condition = parseExpression();
    if (!condition) condition = errorExpr("Expected condition");

    expect(TokenKind::RightParen, "Expected ')' after condition");

    auto* thenStmt = parseStatement();
    if (!thenStmt) thenStmt = errorStmt("Expected then statement");

    Statement* elseStmt = nullptr;
    if (consume(TokenKind::Else)) {
        elseStmt = parseStatement();
        if (!elseStmt) elseStmt = errorStmt("Expected else statement");
    }

    auto* ifExpr = arena.make<IfExpr>();
    ifExpr->condition = condition;
    ifExpr->thenBranch = thenStmt;
    ifExpr->elseBranch = elseStmt;
    ifExpr->location = SourceRange(startToken.location.start, previous().location.end());

    auto* exprStmt = arena.make<ExpressionStmt>();
    exprStmt->expression = ifExpr;
    exprStmt->location = ifExpr->location; // The statement wraps the expression exactly.
    return exprStmt;
}

WhileStmt* Parser::parseWhileStatement() {
    auto startToken = tokens.current();
    auto* stmt = arena.make<WhileStmt>();
    consume(TokenKind::While);
    expect(TokenKind::LeftParen, "Expected '(' after 'while'");

    stmt->condition = parseExpression();
    if (!stmt->condition) stmt->condition = errorExpr("Expected condition");

    expect(TokenKind::RightParen, "Expected ')' after condition");

    stmt->body = withContext(Context::LOOP, [this]() { return parseStatement(); });
    if (!stmt->body) stmt->body = errorStmt("Expected loop body");

    stmt->location = SourceRange(startToken.location.start, previous().location.end());
    return stmt;
}

Statement* Parser::parseForStatement() {
    auto checkpoint = tokens.checkpoint();
    consume(TokenKind::For);
    consume(TokenKind::LeftParen);
    
    bool isForIn = false;
    int parenDepth = 1;
    while (!tokens.at_end() && parenDepth > 0) {
        if (check(TokenKind::LeftParen)) parenDepth++;
        else if (check(TokenKind::RightParen)) parenDepth--;
        else if (check(TokenKind::In) && parenDepth == 1) {
            isForIn = true;
            break;
        }
        tokens.advance();
    }
    
    tokens.restore(checkpoint);
    
    if (isForIn) {
        return parseForInStatement();
    } else {
        return parseTraditionalForStatement();
    }
}

ForStmt* Parser::parseTraditionalForStatement() {
    auto startToken = tokens.current();
    auto* stmt = arena.make<ForStmt>();
    consume(TokenKind::For);
    expect(TokenKind::LeftParen, "Expected '(' after 'for'");

    if (!check(TokenKind::Semicolon)) {
        auto checkpoint = tokens.checkpoint();
        auto* type = parseTypeRef();
        if (type && check(TokenKind::Identifier)) {
            tokens.restore(checkpoint);
            stmt->initializer = parseDeclaration();
        } else {
            tokens.restore(checkpoint);
            if (checkDeclarationStart() || tokens.current().is_modifier()) {
                stmt->initializer = parseDeclaration();
            } else {
                stmt->initializer = parseExpressionStatement();
            }
        }
    }

    if (!check(TokenKind::Semicolon)) {
        stmt->condition = parseExpression();
    } else {
        stmt->condition = nullptr;
    }
    expect(TokenKind::Semicolon, "Expected ';' after for condition");

    std::vector<Expression*> updates;
    while (!check(TokenKind::RightParen) && !tokens.at_end()) {
        if (auto* update = parseExpression()) {
            updates.push_back(update);
        }
        if (!consume(TokenKind::Comma)) break;
    }
    stmt->updates = arena.makeList(updates);
    
    expect(TokenKind::RightParen, "Expected ')' after for clauses");
    
    stmt->body = withContext(Context::LOOP, [this]() { return parseStatement(); });
    if (!stmt->body) stmt->body = errorStmt("Expected loop body");

    stmt->location = SourceRange(startToken.location.start, previous().location.end());
    return stmt;
}

ForInStmt* Parser::parseForInStatement() {
    auto startToken = tokens.current();
    auto* stmt = arena.make<ForInStmt>();
    consume(TokenKind::For);
    expect(TokenKind::LeftParen, "Expected '(' after 'for'");

    stmt->iterator = parseTypedIdentifier();
    if (!stmt->iterator) {
        auto* ti = arena.make<TypedIdentifier>();
        ti->type = errorType("Expected type");
        ti->name = arena.makeIdentifier("");
        stmt->iterator = ti;
    }

    expect(TokenKind::In, "Expected 'in' in for-in loop");

    stmt->iterable = parseExpression();
    if (!stmt->iterable) stmt->iterable = errorExpr("Expected iterable expression");

    if (consume(TokenKind::At)) {
        stmt->indexVar = parseTypedIdentifier();
    } else {
        stmt->indexVar = nullptr;
    }

    expect(TokenKind::RightParen, "Expected ')' after for-in clauses");

    stmt->body = withContext(Context::LOOP, [this]() { return parseStatement(); });
    if (!stmt->body) stmt->body = errorStmt("Expected loop body");

    stmt->location = SourceRange(startToken.location.start, previous().location.end());
    return stmt;
}

ReturnStmt* Parser::parseReturnStatement() {
    auto startToken = tokens.current();
    auto* stmt = arena.make<ReturnStmt>();
    consume(TokenKind::Return);

    if (!check(TokenKind::Semicolon)) {
        stmt->value = parseExpression();
    } else {
        stmt->value = nullptr;
    }

    expect(TokenKind::Semicolon, "Expected ';' after return statement");
    if (!inFunction() && !inGetter() && !inSetter()) {
        warning("Return statement outside function or property");
    }
    stmt->location = SourceRange(startToken.location.start, previous().location.end());
    return stmt;
}

BreakStmt* Parser::parseBreakStatement() {
    auto startToken = tokens.current();
    auto* stmt = arena.make<BreakStmt>();
    consume(TokenKind::Break);
    expect(TokenKind::Semicolon, "Expected ';' after break");
    if (!inLoop()) {
        warning("Break statement outside loop");
    }
    stmt->location = SourceRange(startToken.location.start, previous().location.end());
    return stmt;
}

ContinueStmt* Parser::parseContinueStatement() {
    auto startToken = tokens.current();
    auto* stmt = arena.make<ContinueStmt>();
    consume(TokenKind::Continue);
    expect(TokenKind::Semicolon, "Expected ';' after continue");
    if (!inLoop()) {
        warning("Continue statement outside loop");
    }
    stmt->location = SourceRange(startToken.location.start, previous().location.end());
    return stmt;
}

ExpressionStmt* Parser::parseExpressionStatement() {
    auto* stmt = arena.make<ExpressionStmt>();
    stmt->expression = parseExpression();
    if (!stmt->expression) {
        stmt->expression = errorExpr("Expected expression");
    }
    expect(TokenKind::Semicolon, "Expected ';' after expression");
    stmt->location = SourceRange(stmt->expression->location.start, previous().location.end());
    return stmt;
}

UsingDirective* Parser::parseUsingDirective() {
    auto startToken = tokens.current();
    auto* directive = arena.make<UsingDirective>();
    consume(TokenKind::Using);

    auto checkpoint = tokens.checkpoint();
    if (check(TokenKind::Identifier)) {
        auto* firstId = parseIdentifier();
        if (consume(TokenKind::Assign)) {
            directive->kind = UsingDirective::Kind::Alias;
            directive->alias = firstId;
            directive->aliasedType = parseTypeRef();
            if (!directive->aliasedType) {
                directive->aliasedType = errorType("Expected type after '='");
            }
            directive->path = arena.emptyList<Identifier*>();
        } else {
            tokens.restore(checkpoint);
            directive->kind = UsingDirective::Kind::Namespace;
            std::vector<Identifier*> path;
            do {
                path.push_back(parseIdentifier());
            } while (consume(TokenKind::Dot));
            directive->path = arena.makeList(path);
            directive->alias = nullptr;
            directive->aliasedType = nullptr;
        }
    }

    expect(TokenKind::Semicolon, "Expected ';' after using directive");
    directive->location = SourceRange(startToken.location.start, previous().location.end());
    return directive;
}

// ================== Expressions ==================

Expression* Parser::parseExpression(int minPrecedence) {
    auto* left = parsePrimaryExpression();
    if (!left) {
        return nullptr;
    }
    return parseBinaryExpression(left, minPrecedence);
}

Expression* Parser::parseBinaryExpression(Expression* left, int minPrecedence) {
    while (!tokens.at_end()) {
        Token op = tokens.current();
        int precedence = op.get_binary_precedence();

        if (op.kind == TokenKind::Question)
        {
            if (minPrecedence > precedence) break;
            
            tokens.advance();
            
            auto* conditional = arena.make<ConditionalExpr>();
            conditional->condition = left;
            
            conditional->thenExpr = parseExpression(precedence + 1);
            if (!conditional->thenExpr) conditional->thenExpr = errorExpr("Expected expression after '?'");
            
            expect(TokenKind::Colon, "Expected ':' in conditional expression");
            
            conditional->elseExpr = parseExpression(precedence);
            if (!conditional->elseExpr) conditional->elseExpr = errorExpr("Expected expression after ':'");
            
            conditional->location = SourceRange(left->location.start, conditional->elseExpr->location.end());
            return conditional;
        }

        if (precedence < minPrecedence || precedence == 0) break;

        if (op.is_assignment_operator()) {
            tokens.advance();
            auto* assign = arena.make<AssignmentExpr>();
            assign->target = left;
            assign->op = op.to_assignment_operator_kind();
            assign->value = parseExpression(precedence);
            if (!assign->value) assign->value = errorExpr("Expected value in assignment");
            
            assign->location = SourceRange(left->location.start, assign->value->location.end());
            return assign;
        }

        if (op.is_any({TokenKind::DotDot, TokenKind::DotDotEquals})) {
            return parseRangeExpression(left);
        }

        tokens.advance();
        int nextPrecedence = op.is_right_associative() ? precedence : precedence + 1;

        auto* right = parseExpression(nextPrecedence);
        if (!right) right = errorExpr("Expected right operand");

        auto* binary = arena.make<BinaryExpr>();
        binary->left = left;
        binary->op = op.to_binary_operator_kind();
        binary->right = right;
        binary->location = SourceRange(left->location.start, right->location.end());
        left = binary;
    }
    return left;
}

Expression* Parser::parsePrimaryExpression() {
    Expression* expr = nullptr;
    if (tokens.current().is_unary_operator()) {
        expr = parseUnaryExpression();
    } else if (checkAny({TokenKind::DotDot, TokenKind::DotDotEquals})) {
        expr = parseRangeExpression(nullptr);
    } else if (tokens.current().is_literal()) {
        expr = parseLiteral();
    } else if (check(TokenKind::Identifier)) {
        auto checkpoint = tokens.checkpoint();
        tokens.advance();
        if (check(TokenKind::FatArrow)) {
            tokens.restore(checkpoint);
            expr = parseLambdaExpression();
        } else {
            tokens.restore(checkpoint);
            expr = parseNameExpression();
        }
    } else if (check(TokenKind::This)) {
        auto startToken = tokens.current();
        auto* thisExpr = arena.make<ThisExpr>();
        tokens.advance();
        thisExpr->location = startToken.location;
        expr = thisExpr;
    } else if (check(TokenKind::LeftParen)) {
        expr = parseParenthesizedOrLambda();
    } else if (check(TokenKind::LeftBracket)) {
        expr = parseArrayLiteral();
    } else if (check(TokenKind::New)) {
        expr = parseNewExpression();
    } else if (check(TokenKind::Match)) {
        expr = parseMatchExpression();
    } else if (check(TokenKind::Typeof)) {
        expr = parseTypeOfExpression();
    } else if (check(TokenKind::Sizeof)) {
        expr = parseSizeOfExpression();
    }

    if (!expr) {
        return nullptr;
    }

    return parsePostfixExpression(expr);
}

Expression* Parser::parsePostfixExpression(Expression* expr) {
    while (true) {
        if (check(TokenKind::Less)) {
            auto checkpoint = tokens.checkpoint();
            tokens.advance();
            
            bool isGenericCall = false;
            std::vector<TypeRef*> genericArgs;
            int angleDepth = 1;
            
            while (!tokens.at_end() && angleDepth > 0) {
                auto typeCheckpoint = tokens.checkpoint();
                auto* type = parseTypeRef();
                if (!type) break;
                
                genericArgs.push_back(type);
                
                if (consume(TokenKind::Comma)) continue;
                else if (check(TokenKind::Greater)) {
                    angleDepth--;
                    tokens.advance();
                    if (angleDepth == 0 && check(TokenKind::LeftParen)) isGenericCall = true;
                    break;
                } else if (check(TokenKind::RightShift)) {
                    if (angleDepth >= 2) {
                        angleDepth -= 2;
                        tokens.advance();
                        if (angleDepth == 0 && check(TokenKind::LeftParen)) isGenericCall = true;
                    } else {
                        angleDepth = 0;
                    }
                    break;
                } else if (check(TokenKind::Less)) {
                    angleDepth++;
                    tokens.advance();
                } else break;
            }
            
            if (isGenericCall) {
                consume(TokenKind::LeftParen);
                auto* call = arena.make<CallExpr>();
                call->callee = expr;
                call->genericArgs = arena.makeList(genericArgs);
                
                std::vector<Expression*> args;
                while (!check(TokenKind::RightParen) && !tokens.at_end()) {
                    auto* arg = parseExpression();
                    if (!arg) break;
                    args.push_back(arg);
                    if (!consume(TokenKind::Comma)) break;
                    if (check(TokenKind::RightParen)) break;
                }
                call->arguments = arena.makeList(args);
                
                expect(TokenKind::RightParen, "Expected ')' after arguments");
                call->location = SourceRange(expr->location.start, previous().location.end());
                expr = call;
            } else {
                tokens.restore(checkpoint);
                break;
            }
        } else if (check(TokenKind::LeftParen)) {
            tokens.advance();
            auto* call = arena.make<CallExpr>();
            call->callee = expr;
            call->genericArgs = arena.emptyList<TypeRef*>();
            
            std::vector<Expression*> args;
            while (!check(TokenKind::RightParen) && !tokens.at_end()) {
                auto* arg = parseExpression();
                if (!arg) break;
                args.push_back(arg);
                if (!consume(TokenKind::Comma)) break;
                if (check(TokenKind::RightParen)) break;
            }
            call->arguments = arena.makeList(args);
            
            expect(TokenKind::RightParen, "Expected ')' after arguments");
            call->location = SourceRange(expr->location.start, previous().location.end());
            expr = call;
        } else if (check(TokenKind::Dot)) {
            tokens.advance();
            auto* member = arena.make<MemberAccessExpr>();
            member->object = expr;
            member->member = parseIdentifier();
            member->location = SourceRange(expr->location.start, member->member->location.end());
            expr = member;
        } else if (check(TokenKind::LeftBracket)) {
            tokens.advance();
            auto* indexer = arena.make<IndexerExpr>();
            indexer->object = expr;
            indexer->index = parseExpression();
            if (!indexer->index) indexer->index = errorExpr("Expected index expression");
            
            expect(TokenKind::RightBracket, "Expected ']' after index");
            indexer->location = SourceRange(expr->location.start, previous().location.end());
            expr = indexer;
        } else if (checkAny({TokenKind::Increment, TokenKind::Decrement})) {
            auto* unary = arena.make<UnaryExpr>();
            unary->operand = expr;
            unary->op = tokens.current().to_unary_operator_kind();
            unary->isPostfix = true;
            tokens.advance();
            unary->location = SourceRange(expr->location.start, previous().location.end());
            expr = unary;
        } else {
            break;
        }
    }
    return expr;
}

Expression* Parser::parseUnaryExpression() {
    auto startToken = tokens.current();
    auto* unary = arena.make<UnaryExpr>();
    Token op = tokens.current();
    tokens.advance();
    unary->op = op.to_unary_operator_kind();
    unary->isPostfix = false;
    unary->operand = parseExpression(op.get_unary_precedence());
    if (!unary->operand) {
        unary->operand = errorExpr("Expected operand after unary operator");
    }
    unary->location = SourceRange(startToken.location.start, unary->operand->location.end());
    return unary;
}

Expression* Parser::parseRangeExpression(Expression* start) {
    auto startLoc = start ? start->location.start : tokens.current().location.end();
    auto* range = arena.make<RangeExpr>();
    range->start = start;

    if (consume(TokenKind::DotDotEquals)) {
        range->isInclusive = true;
    } else if (consume(TokenKind::DotDot)) {
        range->isInclusive = false;
    }

    if (!isExpressionTerminator() && !check(TokenKind::By)) {
        range->end = parseExpression(120);
    } else {
        range->end = nullptr;
    }

    if (consume(TokenKind::By)) {
        range->step = parseExpression(120);
        if (!range->step) {
            range->step = errorExpr("Expected step expression after 'by'");
        }
    } else {
        range->step = nullptr;
    }
    range->location = SourceRange(startLoc, previous().location.end());
    return range;
}

Expression* Parser::parseLiteral() {
    auto* lit = arena.make<LiteralExpr>();
    Token tok = tokens.current();
    lit->location = tok.location;
    lit->value = tok.text;

    switch (tok.kind) {
        case TokenKind::LiteralI32: case TokenKind::LiteralI64: lit->kind = LiteralExpr::Kind::Integer; break;
        case TokenKind::LiteralF32: case TokenKind::LiteralF64: lit->kind = LiteralExpr::Kind::Float; break;
        case TokenKind::LiteralString: lit->kind = LiteralExpr::Kind::String; break;
        case TokenKind::LiteralChar: lit->kind = LiteralExpr::Kind::Char; break;
        case TokenKind::LiteralBool: lit->kind = LiteralExpr::Kind::Bool; break;
        case TokenKind::Null: lit->kind = LiteralExpr::Kind::Null; break;
        default: lit->kind = LiteralExpr::Kind::Integer; break;
    }
    tokens.advance();
    return lit;
}

Expression* Parser::parseNameExpression() {
    auto startToken = tokens.current();
    auto* name = arena.make<NameExpr>();
    std::vector<Identifier*> parts;
    do {
        parts.push_back(parseIdentifier());
    } while ((check(TokenKind::Dot) && peekNext() == TokenKind::Identifier && consume(TokenKind::Dot)));
    name->parts = arena.makeList(parts);
    name->location = SourceRange(startToken.location.start, previous().location.end());
    return name;
}

Expression* Parser::parseParenthesizedOrLambda() {
    auto checkpoint = tokens.checkpoint();
    consume(TokenKind::LeftParen);
    
    bool isLambda = false;
    int parenDepth = 1;
    
    while (!tokens.at_end() && parenDepth > 0) {
        if (check(TokenKind::LeftParen)) parenDepth++;
        else if (check(TokenKind::RightParen)) {
            parenDepth--;
            if (parenDepth == 0) {
                tokens.advance();
                if (check(TokenKind::FatArrow)) isLambda = true;
                break;
            }
        } else if (check(TokenKind::FatArrow) && parenDepth == 1) {
            isLambda = true;
            break;
        }
        tokens.advance();
    }
    
    tokens.restore(checkpoint);
    
    if (isLambda) {
        return parseLambdaExpression();
    } else {
        consume(TokenKind::LeftParen);
        if (check(TokenKind::RightParen)) {
            error("Empty parentheses are not a valid expression");
            consume(TokenKind::RightParen);
            return errorExpr("Expected expression in parentheses");
        }
        
        auto* expr = parseExpression();
        if (!expr) return errorExpr("Expected expression in parentheses");
        
        expect(TokenKind::RightParen, "Expected ')' after expression");
        return expr;
    }
}

Expression* Parser::parseArrayLiteral() {
    auto startToken = tokens.current();
    auto* array = arena.make<ArrayLiteralExpr>();
    consume(TokenKind::LeftBracket);
    
    std::vector<Expression*> elements;
    while (!check(TokenKind::RightBracket) && !tokens.at_end()) {
        if (auto* elem = parseExpression()) {
            elements.push_back(elem);
        }
        if (!consume(TokenKind::Comma)) break;
    }
    array->elements = arena.makeList(elements);
    
    expect(TokenKind::RightBracket, "Expected ']' after array elements");
    array->location = SourceRange(startToken.location.start, previous().location.end());
    return array;
}

Expression* Parser::parseNewExpression() {
    auto startToken = tokens.current();
    auto* newExpr = arena.make<NewExpr>();
    consume(TokenKind::New);

    newExpr->type = parseTypeRef();
    if (!newExpr->type) newExpr->type = errorType("Expected type after 'new'");

    if (check(TokenKind::LeftParen)) {
        consume(TokenKind::LeftParen);
        std::vector<Expression*> args;
        while (!check(TokenKind::RightParen) && !tokens.at_end()) {
            if (auto* arg = parseExpression()) args.push_back(arg);
            if (!consume(TokenKind::Comma)) break;
        }
        newExpr->arguments = arena.makeList(args);
        expect(TokenKind::RightParen, "Expected ')' after constructor arguments");
    } else {
        newExpr->arguments = arena.emptyList<Expression*>();
    }
    newExpr->location = SourceRange(startToken.location.start, previous().location.end());
    return newExpr;
}

Expression* Parser::parseLambdaExpression() {
    auto startToken = tokens.current();
    auto* lambda = arena.make<LambdaExpr>();
    
    if (check(TokenKind::LeftParen)) {
        consume(TokenKind::LeftParen);
        std::vector<ParameterDecl*> params;
        while (!check(TokenKind::RightParen) && !tokens.at_end()) {
            auto* param = arena.make<ParameterDecl>();
            auto paramStart = tokens.current();
            param->param = parseTypedIdentifier();
            if (!param->param) {
                auto* ti = arena.make<TypedIdentifier>();
                ti->type = errorType("Expected parameter type");
                ti->name = arena.makeIdentifier("");
                param->param = ti;
            }
            param->defaultValue = nullptr;
            param->location = SourceRange(paramStart.location.start, previous().location.end());
            params.push_back(param);
            if (!consume(TokenKind::Comma)) break;
        }
        lambda->parameters = arena.makeList(params);
        expect(TokenKind::RightParen, "Expected ')' after lambda parameters");
    } else {
        std::vector<ParameterDecl*> params;
        auto* param = arena.make<ParameterDecl>();
        auto paramStart = tokens.current();
        auto* ti = arena.make<TypedIdentifier>();
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
    
    if (check(TokenKind::LeftBrace)) {
        lambda->body = parseBlock();
    } else {
        auto* expr = parseExpression();
        if (!expr) expr = errorExpr("Expected lambda body");
        auto* exprStmt = arena.make<ExpressionStmt>();
        exprStmt->expression = expr;
        exprStmt->location = expr->location;
        lambda->body = exprStmt;
    }
    lambda->location = SourceRange(startToken.location.start, previous().location.end());
    return lambda;
}

Expression* Parser::parseMatchExpression() {
    auto startToken = tokens.current();
    auto* match = arena.make<MatchExpr>();
    consume(TokenKind::Match);
    expect(TokenKind::LeftParen, "Expected '(' after 'match'");

    match->subject = parseExpression();
    if (!match->subject) match->subject = errorExpr("Expected match subject");

    expect(TokenKind::RightParen, "Expected ')' after match subject");
    expect(TokenKind::LeftBrace, "Expected '{' after match");

    std::vector<MatchArm*> arms;
    while (!check(TokenKind::RightBrace) && !tokens.at_end()) {
        if (auto* arm = parseMatchArm()) {
            arms.push_back(arm);
        }
        consume(TokenKind::Comma);
    }
    match->arms = arena.makeList(arms);
    
    expect(TokenKind::RightBrace, "Expected '}' after match arms");
    match->location = SourceRange(startToken.location.start, previous().location.end());
    return match;
}

MatchArm* Parser::parseMatchArm() {
    auto startToken = tokens.current();
    auto* arm = arena.make<MatchArm>();
    arm->pattern = parsePattern();
    if (!arm->pattern) {
        auto* binding = arena.make<BindingPattern>();
        binding->name = nullptr;
        arm->pattern = binding;
        error("Expected pattern in match arm");
    }

    expect(TokenKind::FatArrow, "Expected '=>' after pattern");
    
    if (check(TokenKind::LeftBrace)) {
        arm->result = parseBlock();
    } else {
        auto* expr = parseExpression();
        if (!expr) expr = errorExpr("Expected match arm result");
        auto* exprStmt = arena.make<ExpressionStmt>();
        exprStmt->expression = expr;
        exprStmt->location = expr->location;
        arm->result = exprStmt;
    }
    
    if (!arm->result) arm->result = errorStmt("Expected match arm result");
    
    arm->location = SourceRange(startToken.location.start, previous().location.end());
    return arm;
}

Expression* Parser::parseTypeOfExpression() {
    auto startToken = tokens.current();
    auto* typeOf = arena.make<TypeOfExpr>();
    consume(TokenKind::Typeof);
    expect(TokenKind::LeftParen, "Expected '(' after 'typeof'");
    
    typeOf->type = parseTypeRef();
    if (!typeOf->type) typeOf->type = errorType("Expected type");
    
    expect(TokenKind::RightParen, "Expected ')' after type");
    typeOf->location = SourceRange(startToken.location.start, previous().location.end());
    return typeOf;
}

Expression* Parser::parseSizeOfExpression() {
    auto startToken = tokens.current();
    auto* sizeOf = arena.make<SizeOfExpr>();
    consume(TokenKind::Sizeof);
    expect(TokenKind::LeftParen, "Expected '(' after 'sizeof'");
    
    sizeOf->type = parseTypeRef();
    if (!sizeOf->type) sizeOf->type = errorType("Expected type");
    
    expect(TokenKind::RightParen, "Expected ')' after type");
    sizeOf->location = SourceRange(startToken.location.start, previous().location.end());
    return sizeOf;
}

// ================== Types ==================

TypeRef* Parser::parseTypeRef() {
    auto startToken = tokens.current();
    if (check(TokenKind::Fn)) {
        return parseFunctionType();
    }
    
    auto* namedType = parseNamedType();
    if (!namedType) {
        return nullptr;
    }
    
    TypeRef* currentType = namedType;

    while (true) {
        if (check(TokenKind::LeftBracket)) {
            tokens.advance();
            expect(TokenKind::RightBracket, "Expected ']' for array type");
            auto* arrayType = arena.make<ArrayTypeRef>();
            arrayType->elementType = currentType;
            arrayType->location = SourceRange(startToken.location.start, previous().location.end());
            currentType = arrayType;
        } else if (check(TokenKind::Question)) {
            tokens.advance();
            auto* nullableType = arena.make<NullableTypeRef>();
            nullableType->innerType = currentType;
            nullableType->location = SourceRange(startToken.location.start, previous().location.end());
            currentType = nullableType;
        } else {
            break;
        }
    }
    return currentType;
}

NamedTypeRef* Parser::parseNamedType() {
    if (!check(TokenKind::Identifier)) {
        return nullptr;
    }
    auto startToken = tokens.current();
    auto* type = arena.make<NamedTypeRef>();
    
    std::vector<Identifier*> path;
    do {
        path.push_back(parseIdentifier());
    } while (consume(TokenKind::Dot));
    type->path = arena.makeList(path);
    
    if (check(TokenKind::Less)) {
        type->genericArgs = parseGenericArguments();
    } else {
        type->genericArgs = arena.emptyList<TypeRef*>();
    }
    type->location = SourceRange(startToken.location.start, previous().location.end());
    return type;
}

FunctionTypeRef* Parser::parseFunctionType() {
    auto startToken = tokens.current();
    auto* type = arena.make<FunctionTypeRef>();
    consume(TokenKind::Fn);

    if (check(TokenKind::Less)) {
        parseGenericArguments(); 
    }

    expect(TokenKind::LeftParen, "Expected '(' for function type");
    std::vector<TypeRef*> paramTypes;
    while (!check(TokenKind::RightParen) && !tokens.at_end()) {
        if (auto* paramType = parseTypeRef()) {
            paramTypes.push_back(paramType);
        }
        if (!consume(TokenKind::Comma)) break;
    }
    type->parameterTypes = arena.makeList(paramTypes);
    expect(TokenKind::RightParen, "Expected ')' after function parameters");
    
    if (consume(TokenKind::Colon)) {
        type->returnType = parseTypeRef();
    } else {
        type->returnType = nullptr;
    }
    type->location = SourceRange(startToken.location.start, previous().location.end());
    return type;
}

// ================== Patterns ==================

Pattern* Parser::parsePattern() {
    auto startToken = tokens.current();
    if (consume(TokenKind::In)) {
        auto* pattern = arena.make<InPattern>();
        pattern->innerPattern = parsePattern();
        if (!pattern->innerPattern) {
            auto* binding = arena.make<BindingPattern>();
            binding->name = nullptr;
            pattern->innerPattern = binding;
        }
        pattern->location = SourceRange(startToken.location.start, previous().location.end());
        return pattern;
    }
    
    auto checkpoint = tokens.checkpoint();
    Expression* start = nullptr;
    
    if (tokens.current().is_literal()) start = parseLiteral();
    
    if (checkAny({TokenKind::DotDot, TokenKind::DotDotEquals})) {
        tokens.restore(checkpoint);
        return parseRangePattern();
    }
    
    if (start) {
        tokens.restore(checkpoint);
        auto* pattern = arena.make<LiteralPattern>();
        pattern->literal = static_cast<LiteralExpr*>(parseLiteral());
        pattern->location = pattern->literal->location;
        return pattern;
    }
    if (check(TokenKind::Identifier) && peekNext() == TokenKind::Dot) {
        return parseEnumPattern();
    }
    if (checkAny({TokenKind::Less, TokenKind::Greater, TokenKind::LessEqual, TokenKind::GreaterEqual})) {
        return parseComparisonPattern();
    }
    if (check(TokenKind::Identifier) || check(TokenKind::Underscore)) {
        auto* pattern = arena.make<BindingPattern>();
        if (check(TokenKind::Underscore)) {
            pattern->name = nullptr;
            tokens.advance();
            pattern->location = startToken.location;
        } else {
            pattern->name = parseIdentifier();
            pattern->location = pattern->name->location;
        }
        return pattern;
    }
    return nullptr;
}

Pattern* Parser::parseRangePattern() {
    auto startToken = tokens.current();
    auto* pattern = arena.make<RangePattern>();
    
    if (!checkAny({TokenKind::DotDot, TokenKind::DotDotEquals})) {
        pattern->start = parseExpression();
    } else {
        pattern->start = nullptr;
    }
    
    if (consume(TokenKind::DotDotEquals)) {
        pattern->isInclusive = true;
    } else if (consume(TokenKind::DotDot)) {
        pattern->isInclusive = false;
    }
    
    if (!isPatternTerminator()) {
        pattern->end = parseExpression();
    } else {
        pattern->end = nullptr;
    }
    pattern->location = SourceRange(startToken.location.start, previous().location.end());
    return pattern;
}

Pattern* Parser::parseEnumPattern() {
    auto startToken = tokens.current();
    auto* pattern = arena.make<EnumPattern>();
    std::vector<Identifier*> path;
    
    do {
        path.push_back(parseIdentifier());
    } while (consume(TokenKind::Dot));
    
    pattern->path = arena.makeList(path);
    
    if (check(TokenKind::LeftParen)) {
        tokens.advance();
        std::vector<Pattern*> argPatterns;
        while (!check(TokenKind::RightParen) && !tokens.at_end()) {
            if (auto* argPattern = parsePattern()) {
                argPatterns.push_back(argPattern);
            }
            if (!consume(TokenKind::Comma)) break;
        }
        pattern->argumentPatterns = arena.makeList(argPatterns);
        expect(TokenKind::RightParen, "Expected ')' after enum arguments");
    } else {
        pattern->argumentPatterns = arena.emptyList<Pattern*>();
    }
    pattern->location = SourceRange(startToken.location.start, previous().location.end());
    return pattern;
}

Pattern* Parser::parseComparisonPattern() {
    auto startToken = tokens.current();
    auto* pattern = arena.make<ComparisonPattern>();
    Token op = tokens.current();
    tokens.advance();

    switch (op.kind) {
        case TokenKind::Less: pattern->op = ComparisonPattern::Op::Less; break;
        case TokenKind::Greater: pattern->op = ComparisonPattern::Op::Greater; break;
        case TokenKind::LessEqual: pattern->op = ComparisonPattern::Op::LessEqual; break;
        case TokenKind::GreaterEqual: pattern->op = ComparisonPattern::Op::GreaterEqual; break;
        default: pattern->op = ComparisonPattern::Op::Less; break;
    }

    pattern->value = parseExpression();
    if (!pattern->value) {
        pattern->value = errorExpr("Expected value after comparison operator");
    }
    pattern->location = SourceRange(startToken.location.start, pattern->value->location.end());
    return pattern;
}

// ================== Helper Functions ==================

Identifier* Parser::parseIdentifier() {
    if (!check(TokenKind::Identifier)) {
        error("Expected identifier");
        auto* id = arena.makeIdentifier("");
        id->location = tokens.current().location;
        return id;
    }
    auto tok = tokens.current();
    auto* id = arena.makeIdentifier(tok.text);
    id->location = tok.location;
    tokens.advance();
    return id;
}

TypedIdentifier* Parser::parseTypedIdentifier() {
    auto startToken = tokens.current();
    auto* ti = arena.make<TypedIdentifier>();
    if (consume(TokenKind::Var)) {
        ti->type = nullptr;
        ti->name = parseIdentifier();
    } else {
        ti->type = parseTypeRef();
        if (!ti->type) {
            error("Expected type specification");
            ti->type = errorType("Expected type");
        }
        ti->name = parseIdentifier();
    }
    ti->location = SourceRange(startToken.location.start, previous().location.end());
    return ti;
}

List<GenericParamDecl*> Parser::parseGenericParams() {
    if (!consume(TokenKind::Less)) {
        return arena.emptyList<GenericParamDecl*>();
    }
    
    std::vector<GenericParamDecl*> params;
    while (!check(TokenKind::Greater) && !tokens.at_end()) {
        auto* param = arena.make<GenericParamDecl>();
        param->name = parseIdentifier();
        param->constraints = arena.emptyList<TypeConstraint*>();
        param->location = param->name->location;
        params.push_back(param);
        if (!consume(TokenKind::Comma)) break;
    }
    expect(TokenKind::Greater, "Expected '>' after generic parameters");
    return arena.makeList(params);
}

List<TypeRef*> Parser::parseGenericArguments() {
    if (!consume(TokenKind::Less)) {
        return arena.emptyList<TypeRef*>();
    }
    
    std::vector<TypeRef*> args;
    while (!check(TokenKind::Greater) && !tokens.at_end()) {
        if (auto* arg = parseTypeRef()) {
            args.push_back(arg);
        }
        if (!consume(TokenKind::Comma)) break;
    }
    expect(TokenKind::Greater, "Expected '>' after generic arguments");
    return arena.makeList(args);
}

List<ParameterDecl*> Parser::parseParameterList() {
    expect(TokenKind::LeftParen, "Expected '(' for parameter list");
    
    std::vector<ParameterDecl*> params;
    while (!check(TokenKind::RightParen) && !tokens.at_end()) {
        auto startToken = tokens.current();
        auto* param = arena.make<ParameterDecl>();
        param->param = parseTypedIdentifier();
        if (!param->param) {
            auto* ti = arena.make<TypedIdentifier>();
            ti->type = errorType("Expected parameter type");
            ti->name = arena.makeIdentifier("");
            param->param = ti;
        }

        if (consume(TokenKind::Assign)) {
            param->defaultValue = parseExpression();
        } else {
            param->defaultValue = nullptr;
        }
        param->location = SourceRange(startToken.location.start, previous().location.end());
        params.push_back(param);
        if (!consume(TokenKind::Comma)) break;
    }
    
    expect(TokenKind::RightParen, "Expected ')' after parameters");
    return arena.makeList(params);
}

List<TypeRef*> Parser::parseBaseTypeList() {
    std::vector<TypeRef*> types;
    do {
        if (auto* type = parseTypeRef()) {
            types.push_back(type);
        }
    } while (consume(TokenKind::Comma));
    return arena.makeList(types);
}

void Parser::parseWhereConstraints(List<GenericParamDecl*> params) {
    if (!consume(TokenKind::Where)) {
        return;
    }
    
    do {
        if (!check(TokenKind::Identifier)) break;
        auto* paramName = parseIdentifier();
        expect(TokenKind::Colon, "Expected ':' after type parameter");
        
        std::vector<TypeConstraint*> constraints;
        do {
            auto constraintStart = tokens.current();
            if (check(TokenKind::Ref)) {
                consume(TokenKind::Ref);
                expect(TokenKind::Type, "Expected 'type' after 'ref' in constraint");
                auto* kindConstraint = arena.make<TypeKindConstraint>();
                kindConstraint->kind = TypeKindConstraint::Kind::RefType;
                kindConstraint->location = SourceRange(constraintStart.location.start, previous().location.end());
                constraints.push_back(kindConstraint);
            } else if (check(TokenKind::Type)) {
                consume(TokenKind::Type);
                auto* kindConstraint = arena.make<TypeKindConstraint>();
                kindConstraint->kind = TypeKindConstraint::Kind::ValueType;
                kindConstraint->location = SourceRange(constraintStart.location.start, previous().location.end());
                constraints.push_back(kindConstraint);
            } else if (consume(TokenKind::New)) {
                auto* ctorConstraint = arena.make<ConstructorConstraint>();
                if (check(TokenKind::LeftParen)) {
                    tokens.advance();
                    std::vector<TypeRef*> paramTypes;
                    while (!check(TokenKind::RightParen) && !tokens.at_end()) {
                        if (auto* type = parseTypeRef()) paramTypes.push_back(type);
                        if (!consume(TokenKind::Comma)) break;
                    }
                    expect(TokenKind::RightParen, "Expected ')'");
                    ctorConstraint->parameterTypes = arena.makeList(paramTypes);
                } else {
                    ctorConstraint->parameterTypes = arena.emptyList<TypeRef*>();
                }
                ctorConstraint->location = SourceRange(constraintStart.location.start, previous().location.end());
                constraints.push_back(ctorConstraint);
            } else {
                auto* baseConstraint = arena.make<BaseTypeConstraint>();
                baseConstraint->baseType = parseTypeRef();
                if (!baseConstraint->baseType) {
                    baseConstraint->baseType = errorType("Expected constraint type");
                }
                baseConstraint->location = SourceRange(constraintStart.location.start, previous().location.end());
                constraints.push_back(baseConstraint);
            }
        } while (consume(TokenKind::Comma));

        for (auto* param : params) {
            if (param->name->text == paramName->text) {
                param->constraints = arena.makeList(constraints);
                break;
            }
        }
    } while (consume(TokenKind::Comma));
}

bool Parser::isExpressionTerminator() {
    return checkAny({TokenKind::Semicolon, TokenKind::RightParen,
                     TokenKind::RightBracket, TokenKind::RightBrace,
                     TokenKind::Comma, TokenKind::Colon,
                     TokenKind::FatArrow, TokenKind::EndOfFile});
}

bool Parser::isPatternTerminator() {
    return checkAny({TokenKind::FatArrow, TokenKind::Comma,
                     TokenKind::RightParen, TokenKind::RightBrace});
}

} // namespace Myre