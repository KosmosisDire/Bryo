#include "parser/parser.hpp"

namespace Myre {

Parser::Parser(TokenStream& tokens) : tokens(tokens) {
    contextStack.push_back(Context::TOP_LEVEL);
}

Parser::~Parser() = default;

// ================== Public API ==================

CompilationUnit* Parser::parse() {
    auto* unit = arena.make<CompilationUnit>();
    std::vector<Statement*> statements;

    while (!tokens.at_end()) {
        auto* stmt = parseTopLevelStatement();
        if (stmt) {
            statements.push_back(stmt);
        }
    }

    unit->topLevelStatements = arena.makeList(statements);
    return unit;
}

const std::vector<Parser::ParseError>& Parser::getErrors() const {
    return errors;
}

bool Parser::hasErrors() const {
    return !errors.empty();
}

// ================== Error Handling ==================

void Parser::error(const std::string& msg) {
    errors.push_back({msg, tokens.current().location, ParseError::ERROR});
}

void Parser::warning(const std::string& msg) {
    errors.push_back({msg, tokens.current().location, ParseError::WARNING});
}

ErrorExpression* Parser::errorExpr(const std::string& msg) {
    error(msg);
    return arena.makeErrorExpr(msg);
}

ErrorStatement* Parser::errorStmt(const std::string& msg) {
    error(msg);
    return arena.makeErrorStmt(msg);
}

ErrorTypeRef* Parser::errorType(const std::string& msg) {
    error(msg);
    return arena.makeErrorType(msg);
}

void Parser::synchronize() {
    while (!tokens.at_end()) {
        if (tokens.current().starts_declaration() || tokens.current().starts_statement()) {
            break;
        }
        tokens.advance();
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

bool Parser::inTypeBody() const {
    for (auto it = contextStack.rbegin(); it != contextStack.rend(); ++it) {
        if (*it == Context::TYPE_BODY) return true;
    }
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
        return parseNamespaceDecl();
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

bool Parser::checkDeclarationStart() {
    return checkAny({TokenKind::Type, TokenKind::Enum, TokenKind::Fn,
                     TokenKind::Var, TokenKind::New, TokenKind::Ref,
                     TokenKind::Namespace});
}

Declaration* Parser::parseDeclaration() {
    ModifierSet modifiers = parseModifiers();

    if (check(TokenKind::Namespace)) {
        auto* ns = parseNamespaceDecl();
        ns->modifiers = modifiers;
        return ns;
    }
    if (checkAny({TokenKind::Type, TokenKind::Ref, TokenKind::Enum, TokenKind::Static})) {
        return parseTypeDecl(modifiers);
    }
    if (check(TokenKind::Fn)) {
        return parseFunctionDecl(modifiers);
    }
    if (check(TokenKind::New)) {
        return parseConstructorDecl(modifiers);
    }
    if (check(TokenKind::Inherit)) {
        return parseInheritFunctionDecl(modifiers);
    }
    if (check(TokenKind::Var)) {
        return parseVarDeclaration(modifiers);
    }

    auto checkpoint = tokens.checkpoint();
    auto* type = parseTypeRef();
    if (type && check(TokenKind::Identifier)) {
        return parseTypedMemberDeclaration(modifiers, type);
    }
    
    tokens.restore(checkpoint);
    error("Expected declaration");
    return nullptr;
}

ModifierSet Parser::parseModifiers() {
    ModifierSet mods;
    while (tokens.current().is_modifier()) {
        switch (tokens.current().kind) {
            case TokenKind::Public: mods.access = ModifierSet::Access::Public; break;
            case TokenKind::Private: mods.access = ModifierSet::Access::Private; break;
            case TokenKind::Protected: mods.access = ModifierSet::Access::Protected; break;
            case TokenKind::Static: mods.isStatic = true; break;
            case TokenKind::Virtual: mods.isVirtual = true; break;
            case TokenKind::Abstract: mods.isAbstract = true; break;
            case TokenKind::Override: mods.isOverride = true; break;
            case TokenKind::Ref: mods.isRef = true; break;
            case TokenKind::Enforced: mods.isEnforced = true; break;
            case TokenKind::Inherit: mods.isInherit = true; break;
            default: break;
        }
        tokens.advance();
    }
    return mods;
}

TypeDecl* Parser::parseTypeDecl(ModifierSet modifiers) {
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

    return decl;
}

EnumCaseDecl* Parser::parseEnumCase() {
    auto* decl = arena.make<EnumCaseDecl>();
    decl->name = parseIdentifier();
    
    if (check(TokenKind::LeftParen)) {
        decl->associatedData = parseParameterList();
    } else {
        decl->associatedData = arena.emptyList<ParameterDecl*>();
    }
    
    return decl;
}

FunctionDecl* Parser::parseFunctionDecl(ModifierSet modifiers) {
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
    return decl;
}

ConstructorDecl* Parser::parseConstructorDecl(ModifierSet modifiers) {
    auto* decl = arena.make<ConstructorDecl>();
    decl->modifiers = modifiers;

    consume(TokenKind::New);
    decl->parameters = parseParameterList();

    expect(TokenKind::LeftBrace, "Expected '{' after constructor parameters");
    decl->body = withContext(Context::FUNCTION, [this]() { return parseBlock(); });

    if (!decl->body) {
        auto* block = arena.make<Block>();
        block->statements = arena.emptyList<Statement*>();
        decl->body = block;
    }
    return decl;
}

InheritFunctionDecl* Parser::parseInheritFunctionDecl(ModifierSet modifiers) {
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
    return decl;
}

Declaration* Parser::parseVarDeclaration(ModifierSet modifiers) {
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
        return parseMemberVariableDecl(modifiers, nullptr, true);
    }
    if (check(TokenKind::Assign)) {
        auto initCheckpoint = tokens.checkpoint();
        tokens.advance();
        parseExpression();
        if (check(TokenKind::LeftBrace)) {
            tokens.restore(checkpoint);
            return parseMemberVariableDecl(modifiers, nullptr, true);
        }
        tokens.restore(initCheckpoint);
    }

    if (contextStack.back() == Context::TYPE_BODY) {
        tokens.restore(checkpoint);
        return parseMemberVariableDecl(modifiers, nullptr, true);
    } else {
        auto* decl = arena.make<VariableDecl>();
        decl->modifiers = modifiers;
        
        auto* ti = arena.make<TypedIdentifier>();
        ti->type = nullptr;
        ti->name = name;
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
        return decl;
    }
}

Declaration* Parser::parseTypedMemberDeclaration(ModifierSet modifiers, TypeRef* type) {
    std::vector<Declaration*> declarations;
    
    do {
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
            return prop;
        } else {
            auto* field = arena.make<MemberVariableDecl>();
            field->modifiers = modifiers;
            field->type = type;
            field->name = name;
            field->initializer = initializer;
            field->getter = nullptr;
            field->setter = nullptr;
            declarations.push_back(field);
        }
    } while (consume(TokenKind::Comma));
    
    expect(TokenKind::Semicolon, "Expected ';' after field declaration");
    return declarations.empty() ? nullptr : declarations[0];
}

MemberVariableDecl* Parser::parseMemberVariableDecl(ModifierSet modifiers, TypeRef* type, bool isVar) {
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
        getter->kind = PropertyAccessor::Kind::Get;
        getter->body = parseExpression();
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
    return decl;
}

void Parser::parsePropertyAccessors(MemberVariableDecl* prop) {
    consume(TokenKind::LeftBrace);
    while (!check(TokenKind::RightBrace) && !tokens.at_end()) {
        ModifierSet accessorMods = parseModifiers();
        
        if (consume(TokenKind::Get)) {
            auto* getter = arena.make<PropertyAccessor>();
            getter->kind = PropertyAccessor::Kind::Get;
            getter->modifiers = accessorMods;
            
            if (consume(TokenKind::FatArrow)) {
                // Check for common mistake: => { instead of just {
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
            prop->getter = getter;
        } else if (consume(TokenKind::Set)) {
            auto* setter = arena.make<PropertyAccessor>();
            setter->kind = PropertyAccessor::Kind::Set;
            setter->modifiers = accessorMods;
            
            if (consume(TokenKind::FatArrow)) {
                // Check for common mistake: => { instead of just {
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
            prop->setter = setter;
        } else {
            error("Expected 'get' or 'set' in property accessor");
            // Synchronize by skipping to next semicolon or closing brace
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

NamespaceDecl* Parser::parseNamespaceDecl() {
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
    return decl;
}

// ================== Statements ==================

Statement* Parser::parseStatement() {
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
    
    // Check if this might be a typed variable declaration (Type varName)
    if (check(TokenKind::Identifier)) {
        auto checkpoint = tokens.checkpoint();
        auto* type = parseTypeRef();
        if (type && check(TokenKind::Identifier)) {
            // This looks like a typed declaration
            tokens.restore(checkpoint);
            return parseDeclaration();
        }
        tokens.restore(checkpoint);
    }
    
    return parseExpressionStatement();
}

Block* Parser::parseBlock() {
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
    return block;
}

Statement* Parser::parseIfStatement() {
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

    auto* exprStmt = arena.make<ExpressionStmt>();
    exprStmt->expression = ifExpr;
    return exprStmt;
}

WhileStmt* Parser::parseWhileStatement() {
    auto* stmt = arena.make<WhileStmt>();
    consume(TokenKind::While);
    expect(TokenKind::LeftParen, "Expected '(' after 'while'");

    stmt->condition = parseExpression();
    if (!stmt->condition) stmt->condition = errorExpr("Expected condition");

    expect(TokenKind::RightParen, "Expected ')' after condition");

    stmt->body = withContext(Context::LOOP, [this]() { return parseStatement(); });
    if (!stmt->body) stmt->body = errorStmt("Expected loop body");

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
    auto* stmt = arena.make<ForStmt>();
    consume(TokenKind::For);
    expect(TokenKind::LeftParen, "Expected '(' after 'for'");

    if (!check(TokenKind::Semicolon)) {
    // Check for typed declaration pattern (Type identifier = ...)
    auto checkpoint = tokens.checkpoint();
    auto* type = parseTypeRef();
    if (type && check(TokenKind::Identifier)) {
        // This is a typed declaration
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

    if (!stmt->initializer || !stmt->initializer->is<VariableDecl>()) {
        expect(TokenKind::Semicolon, "Expected ';' after initializer");
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

    return stmt;
}

ForInStmt* Parser::parseForInStatement() {
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

    return stmt;
}

ReturnStmt* Parser::parseReturnStatement() {
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
    return stmt;
}

BreakStmt* Parser::parseBreakStatement() {
    auto* stmt = arena.make<BreakStmt>();
    consume(TokenKind::Break);
    expect(TokenKind::Semicolon, "Expected ';' after break");
    if (!inLoop()) {
        warning("Break statement outside loop");
    }
    return stmt;
}

ContinueStmt* Parser::parseContinueStatement() {
    auto* stmt = arena.make<ContinueStmt>();
    consume(TokenKind::Continue);
    expect(TokenKind::Semicolon, "Expected ';' after continue");
    if (!inLoop()) {
        warning("Continue statement outside loop");
    }
    return stmt;
}

ExpressionStmt* Parser::parseExpressionStatement() {
    auto* stmt = arena.make<ExpressionStmt>();
    stmt->expression = parseExpression();
    if (!stmt->expression) {
        stmt->expression = errorExpr("Expected expression");
    }
    expect(TokenKind::Semicolon, "Expected ';' after expression");
    return stmt;
}

UsingDirective* Parser::parseUsingDirective() {
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
    return directive;
}

// ================== Expressions ==================

Expression* Parser::parseExpression(int minPrecedence) {
    auto* left = parsePrimaryExpression();
    if (!left) {
        synchronize();
        return nullptr; // No need for error here, primary parser handles it.
    }
    return parseBinaryExpression(left, minPrecedence);
}

Expression* Parser::parseBinaryExpression(Expression* left, int minPrecedence) {
    while (!tokens.at_end()) {
        Token op = tokens.current();
        int precedence = op.get_binary_precedence();

        // handle ternary operator (cond ? then : else)
        if (op.kind == TokenKind::Question)
        {
            if (minPrecedence > precedence) {
                break;
            }
            
            tokens.advance();  // consume '?'
            
            auto* conditional = arena.make<ConditionalExpr>();
            conditional->condition = left;
            
            // Parse the then branch (with higher precedence)
            conditional->thenExpr = parseExpression(precedence + 1);
            if (!conditional->thenExpr) {
                conditional->thenExpr = errorExpr("Expected expression after '?'");
            }
            
            expect(TokenKind::Colon, "Expected ':' in conditional expression");
            
            // Parse the else branch with same precedence (right-associative)
            conditional->elseExpr = parseExpression(precedence);
            if (!conditional->elseExpr) {
                conditional->elseExpr = errorExpr("Expected expression after ':'");
            }
            
            return conditional;
        }

        if (precedence < minPrecedence || precedence == 0) {
            break;
        }

        if (op.is_assignment_operator()) {
            tokens.advance();
            auto* assign = arena.make<AssignmentExpr>();
            assign->target = left;
            assign->op = op.to_assignment_operator_kind();
            assign->value = parseExpression(precedence);
            if (!assign->value) {
                assign->value = errorExpr("Expected value in assignment");
            }
            return assign;
        }

        if (op.is_any({TokenKind::DotDot, TokenKind::DotDotEquals})) {
            return parseRangeExpression(left);
        }

        tokens.advance();
        int nextPrecedence = op.is_right_associative() ? precedence : precedence + 1;

        auto* right = parseExpression(nextPrecedence);
        if (!right) {
            right = errorExpr("Expected right operand");
        }

        auto* binary = arena.make<BinaryExpr>();
        binary->left = left;
        binary->op = op.to_binary_operator_kind();
        binary->right = right;
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
        expr = parseNameExpression();
    } else if (check(TokenKind::This)) {
        auto* thisExpr = arena.make<ThisExpr>();
        tokens.advance();
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
    } else if (check(TokenKind::Dot)) {
        tokens.advance();
        if (check(TokenKind::Identifier)) {
            auto* enumExpr = arena.make<NameExpr>();
            std::vector<Identifier*> parts;
            parts.push_back(parseIdentifier());
            enumExpr->parts = arena.makeList(parts);
            expr = enumExpr;
        } else {
            expr = errorExpr("Expected enum member after '.'");
        }
    }

    if (!expr) {
        return nullptr;
    }

    return parsePostfixExpression(expr);
}

Expression* Parser::parsePostfixExpression(Expression* expr) {
    while (true) {
        if (check(TokenKind::LeftParen)) {
            expr = parseCallExpression(expr);
        } else if (check(TokenKind::Dot)) {
            tokens.advance();
            auto* member = arena.make<MemberAccessExpr>();
            member->object = expr;
            member->member = parseIdentifier();
            expr = member;
        } else if (check(TokenKind::LeftBracket)) {
            tokens.advance();
            auto* indexer = arena.make<IndexerExpr>();
            indexer->object = expr;
            indexer->index = parseExpression();
            if (!indexer->index) {
                indexer->index = errorExpr("Expected index expression");
            }
            expect(TokenKind::RightBracket, "Expected ']' after index");
            expr = indexer;
        } else if (checkAny({TokenKind::Increment, TokenKind::Decrement})) {
            auto* unary = arena.make<UnaryExpr>();
            unary->operand = expr;
            unary->op = tokens.current().to_unary_operator_kind();
            unary->isPostfix = true;
            tokens.advance();
            expr = unary;
        } else {
            break;
        }
    }
    return expr;
}

Expression* Parser::parseUnaryExpression() {
    auto* unary = arena.make<UnaryExpr>();
    Token op = tokens.current();
    tokens.advance();
    unary->op = op.to_unary_operator_kind();
    unary->isPostfix = false;
    unary->operand = parseExpression(op.get_unary_precedence());
    if (!unary->operand) {
        unary->operand = errorExpr("Expected operand after unary operator");
    }
    return unary;
}

Expression* Parser::parseRangeExpression(Expression* start) {
    auto* range = arena.make<RangeExpr>();
    range->start = start;

    if (consume(TokenKind::DotDotEquals)) {
        range->isInclusive = true;
    } else if (consume(TokenKind::DotDot)) {
        range->isInclusive = false;
    }

    if (!isExpressionTerminator() && !check(TokenKind::By)) {
        range->end = parseExpression(120); // High precedence to bind tightly
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
    return range;
}

Expression* Parser::parseLiteral() {
    auto* lit = arena.make<LiteralExpr>();
    Token tok = tokens.current();
    lit->value = tok.text;

    switch (tok.kind) {
        case TokenKind::LiteralI32: case TokenKind::LiteralI64: lit->kind = LiteralExpr::Kind::Integer; break;
        case TokenKind::LiteralF32: case TokenKind::LiteralF64: lit->kind = LiteralExpr::Kind::Float; break;
        case TokenKind::LiteralString: lit->kind = LiteralExpr::Kind::String; break;
        case TokenKind::LiteralChar: lit->kind = LiteralExpr::Kind::Char; break;
        case TokenKind::LiteralBool: lit->kind = LiteralExpr::Kind::Bool; break;
        case TokenKind::Null: lit->kind = LiteralExpr::Kind::Null; break;
        default: lit->kind = LiteralExpr::Kind::Integer; break; // Should not happen
    }
    tokens.advance();
    return lit;
}

Expression* Parser::parseNameExpression() {
    auto* name = arena.make<NameExpr>();
    std::vector<Identifier*> parts;
    do {
        parts.push_back(parseIdentifier());
    } while (consume(TokenKind::DoubleColon) || (check(TokenKind::Dot) && peekNext() == TokenKind::Identifier && consume(TokenKind::Dot)));
    name->parts = arena.makeList(parts);
    return name;
}

Expression* Parser::parseParenthesizedOrLambda() {
    auto checkpoint = tokens.checkpoint();
    consume(TokenKind::LeftParen);
    
    bool isLambda = false;
    if (check(TokenKind::RightParen)) {
        tokens.advance();
        if (check(TokenKind::FatArrow)) {
            isLambda = true;
        }
    } else if (check(TokenKind::Identifier)) {
        tokens.advance();
        if (checkAny({TokenKind::FatArrow, TokenKind::Comma, TokenKind::Colon, TokenKind::Identifier})) {
            isLambda = true;
        }
    } else if (check(TokenKind::FatArrow)) { // e.g., for `() => ...`
        isLambda = true;
    }

    tokens.restore(checkpoint);

    if (isLambda) {
        return parseLambdaExpression();
    } else {
        consume(TokenKind::LeftParen);
        auto* expr = parseExpression();
        if (!expr) expr = errorExpr("Expected expression");
        expect(TokenKind::RightParen, "Expected ')' after expression");
        return expr;
    }
}

Expression* Parser::parseArrayLiteral() {
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
    return array;
}

Expression* Parser::parseCallExpression(Expression* callee) {
    auto* call = arena.make<CallExpr>();
    call->callee = callee;
    consume(TokenKind::LeftParen);

    std::vector<Expression*> args;
    while (!check(TokenKind::RightParen) && !tokens.at_end()) {
        if (auto* arg = parseExpression()) {
            args.push_back(arg);
        }
        if (!consume(TokenKind::Comma)) break;
    }
    call->arguments = arena.makeList(args);
    
    expect(TokenKind::RightParen, "Expected ')' after arguments");
    return call;
}

Expression* Parser::parseNewExpression() {
    auto* newExpr = arena.make<NewExpr>();
    consume(TokenKind::New);

    newExpr->type = parseTypeRef();
    if (!newExpr->type) {
        newExpr->type = errorType("Expected type after 'new'");
    }

    if (check(TokenKind::LeftParen)) {
        consume(TokenKind::LeftParen);
        std::vector<Expression*> args;
        while (!check(TokenKind::RightParen) && !tokens.at_end()) {
            if (auto* arg = parseExpression()) {
                args.push_back(arg);
            }
            if (!consume(TokenKind::Comma)) break;
        }
        newExpr->arguments = arena.makeList(args);
        expect(TokenKind::RightParen, "Expected ')' after constructor arguments");
    } else {
        newExpr->arguments = arena.emptyList<Expression*>();
    }
    return newExpr;
}

Expression* Parser::parseLambdaExpression() {
    auto* lambda = arena.make<LambdaExpr>();
    
    if (check(TokenKind::LeftParen)) {
        consume(TokenKind::LeftParen);
        std::vector<ParameterDecl*> params;
        while (!check(TokenKind::RightParen) && !tokens.at_end()) {
            auto* param = arena.make<ParameterDecl>();
            param->param = parseTypedIdentifier();
            if (!param->param) {
                auto* ti = arena.make<TypedIdentifier>();
                ti->type = errorType("Expected parameter type");
                ti->name = arena.makeIdentifier("");
                param->param = ti;
            }
            param->defaultValue = nullptr;
            params.push_back(param);
            if (!consume(TokenKind::Comma)) break;
        }
        lambda->parameters = arena.makeList(params);
        expect(TokenKind::RightParen, "Expected ')' after lambda parameters");
    } else {
        // Single parameter lambda: x => ...
        std::vector<ParameterDecl*> params;
        auto* param = arena.make<ParameterDecl>();
        auto* ti = arena.make<TypedIdentifier>();
        ti->type = nullptr; // Inferred
        ti->name = parseIdentifier();
        param->param = ti;
        param->defaultValue = nullptr;
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
        lambda->body = exprStmt;
    }
    return lambda;
}

Expression* Parser::parseMatchExpression() {
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
    return match;
}

MatchArm* Parser::parseMatchArm() {
    auto* arm = arena.make<MatchArm>();
    arm->pattern = parsePattern();
    if (!arm->pattern) {
        auto* binding = arena.make<BindingPattern>();
        binding->name = nullptr;
        arm->pattern = binding;
        error("Expected pattern in match arm");
    }

    expect(TokenKind::FatArrow, "Expected '=>' after pattern");
    
    // Check if it's a block or expression
    if (check(TokenKind::LeftBrace)) {
        // Block-style arm: parse as a block statement
        arm->result = parseBlock();
    } else {
        // Expression-style arm: parse expression and wrap in ExpressionStmt
        auto* expr = parseExpression();
        if (!expr) {
            expr = errorExpr("Expected match arm result");
        }
        auto* exprStmt = arena.make<ExpressionStmt>();
        exprStmt->expression = expr;
        arm->result = exprStmt;
    }
    
    if (!arm->result) {
        arm->result = errorStmt("Expected match arm result");
    }
    
    return arm;
}

Expression* Parser::parseTypeOfExpression() {
    auto* typeOf = arena.make<TypeOfExpr>();
    consume(TokenKind::Typeof);
    expect(TokenKind::LeftParen, "Expected '(' after 'typeof'");
    
    typeOf->type = parseTypeRef();
    if (!typeOf->type) typeOf->type = errorType("Expected type");
    
    expect(TokenKind::RightParen, "Expected ')' after type");
    return typeOf;
}

Expression* Parser::parseSizeOfExpression() {
    auto* sizeOf = arena.make<SizeOfExpr>();
    consume(TokenKind::Sizeof);
    expect(TokenKind::LeftParen, "Expected '(' after 'sizeof'");
    
    sizeOf->type = parseTypeRef();
    if (!sizeOf->type) sizeOf->type = errorType("Expected type");
    
    expect(TokenKind::RightParen, "Expected ')' after type");
    return sizeOf;
}

// ================== Types ==================

TypeRef* Parser::parseTypeRef() {
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
            currentType = arrayType;
        } else if (check(TokenKind::Question)) {
            tokens.advance();
            auto* nullableType = arena.make<NullableTypeRef>();
            nullableType->innerType = currentType;
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
    return type;
}

FunctionTypeRef* Parser::parseFunctionType() {
    auto* type = arena.make<FunctionTypeRef>();
    consume(TokenKind::Fn);

    if (check(TokenKind::Less)) {
        parseGenericArguments(); // Skipped for now
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
    return type;
}

// ================== Patterns ==================

Pattern* Parser::parsePattern() {
    if (tokens.current().is_literal()) {
        auto* pattern = arena.make<LiteralPattern>();
        pattern->literal = static_cast<LiteralExpr*>(parseLiteral());
        return pattern;
    }
    if (checkAny({TokenKind::DotDot, TokenKind::DotDotEquals})) {
        return parseRangePattern();
    }
    if (consume(TokenKind::In)) {
        auto* pattern = arena.make<InPattern>();
        pattern->innerPattern = parsePattern();
        if (!pattern->innerPattern) {
            auto* binding = arena.make<BindingPattern>();
            binding->name = nullptr;
            pattern->innerPattern = binding;
        }
        return pattern;
    }
    if (check(TokenKind::Dot) || (check(TokenKind::Identifier) && peekNext() == TokenKind::Dot)) {
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
        } else {
            pattern->name = parseIdentifier();
        }
        return pattern;
    }
    return nullptr;
}

Pattern* Parser::parseRangePattern() {
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
    return pattern;
}

Pattern* Parser::parseEnumPattern() {
    auto* pattern = arena.make<EnumPattern>();
    std::vector<Identifier*> path;
    
    if (consume(TokenKind::Dot)) {
        path.push_back(parseIdentifier());
    } else {
        do {
            path.push_back(parseIdentifier());
        } while (consume(TokenKind::Dot));
    }
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
    return pattern;
}

Pattern* Parser::parseComparisonPattern() {
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
    return pattern;
}

// ================== Helper Functions ==================

Identifier* Parser::parseIdentifier() {
    if (!check(TokenKind::Identifier)) {
        error("Expected identifier");
        return arena.makeIdentifier("");
    }
    auto* id = arena.makeIdentifier(tokens.current().text);
    tokens.advance();
    return id;
}

TypedIdentifier* Parser::parseTypedIdentifier() {
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
            if (check(TokenKind::Ref)) {
                consume(TokenKind::Ref);
                expect(TokenKind::Type, "Expected 'type' after 'ref' in constraint");
                auto* kindConstraint = arena.make<TypeKindConstraint>();
                kindConstraint->kind = TypeKindConstraint::Kind::RefType;
                constraints.push_back(kindConstraint);
            } else if (check(TokenKind::Type)) {
                consume(TokenKind::Type);
                auto* kindConstraint = arena.make<TypeKindConstraint>();
                kindConstraint->kind = TypeKindConstraint::Kind::ValueType;
                constraints.push_back(kindConstraint);
            }
            if (consume(TokenKind::New)) {
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
                constraints.push_back(ctorConstraint);
            } else {
                auto* baseConstraint = arena.make<BaseTypeConstraint>();
                baseConstraint->baseType = parseTypeRef();
                if (!baseConstraint->baseType) {
                    baseConstraint->baseType = errorType("Expected constraint type");
                }
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

} // namespace Myre::AST