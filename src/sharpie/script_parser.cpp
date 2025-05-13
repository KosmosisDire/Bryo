#include "script_parser.hpp"
#include "script_token.hpp"
#include <algorithm>
#include <iostream>
#include "platform.hpp"


namespace Mycelium::Scripting::Lang
{

ScriptParser::ScriptParser(const std::vector<Token>& tokens)
    : m_tokens(tokens), m_currentIndex(0)
{
    if (m_tokens.empty() || m_tokens.back().type != TokenType::EndOfFile)
    {
        // This might indicate an issue with tokenizer not appending EOF or an empty token list.
        // For robustness, a parser could append a synthetic EOF if missing, or throw.
        // For now, we assume tokenizer provides a valid list ending with EOF.
    }
}

bool ScriptParser::can_parse_as_generic_method_arguments() const
{
    if (!check_token(TokenType::LessThan)) {
        return false;
    }
    int peekOffset = 1;
    bool firstTypeArgumentParsed = false;
    while (true) {
        TokenType currentPeekType = peek_token(peekOffset).type;
        if (currentPeekType == TokenType::Identifier ||
            currentPeekType == TokenType::Int || currentPeekType == TokenType::String ||
            currentPeekType == TokenType::Bool || currentPeekType == TokenType::Void ||
            currentPeekType == TokenType::Long || currentPeekType == TokenType::Double ||
            currentPeekType == TokenType::Char)
        {
            peekOffset++;
            int typePartLookaheadLimit = peekOffset + 10; // Increased limit slightly
            while (peekOffset < typePartLookaheadLimit &&
                   peek_token(peekOffset).type != TokenType::Comma &&
                   peek_token(peekOffset).type != TokenType::GreaterThan &&
                   peek_token(peekOffset).type != TokenType::EndOfFile) {
                if (peek_token(peekOffset).type == TokenType::LessThan) {
                    peekOffset++;
                    int nestingLevel = 1;
                    while (nestingLevel > 0 && peekOffset < typePartLookaheadLimit && peek_token(peekOffset).type != TokenType::EndOfFile) {
                        if (peek_token(peekOffset).type == TokenType::LessThan) nestingLevel++;
                        else if (peek_token(peekOffset).type == TokenType::GreaterThan) nestingLevel--;
                        peekOffset++;
                    }
                    if (nestingLevel != 0) return false;
                }
                else if (peek_token(peekOffset).type == TokenType::OpenBracket) {
                    peekOffset++;
                    if (peek_token(peekOffset).type != TokenType::CloseBracket) return false;
                    peekOffset++;
                }
                else if (peek_token(peekOffset).type == TokenType::Dot) {
                     peekOffset++;
                     if(peek_token(peekOffset).type != TokenType::Identifier) return false;
                     peekOffset++;
                }
                else {
                    break;
                }
            }
            firstTypeArgumentParsed = true;
        } else {
            return false;
        }

        if (peek_token(peekOffset).type == TokenType::Comma) {
            peekOffset++;
        } else if (peek_token(peekOffset).type == TokenType::GreaterThan) {
            break;
        } else {
            return false;
        }
    }
    if (!firstTypeArgumentParsed) return false;
    if (peek_token(peekOffset).type != TokenType::GreaterThan) return false;
    peekOffset++;
    return peek_token(peekOffset).type == TokenType::OpenParen;
}

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
        // This should ideally not be called if m_currentIndex is 0,
        // or it means an advance hasn't happened yet.
        // Depending on usage, could return a special token or throw.
        // For finalize_node_location, it's used after an advance.
        throw std::logic_error("previous_token() called at the beginning of token stream or before first advance.");
    }
    // m_currentIndex points to the *next* token to be consumed.
    // So, previous_token is at m_currentIndex - 1.
    if (m_currentIndex -1 >= m_tokens.size()){ // Should not happen if EOF is last
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
    // Advances m_currentIndex if not at EOF. Returns the token that was *just consumed*.
    if (current_token().type != TokenType::EndOfFile)
    {
        m_currentIndex++;
    }
    // If current_token() was already EOF, m_currentIndex doesn't change.
    // The function returns m_tokens[m_currentIndex - 1] (or throws if m_currentIndex was 0).
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
    if (m_currentIndex >= m_tokens.size()) return false; // Should be caught by is_at_end typically
    return current_token().type == type;
}

bool ScriptParser::check_token(const std::vector<TokenType>& types) const
{
    if (m_currentIndex >= m_tokens.size()) return false;
    TokenType currentType = current_token().type;

    if (currentType == TokenType::At) // special case to debug the parser
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
    // If current_token() is available, use its location. Otherwise, use last token's location.
    if (m_currentIndex < m_tokens.size()) {
        return ParseError(message, current_token().location);
    } else if (!m_tokens.empty()) {
        return ParseError(message, m_tokens.back().location); // Likely EOF
    }
    // Fallback if tokens is empty (should not happen with valid tokenizer output)
    SourceLocation unknownLoc; // All zeros
    return ParseError(message, unknownLoc);
}

