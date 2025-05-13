#include "script_parser.hpp"
#include "script_token.hpp" // Assuming token_type_to_string is here
#include "platform.hpp"     // For launchDebugger
#include <algorithm>
#include <iostream> // For debugging, can be removed

namespace Mycelium::Scripting::Lang
{

ScriptParser::ScriptParser(const std::vector<Token>& tokens)
    : m_tokens(tokens), m_currentIndex(0)
{
    // Constructor remains largely the same
}

// Core token handling methods (is_at_end, peek_token, current_token, previous_token,
// consume_token, advance_token, match_token, check_token) remain the same.
// Error and AST node creation helpers (create_error, make_ast_node, finalize_node_location, with_parent_context)
// also remain the same. I'll copy them for completeness and then focus on the parsing rules.

bool ScriptParser::is_at_end() const
{
    return m_currentIndex >= m_tokens.size() || current_token().type == TokenType::EndOfFile;
}

const Token& ScriptParser::peek_token(int offset) const
{
    size_t targetIndex = m_currentIndex + offset;
    if (targetIndex >= m_tokens.size())
    {
        return m_tokens.back(); // Should be EOF
    }
    return m_tokens[targetIndex];
}

const Token& ScriptParser::current_token() const
{
    if (m_currentIndex >= m_tokens.size()) {
        return m_tokens.back(); // Should be EOF
    }
    return m_tokens[m_currentIndex];
}

const Token& ScriptParser::previous_token() const
{
    if (m_currentIndex == 0)
    {
        throw std::logic_error("previous_token() called at the beginning of token stream or before first advance.");
    }
    if (m_currentIndex -1 >= m_tokens.size()){
        return m_tokens.back();
    }
    return m_tokens[m_currentIndex - 1];
}

const Token& ScriptParser::consume_token(TokenType expectedType, const std::string& errorMessage)
{
    const Token& token = current_token();
    if (token.type == expectedType)
    {
        return advance_token();
    }
    throw create_error(token, errorMessage + " Found " + token_type_to_string(token.type) + " ('" + token.lexeme + "') instead.");
}

const Token& ScriptParser::advance_token()
{
    if (current_token().type != TokenType::EndOfFile)
    {
        m_currentIndex++;
    }
    return previous_token();
}

bool ScriptParser::match_token(TokenType type)
{
    if (check_token(type))
    {
        advance_token();
        return true;
    }
    return false;
}

bool ScriptParser::match_token(const std::vector<TokenType>& types)
{
    if (check_token(types))
    {
        advance_token();
        return true;
    }
    return false;
}

bool ScriptParser::check_token(TokenType type) const
{
    if (m_currentIndex >= m_tokens.size()) return false;
    return current_token().type == type;
}

bool ScriptParser::check_token(const std::vector<TokenType>& types) const
{
    if (m_currentIndex >= m_tokens.size()) return false;
    TokenType currentType = current_token().type;

    if (currentType == TokenType::At) 
    {
        launchDebugger();
    }

    return std::any_of(types.begin(), types.end(),
                       [currentType](TokenType t){ return t == currentType; });
}

ParseError ScriptParser::create_error(const Token& token, const std::string& message)
{
    return ParseError(message, token.location);
}
ParseError ScriptParser::create_error(const std::string& message)
{
    if (m_currentIndex < m_tokens.size()) {
        return ParseError(message, current_token().location);
    } else if (!m_tokens.empty()) {
        return ParseError(message, m_tokens.back().location);
    }
    SourceLocation unknownLoc;
    return ParseError(message, unknownLoc);
}

void ScriptParser::finalize_node_location(std::shared_ptr<AstNode> node, const SourceLocation& startLocation)
{
    if (!node) return;
    SourceLocation finalLocation = startLocation;
    if (m_currentIndex > 0 && (m_currentIndex -1) < m_tokens.size() )
    {
        const Token& lastConsumedToken = previous_token();
        finalLocation.line_end = lastConsumedToken.location.line_end;
        finalLocation.column_end = lastConsumedToken.location.column_end;
    }
    node->location = finalLocation;
}

void ScriptParser::finalize_node_location(std::shared_ptr<AstNode> node, const Token& startToken)
{
    finalize_node_location(node, startToken.location);
}

void ScriptParser::with_parent_context(const std::shared_ptr<AstNode>& newParentNode, const std::function<void()>& parserFunc)
{
    auto oldParent = m_currentParentNode;
    m_currentParentNode = newParentNode;
    parserFunc();
    m_currentParentNode = oldParent;
}

// --- Simplified Parsing Rules ---

std::shared_ptr<CompilationUnitNode> ScriptParser::parse_compilation_unit()
{
    auto unitNode = make_ast_node<CompilationUnitNode>();
    SourceLocation unitStartLocation = current_token().location;

    with_parent_context(unitNode, [&]() {
        // Removed using directives and file-scoped namespaces
        while (!is_at_end())
        {
            // Expecting type declarations (class) directly
            if (check_token({TokenType::Public, TokenType::Private, TokenType::Static, TokenType::Class})) // Simplified set
            {
                 // parse_file_level_declaration will now return TypeDeclarationNode
                 // Assuming CompilationUnitNode::members is now std::vector<std::shared_ptr<TypeDeclarationNode>>
                 unitNode->members.push_back(parse_file_level_declaration(current_token()));
            }
            else if (is_at_end())
            {
                break;
            }
            else
            {
                throw create_error("Unexpected token at file level: '" + current_token().lexeme + "' (" + token_type_to_string(current_token().type) + "). Expected class declaration.");
            }
        }
    });

    finalize_node_location(unitNode, unitStartLocation);
    return unitNode;
}

std::string ScriptParser::parse_qualified_identifier(const std::string& contextMessageStart, const std::string& contextMessagePart)
{
    std::string qualifiedName = parse_identifier_name(contextMessageStart);
    while (match_token(TokenType::Dot))
    {
        qualifiedName += ".";
        qualifiedName += parse_identifier_name(contextMessagePart);
    }
    return qualifiedName;
}

std::string ScriptParser::parse_identifier_name(const std::string& contextMessage)
{
    return consume_token(TokenType::Identifier, contextMessage).lexeme;
}

std::vector<ModifierKind> ScriptParser::parse_modifiers()
{
    std::vector<ModifierKind> mods;
    while (true)
    {
        if (match_token(TokenType::Public)) { mods.push_back(ModifierKind::Public); }
        else if (match_token(TokenType::Private)) { mods.push_back(ModifierKind::Private); }
        // Removed Protected, Internal
        else if (match_token(TokenType::Static)) { mods.push_back(ModifierKind::Static); }
        // Removed Readonly
        else { break; }
    }
    return mods;
}

// Changed return type and focus
std::shared_ptr<TypeDeclarationNode> ScriptParser::parse_file_level_declaration(const Token& declarationStartToken)
{
    std::vector<ModifierKind> modifiers = parse_modifiers();
    Token keywordToken = current_token();

    if (keywordToken.type == TokenType::Class)
    {
        // Directly call parse_class_declaration, no need for intermediate parse_type_declaration for struct
        Token consumedKeywordToken = advance_token(); // Consume 'class'
        std::string name = parse_identifier_name("Expected class name after 'class'.");
        return parse_class_declaration(std::move(modifiers), consumedKeywordToken, name, declarationStartToken);
    }
    // Removed struct and namespace parsing here
    else
    {
        throw create_error(declarationStartToken, "Expected class declaration at file level.");
    }
}


ParsedDeclarationParts ScriptParser::parse_variable_declaration_parts()
{
    ParsedDeclarationParts parts;
    Token declPartStartToken = current_token();
    parts.startLocation = declPartStartToken.location;

    // Removed 'var' keyword support
    parts.type = parse_type_name();
    parts.declarators = parse_variable_declarator_list();
    return parts;
}

std::shared_ptr<ClassDeclarationNode> ScriptParser::parse_class_declaration(
    std::vector<ModifierKind> modifiers,
    const Token& /*classKeywordToken*/,
    const std::string& className,
    const Token& actualStartToken)
{
    auto node = make_ast_node<ClassDeclarationNode>();
    node->modifiers = std::move(modifiers);
    node->name = className;
    node->location = actualStartToken.location;

    // Removed type parameters (generics) and base list (inheritance)
    // node->typeParameters = parse_optional_type_parameter_list(); // REMOVED
    // if (match_token(TokenType::Colon)) { node->baseList = parse_base_list(); } // REMOVED

    consume_token(TokenType::OpenBrace, "Expected '{' to open class body for '" + className + "'.");
    with_parent_context(node, [&]() {
        while (!check_token(TokenType::CloseBrace) && !is_at_end())
        {
            node->members.push_back(parse_member_declaration(current_token(), node->name));
        }
    });
    consume_token(TokenType::CloseBrace, "Expected '}' to close class body for '" + className + "'.");
    finalize_node_location(node, actualStartToken);
    return node;
}

std::shared_ptr<TypeNameNode> ScriptParser::parse_type_name()
{
    Token nameToken;
    // Simplified to Identifier or core primitive type keywords (int, bool, void)
    if (check_token(TokenType::Identifier) ||
        check_token({TokenType::Int, TokenType::Bool, TokenType::Void})) // Simplified list
    {
        nameToken = advance_token();
    }
    else
    {
        throw create_error("Expected type name identifier or primitive type keyword (int, bool, void).");
    }

    auto node = make_ast_node<TypeNameNode>();
    node->name = nameToken.lexeme;
    node->location = nameToken.location;

    // Removed generic type arguments (<...>)
    // Removed array rank specifiers ([])
    // Assuming TypeNameNode itself is simplified (e.g., no typeArguments, isArray members)

    finalize_node_location(node, nameToken);
    return node;
}

std::shared_ptr<MemberDeclarationNode> ScriptParser::parse_member_declaration(
    const Token& memberStartToken,
    const std::optional<std::string>& currentClassName)
{
    std::vector<ModifierKind> modifiers = parse_modifiers();
    Token firstSignificantTokenAfterMods = current_token();

    if (currentClassName.has_value() &&
        firstSignificantTokenAfterMods.type == TokenType::Identifier &&
        firstSignificantTokenAfterMods.lexeme == currentClassName.value() &&
        peek_token(1).type == TokenType::OpenParen)
    {
        Token constructorNameToken = advance_token();
        return parse_constructor_declaration(std::move(modifiers), currentClassName.value(), constructorNameToken, memberStartToken);
    }
    else
    {
        std::shared_ptr<TypeNameNode> memberType = parse_type_name();
        Token memberNameToken = current_token();

        if (memberNameToken.type != TokenType::Identifier)
        {
            throw create_error(memberNameToken, "Expected identifier for member name after type.");
        }
        std::string memberNameStr = memberNameToken.lexeme;

        // Simplified lookahead: just check for OpenParen for method
        if (peek_token(1).type == TokenType::OpenParen)
        {
            advance_token(); // Consume the method name identifier
            return parse_method_declaration(std::move(modifiers), memberType, memberNameStr, memberStartToken);
        }
        else // Field
        {
            return parse_field_declaration(std::move(modifiers), memberType, memberStartToken);
        }
    }
}

std::shared_ptr<FieldDeclarationNode> ScriptParser::parse_field_declaration(
    std::vector<ModifierKind> modifiers,
    std::shared_ptr<TypeNameNode> type,
    const Token& fieldDeclStartToken)
{
    auto node = make_ast_node<FieldDeclarationNode>();
    node->modifiers = std::move(modifiers);
    node->type = type;
    node->location = fieldDeclStartToken.location;

    node->declarators = parse_variable_declarator_list();

    consume_token(TokenType::Semicolon, "Expected ';' after field declaration.");
    finalize_node_location(node, fieldDeclStartToken);
    return node;
}

std::shared_ptr<MethodDeclarationNode> ScriptParser::parse_method_declaration(
    std::vector<ModifierKind> modifiers,
    std::shared_ptr<TypeNameNode> returnType,
    const std::string& methodName,
    const Token& methodDeclStartToken)
{
    auto node = make_ast_node<MethodDeclarationNode>();
    node->modifiers = std::move(modifiers);
    node->type = returnType;
    node->name = methodName;
    node->location = methodDeclStartToken.location;

    // node->typeParameters = parse_optional_type_parameter_list(); // REMOVED

    consume_token(TokenType::OpenParen, "Expected '(' for method '" + methodName + "' parameter list.");
    node->parameters = parse_parameter_list();

    // Simplified: methods must have a body. No abstract/interface methods.
    // if (match_token(TokenType::Semicolon)) { node->body = std::nullopt; } // REMOVED
    // else
    // {
        node->body = parse_block_statement();
    // }

    finalize_node_location(node, methodDeclStartToken);
    return node;
}

std::shared_ptr<ConstructorDeclarationNode> ScriptParser::parse_constructor_declaration(
    std::vector<ModifierKind> modifiers,
    const std::string& constructorName,
    const Token& /*constructorNameToken*/,
    const Token& actualStartToken)
{
    auto node = make_ast_node<ConstructorDeclarationNode>();
    node->modifiers = std::move(modifiers);
    node->name = constructorName;
    node->type = std::nullopt; // Constructors don't have a return type
    node->location = actualStartToken.location;

    consume_token(TokenType::OpenParen, "Expected '(' for constructor '" + constructorName + "' parameter list.");
    node->parameters = parse_parameter_list();

    node->body = parse_block_statement();

    finalize_node_location(node, actualStartToken);
    return node;
}

std::vector<std::shared_ptr<ParameterDeclarationNode>> ScriptParser::parse_parameter_list()
{
    std::vector<std::shared_ptr<ParameterDeclarationNode>> parameters;
    if (!check_token(TokenType::CloseParen))
    {
        do {
            parameters.push_back(parse_parameter_declaration());
        } while (match_token(TokenType::Comma));
    }
    consume_token(TokenType::CloseParen, "Expected ')' to close parameter list.");
    return parameters;
}

std::shared_ptr<ParameterDeclarationNode> ScriptParser::parse_parameter_declaration()
{
    Token firstTokenOfParam = current_token();
    auto node = make_ast_node<ParameterDeclarationNode>();
    node->location = firstTokenOfParam.location;

    node->type = parse_type_name();
    node->name = parse_identifier_name("Expected parameter name.");

    // node->defaultValue = ...; // REMOVED default parameter values

    finalize_node_location(node, firstTokenOfParam);
    return node;
}

std::vector<std::shared_ptr<VariableDeclaratorNode>> ScriptParser::parse_variable_declarator_list()
{
    std::vector<std::shared_ptr<VariableDeclaratorNode>> declarators;
    do {
        declarators.push_back(parse_variable_declarator());
    } while (match_token(TokenType::Comma));
    return declarators;
}

std::shared_ptr<VariableDeclaratorNode> ScriptParser::parse_variable_declarator()
{
    Token nameToken = consume_token(TokenType::Identifier, "Expected variable name in declarator.");
    auto node = make_ast_node<VariableDeclaratorNode>();
    node->name = nameToken.lexeme;
    node->location = nameToken.location;

    if (match_token(TokenType::Assign))
    {
        node->initializer = parse_expression();
    }
    finalize_node_location(node, nameToken);
    return node;
}

// --- Statements (Simplified) ---

std::shared_ptr<StatementNode> ScriptParser::parse_statement()
{
    if (check_token(TokenType::OpenBrace)) { return parse_block_statement(); }
    if (check_token(TokenType::Return)) { return parse_return_statement(); }
    if (check_token(TokenType::If)) { return parse_if_statement(); }
    // Removed While, For, ForEach, Break, Continue

    // Try to parse as local variable declaration
    // Simplified heuristic: (PrimitiveTypeKeyword | Identifier) followed by Identifier
    if (check_token({TokenType::Int, TokenType::Bool, TokenType::Void}) || // Primitive types
        (check_token(TokenType::Identifier) && // Custom type (class name)
         (peek_token(1).type == TokenType::Identifier || // MyType varName
          (peek_token(1).type == TokenType::Dot && peek_token(2).type == TokenType::Identifier) // MyNamespace.MyType varName
         )
        )
       )
    {
        size_t preParseIndex = m_currentIndex;
        std::shared_ptr<AstNode> tempParent = m_currentParentNode.lock();
        try {
            std::shared_ptr<TypeNameNode> potentialType;
            with_parent_context(tempParent, [&](){ potentialType = parse_type_name(); });

            // If after parsing a type, we see an identifier (variable name)
            // followed by typical declaration terminators/continuators, it's a declaration.
            if (check_token(TokenType::Identifier) &&
                (peek_token(1).type == TokenType::Semicolon ||
                 peek_token(1).type == TokenType::Assign ||
                 peek_token(1).type == TokenType::Comma)) {
                m_currentIndex = preParseIndex; // Backtrack
                return parse_local_variable_declaration_statement();
            }
        } catch (const ParseError&) {
            // Failed to parse as TypeName, so probably not a declaration start this way.
        }
        m_currentIndex = preParseIndex; // Backtrack if not confirmed
    }
    
    if (check_token(TokenType::Semicolon)) {
        throw create_error(current_token(), "Empty statements (';') are not supported. Expected expression or other statement.");
    }

    return parse_expression_statement(); // Default
}

std::shared_ptr<BlockStatementNode> ScriptParser::parse_block_statement()
{
    Token openBraceToken = consume_token(TokenType::OpenBrace, "Expected '{' to start a block statement.");
    auto node = make_ast_node<BlockStatementNode>();
    node->location = openBraceToken.location;

    with_parent_context(node, [&]() {
        while (!check_token(TokenType::CloseBrace) && !is_at_end())
        {
            node->statements.push_back(parse_statement());
        }
    });

    consume_token(TokenType::CloseBrace, "Expected '}' to close a block statement.");
    finalize_node_location(node, openBraceToken);
    return node;
}

std::shared_ptr<LocalVariableDeclarationStatementNode> ScriptParser::parse_local_variable_declaration_statement()
{
    // Token statementStartToken = current_token(); // Captured by parse_variable_declaration_parts
    ParsedDeclarationParts declParts = parse_variable_declaration_parts();

    auto node = make_ast_node<LocalVariableDeclarationStatementNode>();
    // node->isVarDeclaration = declParts.isVar; // REMOVED, var is gone
    node->type = declParts.type;
    node->declarators = declParts.declarators;
    node->location = declParts.startLocation;

    consume_token(TokenType::Semicolon, "Expected ';' after local variable declaration.");
    finalize_node_location(node, declParts.startLocation);
    return node;
}

std::shared_ptr<ExpressionStatementNode> ScriptParser::parse_expression_statement()
{
    Token expressionStartToken = current_token();
    auto node = make_ast_node<ExpressionStatementNode>();
    node->location = expressionStartToken.location;
    node->expression = parse_expression();
    consume_token(TokenType::Semicolon, "Expected ';' after expression statement.");
    finalize_node_location(node, expressionStartToken);
    return node;
}

std::shared_ptr<IfStatementNode> ScriptParser::parse_if_statement()
{
    Token ifKeywordToken = consume_token(TokenType::If, "Expected 'if' keyword.");
    auto node = make_ast_node<IfStatementNode>();
    node->location = ifKeywordToken.location;

    consume_token(TokenType::OpenParen, "Expected '(' after 'if' keyword.");
    node->condition = parse_expression(); // Condition can be any valid expression
    consume_token(TokenType::CloseParen, "Expected ')' after if condition.");

    node->thenStatement = parse_statement();

    if (match_token(TokenType::Else))
    {
        node->elseStatement = parse_statement();
    }
    else
    {
        node->elseStatement = std::nullopt;
    }
    finalize_node_location(node, ifKeywordToken);
    return node;
}

std::shared_ptr<ReturnStatementNode> ScriptParser::parse_return_statement()
{
    Token returnKeywordToken = consume_token(TokenType::Return, "Expected 'return' keyword.");
    auto node = make_ast_node<ReturnStatementNode>();
    node->location = returnKeywordToken.location;

    if (!check_token(TokenType::Semicolon))
    {
        node->expression = parse_expression();
    }
    else
    {
        node->expression = std::nullopt;
    }
    consume_token(TokenType::Semicolon, "Expected ';' after return statement.");
    finalize_node_location(node, returnKeywordToken);
    return node;
}


// --- Expressions (Simplified) ---

std::shared_ptr<ExpressionNode> ScriptParser::parse_expression()
{
    return parse_assignment_expression();
}

std::shared_ptr<ExpressionNode> ScriptParser::parse_assignment_expression()
{
    // For if conditions, we don't want to parse assignments, so start with equality
    std::shared_ptr<ExpressionNode> left = parse_equality_expression(); 

    // Only basic assignment '=' is kept
    if (check_token(TokenType::Assign))
    {
        Token operatorToken = advance_token();
        auto node = make_ast_node<AssignmentExpressionNode>();
        node->target = left;
        node->op = AssignmentOperator::Assign; // Simplified
        node->source = parse_assignment_expression(); // Right-associative
        if (left && left->location) finalize_node_location(node, *left->location);
        else finalize_node_location(node, operatorToken);
        return node;
    }
    return left;
}

// Removed parse_logical_or_expression and parse_logical_and_expression
// The chain now goes from assignment/equality to equality/relational.

std::shared_ptr<ExpressionNode> ScriptParser::parse_equality_expression()
{
    std::shared_ptr<ExpressionNode> left = parse_relational_expression();
    while (check_token({TokenType::EqualsEquals, TokenType::NotEquals}))
    {
        Token opToken = advance_token();
        std::shared_ptr<ExpressionNode> right = parse_relational_expression();
        auto node = make_ast_node<BinaryExpressionNode>();
        node->left = left;
        node->op = (opToken.type == TokenType::EqualsEquals) ? BinaryOperatorKind::Equals : BinaryOperatorKind::NotEquals;
        node->right = right;
        if (left && left->location) finalize_node_location(node, *left->location);
        else finalize_node_location(node, opToken);
        left = node;
    }
    return left;
}

std::shared_ptr<ExpressionNode> ScriptParser::parse_relational_expression()
{
    std::shared_ptr<ExpressionNode> left = parse_additive_expression();
    while (check_token({TokenType::LessThan, TokenType::GreaterThan, TokenType::LessThanOrEqual, TokenType::GreaterThanOrEqual}))
    {
        Token opToken = advance_token();
        // Removed can_parse_as_generic_method_arguments check as generics are removed
        std::shared_ptr<ExpressionNode> right = parse_additive_expression();
        auto node = make_ast_node<BinaryExpressionNode>();
        node->left = left;
        if (opToken.type == TokenType::LessThan) node->op = BinaryOperatorKind::LessThan;
        else if (opToken.type == TokenType::GreaterThan) node->op = BinaryOperatorKind::GreaterThan;
        else if (opToken.type == TokenType::LessThanOrEqual) node->op = BinaryOperatorKind::LessThanOrEqual;
        else node->op = BinaryOperatorKind::GreaterThanOrEqual;
        node->right = right;
        if (left && left->location) finalize_node_location(node, *left->location);
        else finalize_node_location(node, opToken);
        left = node;
    }
    return left;
}

std::shared_ptr<ExpressionNode> ScriptParser::parse_additive_expression()
{
    std::shared_ptr<ExpressionNode> left = parse_multiplicative_expression();
    while (check_token({TokenType::Plus, TokenType::Minus}))
    {
        Token opToken = advance_token();
        std::shared_ptr<ExpressionNode> right = parse_multiplicative_expression();
        auto node = make_ast_node<BinaryExpressionNode>();
        node->left = left;
        node->op = (opToken.type == TokenType::Plus) ? BinaryOperatorKind::Add : BinaryOperatorKind::Subtract;
        node->right = right;
        if (left && left->location) finalize_node_location(node, *left->location);
        else finalize_node_location(node, opToken);
        left = node;
    }
    return left;
}

std::shared_ptr<ExpressionNode> ScriptParser::parse_multiplicative_expression()
{
    std::shared_ptr<ExpressionNode> left = parse_unary_expression();
    while (check_token({TokenType::Asterisk, TokenType::Slash, TokenType::Percent}))
    {
        Token opToken = advance_token();
        std::shared_ptr<ExpressionNode> right = parse_unary_expression();
        auto node = make_ast_node<BinaryExpressionNode>();
        node->left = left;
        if (opToken.type == TokenType::Asterisk) node->op = BinaryOperatorKind::Multiply;
        else if (opToken.type == TokenType::Slash) node->op = BinaryOperatorKind::Divide;
        else node->op = BinaryOperatorKind::Modulo;
        node->right = right;
        if (left && left->location) finalize_node_location(node, *left->location);
        else finalize_node_location(node, opToken);
        left = node;
    }
    return left;
}

std::shared_ptr<ExpressionNode> ScriptParser::parse_unary_expression()
{
    // Simplified: Keep LogicalNot, UnaryMinus. Removed UnaryPlus, Pre/Post Increment/Decrement.
    if (check_token({TokenType::LogicalNot, TokenType::Minus}))
    {
        Token operatorToken = advance_token();
        auto node = make_ast_node<UnaryExpressionNode>();
        node->location = operatorToken.location;

        if (operatorToken.type == TokenType::LogicalNot) node->op = UnaryOperatorKind::LogicalNot;
        else if (operatorToken.type == TokenType::Minus) node->op = UnaryOperatorKind::UnaryMinus;
        else throw create_error(operatorToken, "Unhandled prefix unary operator."); // Should not happen

        node->operand = parse_unary_expression();
        finalize_node_location(node, operatorToken);
        return node;
    }
    return parse_postfix_expression();
}

std::shared_ptr<ExpressionNode> ScriptParser::parse_postfix_expression()
{
    std::shared_ptr<ExpressionNode> expression = parse_primary_expression();
    SourceLocation expressionStartLoc = (expression && expression->location) ? *expression->location : current_token().location;

    while (true)
    {
        if (match_token(TokenType::Dot)) // Member Access
        {
            auto accessNode = make_ast_node<MemberAccessExpressionNode>();
            accessNode->target = expression;
            accessNode->memberName = parse_identifier_name("Expected member name after '.'.");
            finalize_node_location(accessNode, expressionStartLoc);
            expression = accessNode;
        }
        // Removed generic method call parsing: expr<T>(args)
        else if (match_token(TokenType::OpenParen)) // Regular method call: expr(args)
        {
            auto callNode = make_ast_node<MethodCallExpressionNode>();
            callNode->target = expression;
            callNode->arguments = parse_argument_list(); // Consumes ')'
            finalize_node_location(callNode, expressionStartLoc);
            expression = callNode;
        }
        // Removed Postfix ++/--
        else
        {
            break;
        }
        expressionStartLoc = (expression && expression->location) ? *expression->location : current_token().location;
    }
    return expression;
}

std::shared_ptr<ExpressionNode> ScriptParser::parse_primary_expression()
{
    Token current = current_token();
    SourceLocation startLoc = current.location;

    // Simplified literals: Integer, Boolean. String could be added back if needed.
    if (match_token(TokenType::IntegerLiteral))
    {
        Token literalToken = previous_token();
        auto node = make_ast_node<LiteralExpressionNode>();
        node->value = literalToken.lexeme;
        node->kind = LiteralKind::Integer;
        finalize_node_location(node, literalToken);
        return node;
    }
    else if (match_token({TokenType::True, TokenType::False}))
    {
        Token boolToken = previous_token();
        auto node = make_ast_node<LiteralExpressionNode>();
        node->value = boolToken.lexeme;
        node->kind = LiteralKind::Boolean;
        finalize_node_location(node, boolToken);
        return node;
    }
    // Removed StringLiteral, CharLiteral, DoubleLiteral, Null
    else if (match_token(TokenType::Identifier))
    {
        Token identToken = previous_token();
        auto node = make_ast_node<IdentifierExpressionNode>();
        node->name = identToken.lexeme;
        finalize_node_location(node, identToken);
        return node;
    }
    else if (match_token(TokenType::This))
    {
        Token thisToken = previous_token();
        auto node = make_ast_node<ThisExpressionNode>();
        finalize_node_location(node, thisToken);
        return node;
    }
    else if (match_token(TokenType::OpenParen))
    {
        // Token openParenToken = previous_token(); // Not strictly needed if not creating ParenthesizedExpressionNode
        std::shared_ptr<ExpressionNode> expression = parse_expression();
        consume_token(TokenType::CloseParen, "Expected ')' after expression in parentheses.");
        return expression; // Parentheses only group
    }
    else if (check_token(TokenType::New))
    {
        return parse_object_creation_expression();
    }
    else
    {
        throw create_error(current, "Unexpected token in primary expression. Expected literal (integer, boolean), identifier, 'this', '(', or 'new'.");
    }
}

std::shared_ptr<ObjectCreationExpressionNode> ScriptParser::parse_object_creation_expression()
{
    Token newKeywordToken = consume_token(TokenType::New, "Expected 'new' keyword.");
    auto node = make_ast_node<ObjectCreationExpressionNode>();
    node->location = newKeywordToken.location;

    node->type = parse_type_name(); // Type to instantiate

    // Simplified: Must have parentheses for arguments, even if empty.
    // No object/collection initializers.
    consume_token(TokenType::OpenParen, "Expected '(' after type name in new expression.");
    node->arguments = parse_argument_list(); // Consumes ')'

    finalize_node_location(node, newKeywordToken);
    return node;
}

std::shared_ptr<ArgumentListNode> ScriptParser::parse_argument_list()
{
    Token startTokenForLocation = previous_token(); // This was the '('.
    auto node = make_ast_node<ArgumentListNode>();
    node->location = startTokenForLocation.location; // Location of the '('

    if (!check_token(TokenType::CloseParen))
    {
        do {
            node->arguments.push_back(parse_argument());
        } while (match_token(TokenType::Comma));
    }
    consume_token(TokenType::CloseParen, "Expected ')' to close argument list.");
    finalize_node_location(node, startTokenForLocation);
    return node;
}

std::shared_ptr<ArgumentNode> ScriptParser::parse_argument()
{
    Token firstTokenOfArgument = current_token();
    auto node = make_ast_node<ArgumentNode>();
    node->location = firstTokenOfArgument.location;

    // node->name = std::nullopt; // Simplified: no named arguments
    node->expression = parse_expression();
    finalize_node_location(node, firstTokenOfArgument);
    return node;
}

// Removed can_parse_as_generic_method_arguments
// Removed parse_optional_type_argument_list

} // namespace Mycelium::Scripting::Lang