void ScriptParser::finalize_node_location(std::shared_ptr<AstNode> node, const SourceLocation& startLocation)
{
    if (!node) return;
    SourceLocation finalLocation = startLocation;
    if (m_currentIndex > 0 && (m_currentIndex -1) < m_tokens.size() )
    {
        const Token& lastConsumedToken = previous_token(); // Token that ended this node's span
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

std::shared_ptr<CompilationUnitNode> ScriptParser::parse_compilation_unit()
{
    auto unitNode = make_ast_node<CompilationUnitNode>();
    SourceLocation unitStartLocation = current_token().location; // Capture start

    with_parent_context(unitNode, [&]() {
        while (check_token(TokenType::Using))
        {
            unitNode->usings.push_back(parse_using_directive());
        }

        if (check_token(TokenType::Namespace))
        {
            unitNode->fileScopedNamespaceName = parse_file_scoped_namespace_directive();
        }

        while (!is_at_end())
        {
            if (check_token({TokenType::Public, TokenType::Private, TokenType::Protected, TokenType::Internal,
                             TokenType::Static, TokenType::Class, TokenType::Struct, TokenType::Namespace}))
            {
                 unitNode->members.push_back(parse_file_level_declaration(current_token()));
            }
            else if (is_at_end())
            {
                break;
            }
            else
            {
                throw create_error("Unexpected token at file level: '" + current_token().lexeme + "' (" + token_type_to_string(current_token().type) + ")");
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

std::shared_ptr<UsingDirectiveNode> ScriptParser::parse_using_directive()
{
    Token startToken = consume_token(TokenType::Using, "Expected 'using' keyword.");
    auto node = make_ast_node<UsingDirectiveNode>();
    node->location = startToken.location; // Set location from 'using'

    node->namespaceName = parse_qualified_identifier(
        "Expected namespace name component after 'using'.",
        "Expected identifier component after '.' in using directive."
    );

    consume_token(TokenType::Semicolon, "Expected ';' after using directive.");
    finalize_node_location(node, startToken);
    return node;
}

std::string ScriptParser::parse_file_scoped_namespace_directive()
{
    Token startToken = consume_token(TokenType::Namespace, "Expected 'namespace' keyword for file-scoped namespace.");
    std::string qualifiedNsName = parse_qualified_identifier(
        "Expected namespace name component after 'namespace'.",
        "Expected identifier component after '.' in namespace directive."
    );
    consume_token(TokenType::Semicolon, "Expected ';' after file-scoped namespace declaration.");
    return qualifiedNsName;
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
        else if (match_token(TokenType::Protected)) { mods.push_back(ModifierKind::Protected); }
        else if (match_token(TokenType::Internal)) { mods.push_back(ModifierKind::Internal); }
        else if (match_token(TokenType::Static)) { mods.push_back(ModifierKind::Static); }
        else if (match_token(TokenType::Readonly)) { mods.push_back(ModifierKind::Readonly); }
        else { break; }
    }
    return mods;
}

std::shared_ptr<NamespaceMemberDeclarationNode> ScriptParser::parse_file_level_declaration(const Token& declarationStartToken)
{
    std::vector<ModifierKind> modifiers = parse_modifiers();
    Token keywordToken = current_token(); // e.g., 'class', 'struct', 'namespace'

    if (keywordToken.type == TokenType::Class || keywordToken.type == TokenType::Struct)
    {
        return parse_type_declaration(std::move(modifiers), keywordToken, declarationStartToken);
    }
    else
    {
        throw create_error(declarationStartToken, "Expected class, struct, or namespace declaration at file level.");
    }
}

std::shared_ptr<TypeDeclarationNode> ScriptParser::parse_type_declaration(
    std::vector<ModifierKind> modifiers,
    const Token& keywordToken, /* 'class' or 'struct' */
    const Token& actualStartToken /* Modifier or keywordToken if no mods */)
{
    Token consumedKeywordToken = advance_token(); // Consume 'class' or 'struct'
    std::string name = parse_identifier_name("Expected type name after '" + consumedKeywordToken.lexeme + "'.");

    if (consumedKeywordToken.type == TokenType::Class)
    {
        return parse_class_declaration(std::move(modifiers), consumedKeywordToken, name, actualStartToken);
    }
    else if (consumedKeywordToken.type == TokenType::Struct)
    {
        return parse_struct_declaration(std::move(modifiers), consumedKeywordToken, name, actualStartToken);
    }
    else
    {
        throw create_error(consumedKeywordToken, "Internal parser error: Expected 'class' or 'struct' keyword.");
    }
}

ParsedDeclarationParts ScriptParser::parse_variable_declaration_parts()
{
    ParsedDeclarationParts parts;
    Token declPartStartToken = current_token();
    parts.startLocation = declPartStartToken.location;

    if (match_token(TokenType::Var))
    {
        parts.isVar = true;
        auto varTypeNode = make_ast_node<TypeNameNode>();
        varTypeNode->name = "var";
        finalize_node_location(varTypeNode, previous_token()); // Location of 'var' token
        parts.type = varTypeNode;
    }
    else
    {
        parts.isVar = false;
        parts.type = parse_type_name();
    }
    parts.declarators = parse_variable_declarator_list();
    return parts;
}

std::shared_ptr<ClassDeclarationNode> ScriptParser::parse_class_declaration(
    std::vector<ModifierKind> modifiers,
    const Token& /*classKeywordToken (used for lexeme before, now for reference if needed)*/,
    const std::string& className,
    const Token& actualStartToken)
{
    auto node = make_ast_node<ClassDeclarationNode>();
    node->modifiers = std::move(modifiers);
    node->name = className;
    node->location = actualStartToken.location;

    node->typeParameters = parse_optional_type_parameter_list();
    if (match_token(TokenType::Colon))
    {
        node->baseList = parse_base_list();
    }

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

std::shared_ptr<StructDeclarationNode> ScriptParser::parse_struct_declaration(
    std::vector<ModifierKind> modifiers,
    const Token& /*structKeywordToken*/,
    const std::string& structName,
    const Token& actualStartToken)
{
    auto node = make_ast_node<StructDeclarationNode>();
    node->modifiers = std::move(modifiers);
    node->name = structName;
    node->location = actualStartToken.location;

    node->typeParameters = parse_optional_type_parameter_list();
    if (match_token(TokenType::Colon))
    {
        node->baseList = parse_base_list();
    }

    consume_token(TokenType::OpenBrace, "Expected '{' to open struct body for '" + structName + "'.");
    with_parent_context(node, [&]() {
        while (!check_token(TokenType::CloseBrace) && !is_at_end())
        {
            node->members.push_back(parse_member_declaration(current_token(), node->name));
        }
    });
    consume_token(TokenType::CloseBrace, "Expected '}' to close struct body for '" + structName + "'.");
    finalize_node_location(node, actualStartToken);
    return node;
}

std::shared_ptr<TypeNameNode> ScriptParser::parse_type_name()
{
    Token nameToken;
    if (check_token(TokenType::Identifier) ||
        check_token({TokenType::Int, TokenType::String, TokenType::Bool, TokenType::Void, TokenType::Long, TokenType::Double, TokenType::Char}))
    {
        nameToken = advance_token();
    }
    else
    {
        throw create_error("Expected type name identifier or primitive type keyword.");
    }

    auto node = make_ast_node<TypeNameNode>();
    node->name = nameToken.lexeme;
    node->location = nameToken.location;

    if (check_token(TokenType::LessThan))
    {
        consume_token(TokenType::LessThan, "Expected '<' to start type argument list.");
        if (!check_token(TokenType::GreaterThan)) { // Not <>
            do
            {
                node->typeArguments.push_back(parse_type_name());
            } while (match_token(TokenType::Comma));
        }
        consume_token(TokenType::GreaterThan, "Expected '>' to close type argument list.");
    }

    while (match_token(TokenType::OpenBracket))
    {
        consume_token(TokenType::CloseBracket, "Expected ']' to close array rank specifier.");
        node->isArray = true; // Simplification: any '[]' makes it an array.
                              // Further ranks T[][] are parsed but AST only has one bool.
    }

    finalize_node_location(node, nameToken);
    return node;
}

std::shared_ptr<MemberDeclarationNode> ScriptParser::parse_member_declaration(
    const Token& memberStartToken, /* First token (modifier or type/name) */
    const std::optional<std::string>& currentClassName)
{
    std::vector<ModifierKind> modifiers = parse_modifiers();
    // After modifiers, current_token() is the type or constructor name.
    // If modifiers were present, memberStartToken is the first mod. If not, memberStartToken is current_token().
    // The 'actualStartToken' for the specific member parser should be memberStartToken.

    Token firstSignificantTokenAfterMods = current_token();

    if (currentClassName.has_value() &&
        firstSignificantTokenAfterMods.type == TokenType::Identifier &&
        firstSignificantTokenAfterMods.lexeme == currentClassName.value() &&
        peek_token(1).type == TokenType::OpenParen)
    {
        // Constructor: ClassName (...)
        Token constructorNameToken = advance_token(); // Consume ClassName
        return parse_constructor_declaration(std::move(modifiers), currentClassName.value(), constructorNameToken, memberStartToken);
    }
    else
    {
        // Field or Method
        std::shared_ptr<TypeNameNode> memberType = parse_type_name(); // Consumes the type
        Token memberNameToken = current_token(); // Now at potential member name

        if (memberNameToken.type != TokenType::Identifier)
        {
            throw create_error(memberNameToken, "Expected identifier for member name after type.");
        }
        std::string memberNameStr = memberNameToken.lexeme;

        // Look ahead for method: Name ( or Name <
        if (peek_token(1).type == TokenType::OpenParen || peek_token(1).type == TokenType::LessThan)
        {
            advance_token(); // Consume the method name identifier
            return parse_method_declaration(std::move(modifiers), memberType, memberNameStr, memberStartToken);
        }
        else // Field
        {
            // Field name (memberNameToken) is NOT consumed yet. parse_field_declaration will handle it.
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
    const Token& methodDeclStartToken) // First token of whole decl (modifier or return type)
{
    auto node = make_ast_node<MethodDeclarationNode>();
    node->modifiers = std::move(modifiers);
    node->type = returnType; // This is return type
    node->name = methodName;
    node->location = methodDeclStartToken.location;

    node->typeParameters = parse_optional_type_parameter_list();

    consume_token(TokenType::OpenParen, "Expected '(' for method '" + methodName + "' parameter list.");
    node->parameters = parse_parameter_list(); // Consumes ')'

    if (match_token(TokenType::Semicolon))
    {
        node->body = std::nullopt; // Abstract or interface method
    }
    else
    {
        // consume_token(TokenType::OpenBrace, "Expected '{' for method body or ';' for abstract method '" + methodName + "'.");
        node->body = parse_block_statement(); // parse_block_statement consumes '}'
    }

    finalize_node_location(node, methodDeclStartToken);
    return node;
}

std::shared_ptr<ConstructorDeclarationNode> ScriptParser::parse_constructor_declaration(
    std::vector<ModifierKind> modifiers,
    const std::string& constructorName,
    const Token& /*constructorNameToken (token for class name used as constructor name)*/,
    const Token& actualStartToken) // First token of whole decl (modifier or constructor name)
{
    auto node = make_ast_node<ConstructorDeclarationNode>();
    node->modifiers = std::move(modifiers);
    node->name = constructorName;
    node->type = std::nullopt; // Constructors don't have a return type in this field.
    node->location = actualStartToken.location;

    consume_token(TokenType::OpenParen, "Expected '(' for constructor '" + constructorName + "' parameter list.");
    node->parameters = parse_parameter_list(); // Consumes ')'

    // Constructor initializers (: base() / : this()) are not supported yet.

    // consume_token(TokenType::OpenBrace, "Expected '{' for constructor body for '" + constructorName + "'.");
    node->body = parse_block_statement(); // Consumes '}'

    finalize_node_location(node, actualStartToken);
    return node;
}

std::vector<std::shared_ptr<TypeParameterNode>> ScriptParser::parse_optional_type_parameter_list()
{
    std::vector<std::shared_ptr<TypeParameterNode>> typeParameters;
    if (match_token(TokenType::LessThan))
    {
        if (check_token(TokenType::GreaterThan)) {
            throw create_error(current_token(), "Type parameter list cannot be empty if '<>' is present.");
        }
        do {
            typeParameters.push_back(parse_type_parameter());
        } while (match_token(TokenType::Comma));
        consume_token(TokenType::GreaterThan, "Expected '>' to close type parameter list.");
    }
    return typeParameters;
}

std::shared_ptr<TypeParameterNode> ScriptParser::parse_type_parameter()
{
    Token nameToken = consume_token(TokenType::Identifier, "Expected type parameter name.");
    auto node = make_ast_node<TypeParameterNode>();
    node->name = nameToken.lexeme;
    finalize_node_location(node, nameToken); // Location of the identifier token
    // Constraints (where T : ...) not supported yet.
    return node;
}

std::vector<std::shared_ptr<ParameterDeclarationNode>> ScriptParser::parse_parameter_list()
{
    std::vector<std::shared_ptr<ParameterDeclarationNode>> parameters;
    // Opening '(' consumed by caller.
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
    // Parameter modifiers (ref, out, params) not supported yet.

    node->type = parse_type_name();
    node->name = parse_identifier_name("Expected parameter name.");

    if (match_token(TokenType::Assign))
    {
        node->defaultValue = parse_expression();
    }
    else
    {
        node->defaultValue = std::nullopt;
    }
    finalize_node_location(node, firstTokenOfParam);
    return node;
}

std::vector<std::shared_ptr<TypeNameNode>> ScriptParser::parse_base_list()
{
    // Colon ':' consumed by caller.
    std::vector<std::shared_ptr<TypeNameNode>> baseTypes;
    do {
        baseTypes.push_back(parse_type_name());
    } while (match_token(TokenType::Comma));
    return baseTypes;
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

// --- Statements ---

std::shared_ptr<StatementNode> ScriptParser::parse_statement()
{
    if (check_token(TokenType::OpenBrace)) { return parse_block_statement(); }
    if (check_token(TokenType::Return)) { return parse_return_statement(); }
    if (check_token(TokenType::If)) { return parse_if_statement(); }
    if (check_token(TokenType::While)) { return parse_while_statement(); }
    if (check_token(TokenType::For)) { return parse_for_statement(); }
    if (check_token(TokenType::ForEach)) { return parse_for_each_statement(); }
    if (check_token(TokenType::Break)) { return parse_break_statement(); }
    if (check_token(TokenType::Continue)) { return parse_continue_statement(); }

    // Try to parse as local variable declaration
    // Heuristic: 'var' or (PrimitiveType | Identifier followed by Identifier or < or [)
    if (check_token(TokenType::Var)) {
        return parse_local_variable_declaration_statement();
    }
    if (check_token({TokenType::Int, TokenType::String, TokenType::Bool, TokenType::Void, TokenType::Double, TokenType::Long, TokenType::Char}) ||
        (check_token(TokenType::Identifier) &&
         (peek_token(1).type == TokenType::Identifier ||
          peek_token(1).type == TokenType::LessThan ||
          peek_token(1).type == TokenType::OpenBracket ||
          (peek_token(1).type == TokenType::Dot && peek_token(2).type == TokenType::Identifier) // For qualified type name start
         )
        )
       )
    {
        // This is tricky. If it's "MyType.StaticMember.Method()", it's an expression.
        // If it's "MyType varName;", it's a declaration.
        // A more robust way is to try parsing as TypeName. If successful and followed by Identifier then (';' or '=' or ','),
        // it's likely a declaration. This requires more lookahead or tentative parsing.
        // For now, the existing heuristic is kept but it has known limitations.
        // A common C# parser strategy: if it *can* be parsed as a declaration, it is.
        // Let's try to parse as a type name, and if it's followed by an identifier and then typical declaration terminators, assume declaration.
        size_t preTypeNameParseIndex = m_currentIndex;
        std::shared_ptr<AstNode> tempParent = m_currentParentNode.lock(); // Dummy parent for tentative parse

        try {
            std::shared_ptr<TypeNameNode> potentialType;
            with_parent_context(tempParent, [&](){ potentialType = parse_type_name(); });

            if (check_token(TokenType::Identifier) &&
                (peek_token(1).type == TokenType::Semicolon ||
                 peek_token(1).type == TokenType::Assign ||
                 peek_token(1).type == TokenType::Comma)) {
                // Looks like a declaration. Reset and parse for real.
                m_currentIndex = preTypeNameParseIndex; // Backtrack
                return parse_local_variable_declaration_statement();
            }
        } catch (const ParseError&) {
            // Failed to parse as TypeName, so probably not a declaration start this way.
        }
        m_currentIndex = preTypeNameParseIndex; // Backtrack if not confirmed as declaration
    }

    if (check_token(TokenType::Semicolon)) {
        throw create_error(current_token(), "Empty statements (';') are not supported. Expected expression or other statement.");
    }

    return parse_expression_statement(); // Default to expression statement
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
    Token statementStartToken = current_token();
    ParsedDeclarationParts declParts = parse_variable_declaration_parts();

    auto node = make_ast_node<LocalVariableDeclarationStatementNode>();
    node->isVarDeclaration = declParts.isVar;
    node->type = declParts.type;
    node->declarators = declParts.declarators;
    node->location = declParts.startLocation; // Start of 'var' or TypeName

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
    node->condition = parse_expression();
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

std::shared_ptr<WhileStatementNode> ScriptParser::parse_while_statement()
{
    Token whileKeywordToken = consume_token(TokenType::While, "Expected 'while' keyword.");
    auto node = make_ast_node<WhileStatementNode>();
    node->location = whileKeywordToken.location;

    consume_token(TokenType::OpenParen, "Expected '(' after 'while' keyword.");
    node->condition = parse_expression();
    consume_token(TokenType::CloseParen, "Expected ')' after while condition.");
    node->body = parse_statement();
    finalize_node_location(node, whileKeywordToken);
    return node;
}

std::shared_ptr<ForStatementNode> ScriptParser::parse_for_statement()
{
    Token forKeywordToken = consume_token(TokenType::For, "Expected 'for' keyword.");
    auto node = make_ast_node<ForStatementNode>();
    node->location = forKeywordToken.location;

    consume_token(TokenType::OpenParen, "Expected '(' after 'for' keyword.");

    // Initializer part
    if (!check_token(TokenType::Semicolon))
    {
        // Heuristic: if 'var' or looks like a type, it's a declaration.
        if (check_token(TokenType::Var) ||
            check_token({TokenType::Int, TokenType::String, TokenType::Bool, TokenType::Void, TokenType::Double, TokenType::Long, TokenType::Char}) ||
            (check_token(TokenType::Identifier) && (peek_token(1).type == TokenType::Identifier || peek_token(1).type == TokenType::LessThan || peek_token(1).type == TokenType::OpenBracket)))
        {
            ParsedDeclarationParts declParts = parse_variable_declaration_parts();
            auto declStmtNode = make_ast_node<LocalVariableDeclarationStatementNode>();
            declStmtNode->isVarDeclaration = declParts.isVar;
            declStmtNode->type = declParts.type;
            declStmtNode->declarators = declParts.declarators;
            finalize_node_location(declStmtNode, declParts.startLocation);
            node->declaration = declStmtNode;
        }
        else // Expression list
        {
            node->declaration = std::nullopt;
            do {
                node->initializers.push_back(parse_expression());
            } while (match_token(TokenType::Comma));
        }
    }
    consume_token(TokenType::Semicolon, "Expected ';' after for loop initializer/declaration.");

    // Condition part
    if (!check_token(TokenType::Semicolon))
    {
        node->condition = parse_expression();
    }
    consume_token(TokenType::Semicolon, "Expected ';' after for loop condition.");

    // Incrementor part
    if (!check_token(TokenType::CloseParen))
    {
        do {
            node->incrementors.push_back(parse_expression());
        } while (match_token(TokenType::Comma));
    }
    consume_token(TokenType::CloseParen, "Expected ')' to close for loop header.");

    node->body = parse_statement();
    finalize_node_location(node, forKeywordToken);
    return node;
}

std::shared_ptr<ForEachStatementNode> ScriptParser::parse_for_each_statement()
{
    Token foreachKeywordToken = consume_token(TokenType::ForEach, "Expected 'foreach' keyword.");
    auto node = make_ast_node<ForEachStatementNode>();
    node->location = foreachKeywordToken.location;

    consume_token(TokenType::OpenParen, "Expected '(' after 'foreach' keyword.");

    Token varTypeStartToken = current_token();
    if (match_token(TokenType::Var))
    {
        auto varTypeNode = make_ast_node<TypeNameNode>();
        varTypeNode->name = "var";
        finalize_node_location(varTypeNode, previous_token());
        node->variableType = varTypeNode;
    }
    else
    {
        node->variableType = parse_type_name();
    }

    node->variableName = parse_identifier_name("Expected variable name in foreach loop.");
    consume_token(TokenType::In, "Expected 'in' keyword in foreach loop.");
    node->collection = parse_expression();
    consume_token(TokenType::CloseParen, "Expected ')' to close foreach loop header.");
    node->body = parse_statement();
    finalize_node_location(node, foreachKeywordToken);
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

std::shared_ptr<BreakStatementNode> ScriptParser::parse_break_statement()
{
    Token breakKeywordToken = consume_token(TokenType::Break, "Expected 'break' keyword.");
    auto node = make_ast_node<BreakStatementNode>();
    node->location = breakKeywordToken.location;
    consume_token(TokenType::Semicolon, "Expected ';' after 'break' statement.");
    finalize_node_location(node, breakKeywordToken);
    return node;
}

std::shared_ptr<ContinueStatementNode> ScriptParser::parse_continue_statement()
{
    Token continueKeywordToken = consume_token(TokenType::Continue, "Expected 'continue' keyword.");
    auto node = make_ast_node<ContinueStatementNode>();
    node->location = continueKeywordToken.location;
    consume_token(TokenType::Semicolon, "Expected ';' after 'continue' statement.");
    finalize_node_location(node, continueKeywordToken);
    return node;
}

// --- Expressions (Pratt Parser Style - Precedence Climbing) ---

std::shared_ptr<ExpressionNode> ScriptParser::parse_expression()
{
    return parse_assignment_expression(); // Lowest precedence
}

std::shared_ptr<ExpressionNode> ScriptParser::parse_assignment_expression()
{
    std::shared_ptr<ExpressionNode> left = parse_logical_or_expression();
    if (check_token({TokenType::Assign, TokenType::PlusAssign, TokenType::MinusAssign,
                     TokenType::AsteriskAssign, TokenType::SlashAssign, TokenType::PercentAssign}))
    {
        Token operatorToken = advance_token();
        auto node = make_ast_node<AssignmentExpressionNode>();
        node->target = left;
        switch (operatorToken.type) {
            case TokenType::Assign:         node->op = AssignmentOperator::Assign; break;
            case TokenType::PlusAssign:     node->op = AssignmentOperator::AddAssign; break;
            case TokenType::MinusAssign:    node->op = AssignmentOperator::SubtractAssign; break;
            case TokenType::AsteriskAssign: node->op = AssignmentOperator::MultiplyAssign; break;
            case TokenType::SlashAssign:    node->op = AssignmentOperator::DivideAssign; break;
            // case TokenType::PercentAssign: node->op = AssignmentOperator::ModuloAssign; // Add if needed
            default: throw create_error(operatorToken, "Unhandled assignment operator.");
        }
        node->source = parse_assignment_expression(); // Right-associative
        if (left && left->location) finalize_node_location(node, *left->location);
        else finalize_node_location(node, operatorToken);
        return node;
    }
    return left;
}

std::shared_ptr<ExpressionNode> ScriptParser::parse_logical_or_expression()
{
    std::shared_ptr<ExpressionNode> left = parse_logical_and_expression();
    while (match_token(TokenType::LogicalOr))
    {
        Token opToken = previous_token();
        std::shared_ptr<ExpressionNode> right = parse_logical_and_expression();
        auto node = make_ast_node<BinaryExpressionNode>();
        node->left = left;
        node->op = BinaryOperatorKind::LogicalOr;
        node->right = right;
        if (left && left->location) finalize_node_location(node, *left->location);
        else finalize_node_location(node, opToken); // Fallback
        left = node;
    }
    return left;
}

std::shared_ptr<ExpressionNode> ScriptParser::parse_logical_and_expression()
{
    std::shared_ptr<ExpressionNode> left = parse_equality_expression();
    while (match_token(TokenType::LogicalAnd))
    {
        Token opToken = previous_token();
        std::shared_ptr<ExpressionNode> right = parse_equality_expression();
        auto node = make_ast_node<BinaryExpressionNode>();
        node->left = left;
        node->op = BinaryOperatorKind::LogicalAnd;
        node->right = right;
        if (left && left->location) finalize_node_location(node, *left->location);
        else finalize_node_location(node, opToken);
        left = node;
    }
    return left;
}

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
        // Special check for generic type arguments vs. relational operators
        // If `left` is an Identifier or MemberAccess that could be a generic type/method name,
        // and we just consumed '<', and it looks like `Foo<Bar>` rather than `a < b`.
        if (opToken.type == TokenType::LessThan && can_parse_as_generic_method_arguments()) {
            // This `<` is likely part of a generic construct, not a relational operator.
            // We need to "put back" the token and let postfix_expression handle it.
            // This is complex because advance_token already moved m_currentIndex.
            // For now, the can_parse_as_generic_method_arguments is used in postfix.
            // Here, we assume if we reach this point, it's a relational op unless postfix already took it.
            // The primary ambiguity point is handled in parse_postfix_expression.
            // If we are here, it's likely a relational operator.
        }

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
    if (check_token({TokenType::LogicalNot, TokenType::Plus, TokenType::Minus, TokenType::Increment, TokenType::Decrement}))
    {
        Token operatorToken = advance_token(); // Consume prefix operator
        auto node = make_ast_node<UnaryExpressionNode>();
        node->location = operatorToken.location;

        if (operatorToken.type == TokenType::LogicalNot) node->op = UnaryOperatorKind::LogicalNot;
        else if (operatorToken.type == TokenType::Plus) node->op = UnaryOperatorKind::UnaryPlus;
        else if (operatorToken.type == TokenType::Minus) node->op = UnaryOperatorKind::UnaryMinus;
        else if (operatorToken.type == TokenType::Increment) node->op = UnaryOperatorKind::PreIncrement;
        else if (operatorToken.type == TokenType::Decrement) node->op = UnaryOperatorKind::PreDecrement;
        else throw create_error(operatorToken, "Unhandled prefix unary operator.");

        node->operand = parse_unary_expression(); // Right-recursive for stacked unary ops
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
        else if (check_token(TokenType::LessThan) && can_parse_as_generic_method_arguments()) // Generic method call: expr<T>(args)
        {
            auto typeArgs = parse_optional_type_argument_list(); // Consumes <...>
            if (!check_token(TokenType::OpenParen)) {
                throw create_error("Expected '(' after generic type arguments in method call.");
            }
            consume_token(TokenType::OpenParen, "Expected '(' after generic type arguments.");
            auto callNode = make_ast_node<MethodCallExpressionNode>();
            callNode->target = expression;
            callNode->typeArguments = typeArgs;
            callNode->arguments = parse_argument_list(); // Consumes ')'
            finalize_node_location(callNode, expressionStartLoc);
            expression = callNode;
        }
        else if (match_token(TokenType::OpenParen)) // Regular method call: expr(args)
        {
            auto callNode = make_ast_node<MethodCallExpressionNode>();
            callNode->target = expression;
            callNode->arguments = parse_argument_list(); // Consumes ')'
            finalize_node_location(callNode, expressionStartLoc);
            expression = callNode;
        }
        else if (check_token({TokenType::Increment, TokenType::Decrement})) // Postfix ++/--
        {
            Token opToken = advance_token();
            auto unaryNode = make_ast_node<UnaryExpressionNode>();
            unaryNode->operand = expression;
            unaryNode->op = (opToken.type == TokenType::Increment) ? UnaryOperatorKind::PostIncrement : UnaryOperatorKind::PostDecrement;
            finalize_node_location(unaryNode, expressionStartLoc);
            expression = unaryNode;
        }
        else
        {
            break; // No more postfix operators
        }
        // Update start location for next potential postfix operation
        expressionStartLoc = (expression && expression->location) ? *expression->location : current_token().location;
    }
    return expression;
}

std::shared_ptr<ExpressionNode> ScriptParser::parse_primary_expression()
{
    Token current = current_token();
    SourceLocation startLoc = current.location;

    if (match_token({TokenType::IntegerLiteral, TokenType::DoubleLiteral, TokenType::StringLiteral, TokenType::CharLiteral}))
    {
        Token literalToken = previous_token();
        auto node = make_ast_node<LiteralExpressionNode>();
        node->value = literalToken.lexeme;
        if (literalToken.type == TokenType::IntegerLiteral) node->kind = LiteralKind::Integer;
        else if (literalToken.type == TokenType::DoubleLiteral) node->kind = LiteralKind::Float;
        else if (literalToken.type == TokenType::StringLiteral) node->kind = LiteralKind::String;
        else node->kind = LiteralKind::Char;
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
    else if (match_token(TokenType::Null))
    {
        Token nullToken = previous_token();
        auto node = make_ast_node<LiteralExpressionNode>();
        node->value = "null";
        node->kind = LiteralKind::Null;
        finalize_node_location(node, nullToken);
        return node;
    }
    else if (match_token(TokenType::Identifier)) // allow built in types to be used like static classes
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
        Token openParenToken = previous_token();
        std::shared_ptr<ExpressionNode> expression = parse_expression();
        consume_token(TokenType::CloseParen, "Expected ')' after expression in parentheses.");
        // Parentheses only group, so return the inner expression.
        // If a ParenthesizedExpressionNode were used, its location would span the parens.
        // For now, location of inner expression is preserved.
        return expression;
    }
    else if (check_token(TokenType::New))
    {
        return parse_object_creation_expression();
    }
    else
    {
        throw create_error(current, "Unexpected token in primary expression. Expected literal, identifier, 'this', '(', or 'new'.");
    }
}

std::shared_ptr<ObjectCreationExpressionNode> ScriptParser::parse_object_creation_expression()
{
    Token newKeywordToken = consume_token(TokenType::New, "Expected 'new' keyword.");
    auto node = make_ast_node<ObjectCreationExpressionNode>();
    node->location = newKeywordToken.location;

    node->type = parse_type_name();

    if (match_token(TokenType::OpenParen))
    {
        node->arguments = parse_argument_list(); // Consumes ')'
    }
    else
    {
        // C# requires `()` for object creation, even parameterless.
        // If language allows `new Type` without `()`, then arguments remains nullopt.
        // For strict C#-like, this might be an error if not followed by `(`.
        // For now, if no '(', arguments is nullopt. Semantic analysis can check constructor.
        // This could be a point of refinement if strict C# `()` requirement is enforced.
        node->arguments = std::nullopt;
    }
    // Object/collection initializers `{ Prop = val }` not supported yet.
    finalize_node_location(node, newKeywordToken);
    return node;
}

std::optional<std::vector<std::shared_ptr<TypeNameNode>>> ScriptParser::parse_optional_type_argument_list()
{
    // Assumes caller (e.g., parse_postfix_expression) has determined this is likely type args.
    if (match_token(TokenType::LessThan))
    {
        std::vector<std::shared_ptr<TypeNameNode>> typeArguments;
        if (!check_token(TokenType::GreaterThan)) { // Not <>
            do {
                typeArguments.push_back(parse_type_name());
            } while (match_token(TokenType::Comma));
        }
        consume_token(TokenType::GreaterThan, "Expected '>' to close explicit type argument list.");
        return typeArguments;
    }
    return std::nullopt;
}

std::shared_ptr<ArgumentListNode> ScriptParser::parse_argument_list()
{
    // Opening '(' consumed by caller.
    Token startTokenForLocation = previous_token(); // This was the '('.
    auto node = make_ast_node<ArgumentListNode>();
    node->location = startTokenForLocation.location;

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

    // Named arguments `name:` not supported yet.
    node->name = std::nullopt;
    node->expression = parse_expression();
    finalize_node_location(node, firstTokenOfArgument);
    return node;
}

} // namespace Mycelium::Scripting::Lang
