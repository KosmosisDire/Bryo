#include "script_parser.hpp"
#include "script_token.hpp" 

#include <algorithm> 
#include <iostream>  

namespace Mycelium::Scripting::Lang
{

ScriptParser::ScriptParser(const std::vector<Token>& tokens)
    : m_tokens(tokens), m_currentIndex(0)
{
    if (m_tokens.empty() || m_tokens.back().type != TokenType::EndOfFile)
    {
    }
}

bool ScriptParser::can_parse_as_generic_method_arguments() const
{
    // This function is called when current_token() is '<'.
    // We peek ahead to see if it forms a plausible < TypeName,...,TypeName > ( sequence.
    // This is a non-consuming "trial parse" using peek_token.

    if (!check_token(TokenType::LessThan)) {
        // This should ideally not be hit if called when current_token is '<'.
        // If it is, something is wrong with the calling logic.
        return false; 
    }

    int peekOffset = 1; // Start peeking at the token *after* '<'

    // Phase 1: Try to parse one or more TypeNames separated by commas.
    bool firstTypeArgumentParsed = false;
    while (true) {
        // Try to identify the start of a TypeName (Identifier or primitive type)
        TokenType currentPeekType = peek_token(peekOffset).type;
        if (currentPeekType == TokenType::Identifier ||
            currentPeekType == TokenType::Int || currentPeekType == TokenType::String ||
            currentPeekType == TokenType::Bool || currentPeekType == TokenType::Void ||
            currentPeekType == TokenType::Long || currentPeekType == TokenType::Double ||
            currentPeekType == TokenType::Char) 
        {
            peekOffset++; // Consume the identifier/primitive that starts the type name

            // Simplified: Skip potential qualified parts or inner generics/arrays for this lookahead.
            // A truly robust lookahead here would recursively "try" to parse a full TypeName.
            // For this heuristic, we'll be more lenient after the initial identifier.
            // We are mainly looking for the gross structure: < type, type > (
            
            // Skip parts of a potentially complex type name (e.g., '.IDENT', '<...>', '[]')
            // This loop is a very rough approximation.
            int typePartLookaheadLimit = peekOffset + 5; // Limit inner skipping
            while (peekOffset < typePartLookaheadLimit && peek_token(peekOffset).type != TokenType::Comma && peek_token(peekOffset).type != TokenType::GreaterThan && peek_token(peekOffset).type != TokenType::EndOfFile) {
                // If we see another '<', it could be nested generics. Skip until matching '>'.
                if (peek_token(peekOffset).type == TokenType::LessThan) {
                    peekOffset++; // consume '<'
                    int nestingLevel = 1;
                    while (nestingLevel > 0 && peekOffset < typePartLookaheadLimit && peek_token(peekOffset).type != TokenType::EndOfFile) {
                        if (peek_token(peekOffset).type == TokenType::LessThan) nestingLevel++;
                        else if (peek_token(peekOffset).type == TokenType::GreaterThan) nestingLevel--;
                        peekOffset++;
                    }
                    if (nestingLevel != 0) return false; // Mismatched inner generics
                } 
                // Skip array brackets
                else if (peek_token(peekOffset).type == TokenType::OpenBracket) {
                    peekOffset++; // consume '['
                    if (peek_token(peekOffset).type != TokenType::CloseBracket) return false; // Expected ']'
                    peekOffset++; // consume ']'
                }
                // Skip qualified name access
                else if (peek_token(peekOffset).type == TokenType::Dot) {
                     peekOffset++; // consume '.'
                     if(peek_token(peekOffset).type != TokenType::Identifier) return false; // Expected identifier after dot
                     peekOffset++; // consume identifier
                }
                else {
                    // Some other token that doesn't continue a complex type name in a simple way
                    // Or it's just a simple identifier type.
                    // This simplified lookahead might break on extremely complex types here.
                    // We'll assume it's the end of this particular type name part for now.
                    break; 
                }
            }
            firstTypeArgumentParsed = true;
        } else {
            // Not starting with an identifier or primitive, so not a type argument.
            return false; 
        }

        // After a type argument, expect either a comma or '>'
        if (peek_token(peekOffset).type == TokenType::Comma) {
            peekOffset++; // Consume comma, look for next type argument
        } else if (peek_token(peekOffset).type == TokenType::GreaterThan) {
            break; // End of type argument list
        } else {
            return false; // Unexpected token in type argument list
        }
    }

    if (!firstTypeArgumentParsed) {
        return false; // e.g. `< >` without arguments, or `< , ...`
    }

    // At this point, peek_token(peekOffset) should be '>'
    if (peek_token(peekOffset).type != TokenType::GreaterThan) {
        return false; // Should have broken from loop on '>'
    }
    peekOffset++; // Consume '>'

    // After the closing '>', we MUST find an '(' for it to be a generic method call signature.
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
        return m_tokens.back();
    }
    return m_tokens[targetIndex];
}

const Token& ScriptParser::current_token() const
{
    if (m_currentIndex >= m_tokens.size()) {
        return m_tokens.back(); 
    }
    return m_tokens[m_currentIndex];
}

const Token& ScriptParser::previous_token() const
{
    if (m_currentIndex == 0)
    {
        throw std::logic_error("previous_token() called at the beginning of token stream or before first advance.");
    }
    if (m_currentIndex -1 >= m_tokens.size()) {
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
    if (m_currentIndex < m_tokens.size() && current_token().type != TokenType::EndOfFile)
    {
        m_currentIndex++;
    } else if (m_currentIndex < m_tokens.size() -1 && current_token().type == TokenType::EndOfFile) {
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
    return std::any_of(types.begin(), types.end(),
                       [currentType](TokenType t){ return t == currentType; });
}

ParseError ScriptParser::create_error(const Token& token, const std::string& message)
{
    return ParseError(message, token.location);
}
ParseError ScriptParser::create_error(const std::string& message)
{
    if (m_currentIndex >= m_tokens.size()) {
        return ParseError(message, m_tokens.back().location);
    }
    return ParseError(message, current_token().location);
}


void ScriptParser::finalize_node_location(std::shared_ptr<AstNode> node, const SourceLocation& startLocation)
{
    if (!node) return;
    SourceLocation finalLocation = startLocation;
    if (m_currentIndex > 0 && (m_currentIndex -1) < m_tokens.size() ) 
    {
        const Token& lastConsumed = previous_token(); 
        finalLocation.line_end = lastConsumed.location.line_end;
        finalLocation.column_end = lastConsumed.location.column_end;
    }
    node->location = finalLocation;
}

void ScriptParser::finalize_node_location(std::shared_ptr<AstNode> node, const Token& startToken)
{
    finalize_node_location(node, startToken.location);
}


std::shared_ptr<CompilationUnitNode> ScriptParser::parse_compilation_unit()
{
    auto unitNode = make_ast_node<CompilationUnitNode>();
    SourceLocation unitStartLocation = current_token().location;

    auto oldParent = m_currentParentNode;
    m_currentParentNode = unitNode;

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
                         TokenType::Static, TokenType::Class, TokenType::Struct})) 
        {
             unitNode->members.push_back(parse_file_level_declaration());
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

    m_currentParentNode = oldParent;
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

std::shared_ptr<NamespaceMemberDeclarationNode> ScriptParser::parse_file_level_declaration()
{
    Token startOfEntireDeclaration = current_token(); // This captures the very first token (modifier or keyword)
    std::vector<ModifierKind> modifiers = parse_modifiers();

    Token keywordToken = current_token(); // This is the 'class', or 'struct' token

    if (keywordToken.type == TokenType::Class || keywordToken.type == TokenType::Struct)
    {
        // parse_type_declaration expects to be AT the keywordToken.
        // It will consume it and parse the name, then call the specific class/struct parser.
        // The specific class/struct parser will set its own location from the keywordToken.
        auto typeNode = parse_type_declaration(std::move(modifiers), keywordToken);
        
        // Now, typeNode's location starts from 'class' or 'struct'.
        // We want its location to span from 'startOfEntireDeclaration'.
        if (typeNode && typeNode->location.has_value()) {
            // Create a new SourceLocation that uses the earlier start but the existing end.
            SourceLocation finalLoc = typeNode->location.value();
            if (startOfEntireDeclaration.location.line_start < finalLoc.line_start ||
                (startOfEntireDeclaration.location.line_start == finalLoc.line_start &&
                 startOfEntireDeclaration.location.column_start < finalLoc.column_start))
            {
                finalLoc.line_start = startOfEntireDeclaration.location.line_start;
                finalLoc.column_start = startOfEntireDeclaration.location.column_start;
            }
            typeNode->location = finalLoc; // Update with the true start
        } else if (typeNode) { // If location wasn't set but node exists
            finalize_node_location(typeNode, startOfEntireDeclaration);
        }
        return typeNode;
    }
    else
    {
        throw create_error(startOfEntireDeclaration, "Expected class, struct, enum, or interface declaration at file level.");
    }
}

std::shared_ptr<TypeDeclarationNode> ScriptParser::parse_type_declaration(
    std::vector<ModifierKind> modifiers, 
    const Token& keywordToken /* 'class' or 'struct', already at this token */)
{
    // keywordToken is 'class' or 'struct'. We need to consume it.
    Token consumedKeywordToken = advance_token(); // Consume 'class' or 'struct'

    std::string name = parse_identifier_name("Expected type name after '" + consumedKeywordToken.lexeme + "'.");

    if (consumedKeywordToken.type == TokenType::Class)
    {
        return parse_class_declaration(std::move(modifiers), consumedKeywordToken, name);
    }
    else if (consumedKeywordToken.type == TokenType::Struct)
    {
        return parse_struct_declaration(std::move(modifiers), consumedKeywordToken, name);
    }
    else
    {
        // This case should ideally not be reached if the caller checks correctly.
        throw create_error(consumedKeywordToken, "Internal parser error: Expected 'class' or 'struct' keyword in parse_type_declaration.");
    }
}

Mycelium::Scripting::Lang::ParsedDeclarationParts ScriptParser::parse_variable_declaration_parts(bool isForLoopContext /*= false*/)
{
    Mycelium::Scripting::Lang::ParsedDeclarationParts parts;
    Token declPartStartToken = current_token();
    parts.startLocation = declPartStartToken.location;

    if (match_token(TokenType::Var))
    {
        parts.isVar = true;
        auto varTypeNode = make_ast_node<TypeNameNode>(); 
        varTypeNode->name = "var"; // Special name
        finalize_node_location(varTypeNode, previous_token());
        parts.type = varTypeNode;
    }
    else
    {
        parts.isVar = false;
        parts.type = parse_type_name();
    }

    // `parse_variable_declarator_list` will parse one or more declarators separated by commas.
    // It internally calls `parse_variable_declarator`.
    parts.declarators = parse_variable_declarator_list(parts.type);

    // The semicolon is handled by the caller (parse_local_variable_declaration_statement or parse_for_statement)
    return parts;
}

std::shared_ptr<ClassDeclarationNode> ScriptParser::parse_class_declaration(
    std::vector<ModifierKind> modifiers,
    const Token& classKeywordToken, 
    const std::string& className)
{
    auto node = make_ast_node<ClassDeclarationNode>();
    node->modifiers = std::move(modifiers);
    node->name = className;
    node->location = classKeywordToken.location; // Initial location from 'class' keyword

    // M9: Parse optional type parameters <T, U>
    node->typeParameters = parse_optional_type_parameter_list();

    // M9: Parse optional base list : BaseClass, IInterface
    if (match_token(TokenType::Colon)) // Use match_token as ':' is optional
    {
        node->baseList = parse_base_list();
    }

    consume_token(TokenType::OpenBrace, "Expected '{' to open class body.");

    auto oldParent = m_currentParentNode;
    m_currentParentNode = node;

    while (!check_token(TokenType::CloseBrace) && !is_at_end())
    {
        node->members.push_back(parse_member_declaration(current_token(), node->name));
    }
    m_currentParentNode = oldParent;

    consume_token(TokenType::CloseBrace, "Expected '}' to close class body.");
    // The final location should ideally span from the first modifier (or 'class' keyword)
    // to the closing '}'. This is tricky because classKeywordToken is just 'class'.
    // The caller `parse_file_level_declaration` is responsible for the true start if modifiers exist.
    // `finalize_node_location` here uses `classKeywordToken` as its start reference.
    finalize_node_location(node, classKeywordToken);
    return node;
}

std::shared_ptr<TypeNameNode> ScriptParser::parse_type_name()
{
    // A type name can be a simple identifier (e.g. "int", "String", "MyClass")
    // or a qualified identifier (e.g. "System.Collections.Generic.List")
    // followed by optional type arguments (e.g. "<int, string>")
    // and optional array ranks (e.g. "[]", "[][]").

    // For M9, we'll handle Identifier (possibly qualified) <TypeArguments> [].
    // Qualified identifier parsing for type names is not explicitly done here yet,
    // it would involve parsing `Ident.Ident.Ident`. For now, `name` is a single identifier.
    // This means "System.Collections.Generic.List" would be parsed as "System" if we don't enhance this.
    // Let's assume for now that `name` in TypeNameNode is the first part, and qualified names are
    // represented by a chain of MemberAccessExpressions if they appear in an expression context,
    // or would need a dedicated qualified name parser here.
    // For simplicity in M9, let's assume simple (single) identifier for the base type name.
    // A full solution would parse `QualifiedIdentifierNode` here or similar.

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
    // node->location will be initially set by make_ast_node to nameToken.location.

    // Parse optional type arguments: <T1, T2, ...>
    if (check_token(TokenType::LessThan))
    {
        // This check is important to differentiate `a < b` from `List<T>`.
        // A common heuristic: if the preceding token was an identifier that could be a generic type,
        // and we see '<', try to parse type arguments. If it fails (e.g., not `ident >`), backtrack or error.
        // Our current structure: parse_type_name is called when a type is expected.
        // So, if we see '<', it's highly likely for type arguments.
        
        // We need a function that parses a list of TypeNameNodes.
        // Let's use a new helper or adapt `parse_optional_type_argument_list` if suitable.
        // `parse_optional_type_argument_list` is designed for expressions.
        // Here, we are parsing TypeNameNodes recursively.
        
        consume_token(TokenType::LessThan, "Expected '<' to start type argument list.");
        if (check_token(TokenType::GreaterThan)) { // Handles <> for unbound generics like typeof(List<>)
             // If `List<>` is allowed, we consume `>` and typeArguments remains empty.
             // C# allows this in `typeof`. For general type usage, it's usually an error.
             // For now, let's assume if `<` is present, arguments must follow or it's `>` for unbound.
        } else {
            do
            {
                node->typeArguments.push_back(parse_type_name()); // Recursive call
            } while (match_token(TokenType::Comma));
        }
        consume_token(TokenType::GreaterThan, "Expected '>' to close type argument list.");
    }

    // Parse optional array ranks: [], [][] etc. (M10 for full array support)
    // For M9, we'll just check for one level of array `isArray`.
    if (match_token(TokenType::OpenBracket))
    {
        consume_token(TokenType::CloseBracket, "Expected ']' to close array rank specifier.");
        node->isArray = true;
        // Future: handle multi-dimensional arrays or jagged arrays by counting ranks or parsing dimensions.
        while(match_token(TokenType::OpenBracket)) // For `int[][]`
        {
            consume_token(TokenType::CloseBracket, "Expected ']' for subsequent array rank.");
            // How to represent jagged `[][]` vs multi-dim `[,]`?
            // Current AST `bool isArray` is too simple for that.
            // For M9, one `[]` sets `isArray = true`.
        }
    }
    
    finalize_node_location(node, nameToken);
    return node;
}

std::shared_ptr<MemberDeclarationNode> ScriptParser::parse_member_declaration(
    const Token& startOfMemberToken,
    const std::optional<std::string>& currentClassName)
{
    std::vector<ModifierKind> modifiers = parse_modifiers();
    Token effectiveStartToken = modifiers.empty() ? current_token() : startOfMemberToken;

    Token firstSignificantToken = current_token();

    if (currentClassName.has_value() &&
        firstSignificantToken.type == TokenType::Identifier &&
        firstSignificantToken.lexeme == currentClassName.value() &&
        peek_token(1).type == TokenType::OpenParen) // Constructor: ClassName (
    {
        advance_token(); 
        return parse_constructor_declaration(std::move(modifiers), currentClassName.value(), firstSignificantToken);
    }
    // Check for constructor with generic parameters: ClassName <T> (
    // This is not standard C#; class itself is generic. If constructors could be generic, it'd be:
    // else if (currentClassName.has_value() &&
    //     firstSignificantToken.type == TokenType::Identifier &&
    //     firstSignificantToken.lexeme == currentClassName.value() &&
    //     peek_token(1).type == TokenType::LessThan) // Potentially ClassName<T> ( for constructor if they could be generic
    // { ... }
    else
    {
        // Must be a field or method (or other member type like property, event, operator)
        std::shared_ptr<TypeNameNode> memberType = parse_type_name(); // Consumes the type name
        Token memberNameToken = current_token(); // Now at the potential member name

        if (memberNameToken.type == TokenType::Identifier)
        {
            std::string memberNameStr = memberNameToken.lexeme;
            
            // Look ahead for method signature: Identifier <...> ( or Identifier (
            // peek_token(0) is memberNameToken, peek_token(1) is after memberNameToken
            Token lookahead1 = peek_token(1);
            Token lookahead2; // Potentially after <...> or (
            bool isMethodSignature = false;

            if (lookahead1.type == TokenType::OpenParen) // Method: Name (
            {
                isMethodSignature = true;
            }
            else if (lookahead1.type == TokenType::LessThan) // Potential generic method: Name <
            {
                // Need to look further past the potential <...> to see if '(' follows
                // This is complex for simple LL(1). We need to tentatively parse type parameters.
                // For now, a simpler heuristic: if we see Name<...>, and *after* the matching >,
                // we see '(', then it's a method.
                // This requires a "trial parse" or more robust lookahead.
                
                // Let's try a limited lookahead for `Name < ... > (`
                // This is still not perfect without backtracking or a more powerful parser.
                // The current `parse_method_declaration` *expects* to be called when we're sure it's a method.
                // It consumes the name *then* looks for `<`.

                // The current `parse_method_declaration` will parse `name <params> (args)`.
                // So, if `memberNameToken` is followed by `<` OR `(`, it's a method.
                isMethodSignature = true; // Assume if it's `Name <` or `Name (`, it's a method/constructor path
                                          // and parse_method_declaration will sort it out.
            }

            if (isMethodSignature)
            {
                advance_token(); // Consume the method name identifier (memberNameToken)
                return parse_method_declaration(std::move(modifiers), memberType, memberNameStr, effectiveStartToken);
            }
            else // Field
            {
                // `memberNameToken` is the first field name. It has NOT been consumed yet.
                // `parse_field_declaration` calls `parse_variable_declarator_list`, which consumes it.
                return parse_field_declaration(std::move(modifiers), memberType, effectiveStartToken);
            }
        }
        else
        {
            throw create_error(memberNameToken, "Expected identifier for member name after type.");
        }
    }
}

std::shared_ptr<FieldDeclarationNode> ScriptParser::parse_field_declaration(
    std::vector<ModifierKind> modifiers,
    std::shared_ptr<TypeNameNode> type,
    const Token& fieldDeclarationStartToken) // The very first token of this field decl (modifier or type)
{
    auto node = make_ast_node<FieldDeclarationNode>();
    node->modifiers = std::move(modifiers);
    node->type = type; // This is the TypeNameNode common to all declarators in this field.
    // The location should span from the fieldDeclarationStartToken to the semicolon.
    node->location = fieldDeclarationStartToken.location;


    // The `parse_member_declaration` already consumed modifiers and type.
    // `current_token()` should be the first field name (identifier).
    // `parse_variable_declarator_list` will handle parsing one or more declarators.
    node->declarators = parse_variable_declarator_list(type); // Pass type for context, though not strictly needed by declarator AST node itself

    consume_token(TokenType::Semicolon, "Expected ';' after field declaration.");
    finalize_node_location(node, fieldDeclarationStartToken);
    return node;
}

std::shared_ptr<ExpressionNode> ScriptParser::parse_expression()
{
    // This is the main entry point for parsing any expression.
    // It starts with the lowest precedence operator, which is assignment in many C-like languages.
    // Or, if you have comma operator for expressions, that would be even lower.
    // Your AST has AssignmentExpressionNode.
    return parse_assignment_expression();
}

std::shared_ptr<ExpressionNode> ScriptParser::parse_assignment_expression()
{
    // Parse the left-hand side, which could be an L-value.
    // This would be an expression of higher precedence.
    std::shared_ptr<ExpressionNode> leftOperand = parse_logical_or_expression(); // Or whatever is next in precedence. For M5, this chain still leads to unary/postfix/primary.

    if (check_token({TokenType::Assign, TokenType::PlusAssign, TokenType::MinusAssign,
                     TokenType::AsteriskAssign, TokenType::SlashAssign, TokenType::PercentAssign}))
    {
        Token operatorToken = advance_token(); // Consume the assignment operator
        auto node = make_ast_node<AssignmentExpressionNode>();
        node->target = leftOperand;

        switch (operatorToken.type)
        {
            case TokenType::Assign:         node->op = AssignmentOperator::Assign;          break;
            case TokenType::PlusAssign:     node->op = AssignmentOperator::AddAssign;       break;
            case TokenType::MinusAssign:    node->op = AssignmentOperator::SubtractAssign;  break;
            case TokenType::AsteriskAssign: node->op = AssignmentOperator::MultiplyAssign;  break;
            case TokenType::SlashAssign:    node->op = AssignmentOperator::DivideAssign;    break;
            // case TokenType::PercentAssign:  node->op = AssignmentOperator::ModuloAssign; // Add if ModuloAssign is in your enum
            default:
                throw create_error(operatorToken, "Internal parser error: Unhandled assignment operator token.");
        }

        // Assignment is right-associative, so recursively call parse_assignment_expression for the RHS.
        node->source = parse_assignment_expression();

        // The location should span from the start of the leftOperand to the end of the source.
        if (leftOperand && leftOperand->location.has_value()) {
            finalize_node_location(node, leftOperand->location.value());
        } else {
            // Fallback if leftOperand has no location, though it should.
            finalize_node_location(node, operatorToken);
        }
        return node;
    }

    // If not an assignment, just return the left-hand side expression.
    return leftOperand;
}

std::shared_ptr<ExpressionNode> ScriptParser::parse_primary_expression()
{
    Token current = current_token();
    SourceLocation startLoc = current.location;

    if (match_token(TokenType::IntegerLiteral) ||
        match_token(TokenType::DoubleLiteral)  || // Added DoubleLiteral
        match_token(TokenType::StringLiteral)  ||
        match_token(TokenType::CharLiteral))
    {
        auto node = make_ast_node<LiteralExpressionNode>();
        node->value = previous_token().lexeme; // Use the lexeme as string value for now
        // Determine LiteralKind based on previous_token().type
        switch (previous_token().type)
        {
            case TokenType::IntegerLiteral: node->kind = LiteralKind::Integer; break;
            case TokenType::DoubleLiteral:  node->kind = LiteralKind::Float;   break; // AST uses Float
            case TokenType::StringLiteral:  node->kind = LiteralKind::String;  break;
            case TokenType::CharLiteral:    node->kind = LiteralKind::Char;    break;
            default:
                throw create_error(previous_token(), "Internal error: Unhandled literal token type in primary expression.");
        }
        finalize_node_location(node, previous_token());
        return node;
    }
    else if (match_token(TokenType::True) || match_token(TokenType::False))
    {
        auto node = make_ast_node<LiteralExpressionNode>();
        node->value = previous_token().lexeme;
        node->kind = LiteralKind::Boolean;
        finalize_node_location(node, previous_token());
        return node;
    }
    else if (match_token(TokenType::Null))
    {
        auto node = make_ast_node<LiteralExpressionNode>();
        node->value = "null"; // Consistent value for null
        node->kind = LiteralKind::Null;
        finalize_node_location(node, previous_token());
        return node;
    }
    else if (match_token(TokenType::Identifier))
    {
        auto node = make_ast_node<IdentifierExpressionNode>();
        node->name = previous_token().lexeme;
        finalize_node_location(node, previous_token());
        return node;
    }
    else if (match_token(TokenType::This))
    {
        auto node = make_ast_node<ThisExpressionNode>();
        finalize_node_location(node, previous_token());
        return node;
    }
    else if (match_token(TokenType::OpenParen))
    {
        SourceLocation parenStartLoc = previous_token().location;
        std::shared_ptr<ExpressionNode> expression = parse_expression();
        consume_token(TokenType::CloseParen, "Expected ')' after expression in parentheses.");
        // The location of a parenthesized expression is usually just the inner expression's location,
        // or it could span the parentheses. For simplicity, we just return the inner expression.
        // If a specific node for parenthesized expressions was desired, it'd be created here.
        // For now, the parentheses just control precedence.
        // Let's ensure the location of the expression is preserved.
        if (expression) {
             // If we wanted to wrap it, we'd do it here. But for now, parens just group.
             // Example if we had a ParenthesizedExpressionNode:
             // auto parenNode = make_ast_node<ParenthesizedExpressionNode>();
             // parenNode->expression = expression;
             // finalize_node_location(parenNode, parenStartLoc);
             // return parenNode;
        }
        return expression;
    }
    else if (check_token(TokenType::New)) // check_token because parse_object_creation_expression will consume 'new'
    {
        return parse_object_creation_expression(); // To be fully implemented in M8, basic stub for M3
    }
    else
    {
        throw create_error(current, "Unexpected token in primary expression. Expected literal, identifier, 'this', '(', or 'new'.");
    }
}

std::vector<std::shared_ptr<VariableDeclaratorNode>> ScriptParser::parse_variable_declarator_list(std::shared_ptr<TypeNameNode> typeForContext)
{
    std::vector<std::shared_ptr<VariableDeclaratorNode>> declarators;

    // The first declarator is parsed unconditionally.
    declarators.push_back(parse_variable_declarator(typeForContext));

    // Then, parse subsequent declarators if a comma is found.
    while (match_token(TokenType::Comma))
    {
        declarators.push_back(parse_variable_declarator(typeForContext));
    }

    return declarators;
}

std::shared_ptr<VariableDeclaratorNode> ScriptParser::parse_variable_declarator(std::shared_ptr<TypeNameNode> typeForContext)
{
    Token nameToken = consume_token(TokenType::Identifier, "Expected variable name in declarator.");
    auto node = make_ast_node<VariableDeclaratorNode>();
    node->name = nameToken.lexeme;
    node->location = nameToken.location; // Initial location

    if (match_token(TokenType::Assign))
    {
        node->initializer = parse_expression(); 
        // parse_expression() will parse the RHS.
        // The location of the VariableDeclaratorNode should span from nameToken to the end of the initializer.
    }

    // Finalize location: from the nameToken to the end of the initializer (if any) or just the nameToken.
    // If there's an initializer, previous_token() would be the last token of that initializer.
    // If not, previous_token() would be nameToken itself (due to how advance_token works).
    // However, if there's no initializer, we want the location to be just the nameToken.
    if (node->initializer.has_value()) {
        finalize_node_location(node, nameToken); // Spans from name to end of initializer
    } else {
        node->location = nameToken.location; // Only spans the name token
    }
    return node;
}

std::shared_ptr<LocalVariableDeclarationStatementNode> ScriptParser::parse_local_variable_declaration_statement()
{
    // The first token is the start of the whole statement (e.g. 'var' or TypeName start)
    Token statementStartToken = current_token(); 
    
    ParsedDeclarationParts declParts = parse_variable_declaration_parts();

    auto node = make_ast_node<LocalVariableDeclarationStatementNode>();
    node->isVarDeclaration = declParts.isVar;
    node->type = declParts.type;
    node->declarators = declParts.declarators;
    // node->location will be initially set by make_ast_node using statementStartToken (if current_token at that time was correct)
    // or using the first token of declParts.
    node->location = declParts.startLocation;


    consume_token(TokenType::Semicolon, "Expected ';' after local variable declaration.");
    finalize_node_location(node, declParts.startLocation); // Use the start of the actual declaration parts
    return node;
}

std::shared_ptr<StatementNode> ScriptParser::parse_statement()
{
    if (check_token(TokenType::OpenBrace)) 
    {
        return parse_block_statement();
    }
    else if (check_token(TokenType::Return))
    {
        return parse_return_statement();
    }
    else if (check_token(TokenType::If)) 
    {
        return parse_if_statement();
    }
    else if (check_token(TokenType::While)) 
    {
        return parse_while_statement();
    }
    else if (check_token(TokenType::For)) 
    {
        return parse_for_statement();
    }
    else if (check_token(TokenType::ForEach)) 
    {
        return parse_for_each_statement();
    }
    else if (check_token(TokenType::Break)) 
    {
        return parse_break_statement();
    }
    else if (check_token(TokenType::Continue)) 
    {
        return parse_continue_statement();
    }
    else if (check_token(TokenType::Var))
    {
        return parse_local_variable_declaration_statement();
    }
    // Check for primitive types that could start a declaration
    else if (check_token({TokenType::Int, TokenType::String, TokenType::Bool, TokenType::Void, TokenType::Double, TokenType::Long, TokenType::Char}))
    {
        // Look ahead:
        // PrimitiveType Identifier ... (e.g., "int count;")
        // PrimitiveType [ ] Identifier ... (e.g., "string[] names;")
        Token lookahead1 = peek_token(1);
        if (lookahead1.type == TokenType::Identifier) {
             // Could be "int count;" or "Action myAction;" if Action is a primitive type token (unlikely)
             // Further lookahead for typical declaration endings can make this more robust.
             // For now, assume "PrimitiveType Identifier" is a declaration.
             return parse_local_variable_declaration_statement();
        } else if (lookahead1.type == TokenType::OpenBracket) {
            // Could be "string[] names;"
            // Need to check if after "[]" there's an identifier.
            // peek_token(2) would be ']', peek_token(3) would be the identifier.
            if (peek_token(2).type == TokenType::CloseBracket && 
                peek_token(3).type == TokenType::Identifier) {
                return parse_local_variable_declaration_statement();
            }
            // Also handle jagged arrays like string[][] names;
            // This simple lookahead might need to be more sophisticated for multiple [][]
            // For now, Type[] Ident is the main case.
            // If typeName can parse multiple brackets, this will eventually work out
            // when parse_local_variable_declaration_statement calls parse_type_name.
            // The key is to decide to *try* parsing as a local var decl.
            // If it's Type [ ... ] Ident, it's a declaration.
            // So, if after Type we see [, it's highly likely a declaration attempt.
            return parse_local_variable_declaration_statement();
        }
        // If not "PrimitiveType Identifier" or "PrimitiveType [ ... ] Identifier",
        // it might be an expression or an error. Fall through to expression statement parsing.
    }
    else if (check_token(TokenType::Identifier))
    {
        // This is for user-defined types or generic types starting a declaration.
        // e.g., "MyClass obj;" or "List<int> numbers;"
        // The previous heuristics might still apply, or we can make it more direct.
        // If it starts with an identifier, and it's not followed by typical expression continuations
        // like '(', '.', '++', '--', it might be a type name.
        
        bool tryLocalVarDecl = false;
        Token lookahead1_ident = peek_token(1);

        if (lookahead1_ident.type == TokenType::Identifier) { // Catches `TypeName varName`
            TokenType la2 = peek_token(2).type;
            if (la2 == TokenType::Semicolon || la2 == TokenType::Assign || la2 == TokenType::Comma) {
                tryLocalVarDecl = true;
            }
        } else if (lookahead1_ident.type == TokenType::LessThan || lookahead1_ident.type == TokenType::OpenBracket) { 
            // Catches `GenericType<...> varName` or `Type[] varName`
            // This is a strong indicator of a type name starting.
            tryLocalVarDecl = true;
        }

        if (tryLocalVarDecl) {
             // This path will call parse_local_variable_declaration_statement,
             // which in turn calls parse_type_name. parse_type_name is responsible
             // for correctly parsing "string[]" or "MyType<int>[]" etc.
             return parse_local_variable_declaration_statement();
        }
    }
    
    if (check_token(TokenType::Semicolon)) {
         throw create_error(current_token(), "Empty statements (';') are not directly supported yet, expected expression.");
    }
    
    return parse_expression_statement();
}

std::shared_ptr<BlockStatementNode> ScriptParser::parse_block_statement()
{
    Token openBraceToken = consume_token(TokenType::OpenBrace, "Expected '{' to start a block statement.");
    auto node = make_ast_node<BlockStatementNode>();
    // Location will span from '{' to '}'

    auto oldParent = m_currentParentNode;
    m_currentParentNode = node;

    while (!check_token(TokenType::CloseBrace) && !is_at_end())
    {
        node->statements.push_back(parse_statement());
    }

    m_currentParentNode = oldParent;

    consume_token(TokenType::CloseBrace, "Expected '}' to close a block statement.");
    finalize_node_location(node, openBraceToken);
    return node;
}


std::shared_ptr<MethodDeclarationNode> ScriptParser::parse_method_declaration(
    std::vector<ModifierKind> modifiers,
    std::shared_ptr<TypeNameNode> returnType,
    const std::string& methodName, 
    const Token& methodStartToken) // This token is likely the method name identifier
{
    auto node = make_ast_node<MethodDeclarationNode>();
    node->modifiers = std::move(modifiers);
    node->type = returnType; 
    node->name = methodName;
    node->location = methodStartToken.location; // Initial location from method name token

    // M9: Parse optional generic type parameters for the method itself (e.g., <T>)
    node->typeParameters = parse_optional_type_parameter_list();

    consume_token(TokenType::OpenParen, "Expected '(' to start parameter list for method '" + methodName + "'.");
    node->parameters = parse_parameter_list(); 

    if (check_token(TokenType::OpenBrace))
    {
        node->body = parse_block_statement();
    }
    else if (check_token(TokenType::Semicolon))
    {
        consume_token(TokenType::Semicolon, "Expected ';' for method without a body.");
        node->body = std::nullopt; 
    }
    else
    {
        throw create_error(current_token(), "Expected '{' to start method body or ';' after parameter list for method '" + methodName + "'.");
    }

    // The methodStartToken is the method name. The location should span from potentially
    // earlier tokens (return type, modifiers) to the end of the body/semicolon.
    // The caller `parse_member_declaration` passes `effectiveStartToken` which should be used.
    // For now, `finalize_node_location` uses methodStartToken as the reference start.
    // This might need adjustment if `methodStartToken` isn't the *actual* first token of the declaration.
    // Let's assume `methodStartToken` passed by `parse_member_declaration` is the true start (modifier or type).
    finalize_node_location(node, methodStartToken); 
    return node;
}


std::vector<std::shared_ptr<ParameterDeclarationNode>> ScriptParser::parse_parameter_list()
{
    std::vector<std::shared_ptr<ParameterDeclarationNode>> parameters;

    // The opening parenthesis '(' should have been consumed by the caller.
    if (!check_token(TokenType::CloseParen)) // If not an empty parameter list like ()
    {
        parameters.push_back(parse_parameter_declaration());
        while (match_token(TokenType::Comma))
        {
            parameters.push_back(parse_parameter_declaration());
        }
    }

    consume_token(TokenType::CloseParen, "Expected ')' to close parameter list.");
    return parameters;
}

std::shared_ptr<ParameterDeclarationNode> ScriptParser::parse_parameter_declaration()
{
    Token firstTokenOfParam = current_token(); 
    auto node = make_ast_node<ParameterDeclarationNode>();
    
    // Parameter modifiers (like 'ref', 'out', 'params') would be parsed here if supported.
    // node->modifiers = parse_parameter_specific_modifiers(); // If any

    node->type = parse_type_name();
    node->name = parse_identifier_name("Expected parameter name.");

    // M11: Parse optional default value (e.g., int x = 10)
    if (match_token(TokenType::Assign))
    {
        node->defaultValue = parse_expression(); 
    }
    else
    {
        node->defaultValue = std::nullopt; // Explicitly set to nullopt if no default value
    }
    
    finalize_node_location(node, firstTokenOfParam);
    return node;
}

std::shared_ptr<ReturnStatementNode> ScriptParser::parse_return_statement()
{
    Token returnKeywordToken = consume_token(TokenType::Return, "Expected 'return' keyword.");
    auto node = make_ast_node<ReturnStatementNode>();
    // node->location will be initially set by make_ast_node using returnKeywordToken

    // Check if there's an expression to return or just a semicolon.
    if (check_token(TokenType::Semicolon))
    {
        // `return;` case
        node->expression = std::nullopt;
    }
    else
    {
        // `return expression;` case
        node->expression = parse_expression();
    }

    consume_token(TokenType::Semicolon, "Expected ';' after return statement.");
    finalize_node_location(node, returnKeywordToken);
    return node;
}

std::shared_ptr<ExpressionStatementNode> ScriptParser::parse_expression_statement()
{
    // The first token of the expression will be the start token for location purposes.
    Token expressionStartToken = current_token();
    auto node = make_ast_node<ExpressionStatementNode>();
    
    node->expression = parse_expression();

    consume_token(TokenType::Semicolon, "Expected ';' after expression statement.");
    finalize_node_location(node, expressionStartToken);
    return node;
}

std::shared_ptr<ExpressionNode> ScriptParser::parse_unary_expression()
{
    Token operatorToken = current_token();

    if (match_token(TokenType::LogicalNot) ||
        match_token(TokenType::Plus) ||       // Unary Plus
        match_token(TokenType::Minus) ||      // Unary Minus
        match_token(TokenType::Increment) ||  // Pre-increment ++x
        match_token(TokenType::Decrement))    // Pre-decrement --x
    {
        // `operatorToken` was consumed by `match_token`, so use `previous_token()`
        Token consumedOperatorToken = previous_token();
        auto node = make_ast_node<UnaryExpressionNode>();
        // node->location will be initially set from consumedOperatorToken

        switch (consumedOperatorToken.type)
        {
            case TokenType::LogicalNot: node->op = UnaryOperatorKind::LogicalNot; break;
            case TokenType::Plus:       node->op = UnaryOperatorKind::UnaryPlus;  break;
            case TokenType::Minus:      node->op = UnaryOperatorKind::UnaryMinus; break;
            case TokenType::Increment:  node->op = UnaryOperatorKind::PreIncrement; break;
            case TokenType::Decrement:  node->op = UnaryOperatorKind::PreDecrement; break;
            default:
                // Should not happen if match_token worked correctly
                throw create_error(consumedOperatorToken, "Internal parser error: Unhandled unary operator token.");
        }

        // Recursively call parse_unary_expression for stacked unary operators like `!!x` or `--x`
        // or just parse_postfix_expression if only one level of unary is typical before postfix.
        // Standard C# precedence: postfix > unary > multiplicative ...
        // So unary calls postfix. If multiple unaries are stacked (!!!b), each is a UnaryExpressionNode.
        node->operand = parse_unary_expression(); // Allows for `!!!x` or `---+x`

        finalize_node_location(node, consumedOperatorToken); // Location from operator to end of operand
        return node;
    }
    else
    {
        // If no prefix unary operator, parse the next higher precedence (postfix expressions)
        return parse_postfix_expression();
    }
}

std::shared_ptr<ExpressionNode> ScriptParser::parse_logical_or_expression()
{
    std::shared_ptr<ExpressionNode> leftOperand = parse_logical_and_expression();

    while (match_token(TokenType::LogicalOr))
    {
        Token operatorToken = previous_token(); // The '||' token
        std::shared_ptr<ExpressionNode> rightOperand = parse_logical_and_expression();

        auto binaryNode = make_ast_node<BinaryExpressionNode>();
        binaryNode->left = leftOperand;
        binaryNode->op = BinaryOperatorKind::LogicalOr;
        binaryNode->right = rightOperand;
        
        // Set location from start of left operand to end of right operand
        if (leftOperand && leftOperand->location.has_value()) {
            finalize_node_location(binaryNode, leftOperand->location.value());
        } else { // Fallback, should ideally not happen if leftOperand is valid
            finalize_node_location(binaryNode, operatorToken);
        }
        leftOperand = binaryNode; // For left-associativity
    }
    return leftOperand;
}

std::shared_ptr<ExpressionNode> ScriptParser::parse_logical_and_expression()
{
    std::shared_ptr<ExpressionNode> leftOperand = parse_equality_expression();

    while (match_token(TokenType::LogicalAnd))
    {
        Token operatorToken = previous_token(); // The '&&' token
        std::shared_ptr<ExpressionNode> rightOperand = parse_equality_expression();

        auto binaryNode = make_ast_node<BinaryExpressionNode>();
        binaryNode->left = leftOperand;
        binaryNode->op = BinaryOperatorKind::LogicalAnd;
        binaryNode->right = rightOperand;

        if (leftOperand && leftOperand->location.has_value()) {
            finalize_node_location(binaryNode, leftOperand->location.value());
        } else {
            finalize_node_location(binaryNode, operatorToken);
        }
        leftOperand = binaryNode; // For left-associativity
    }
    return leftOperand;
}

std::shared_ptr<ExpressionNode> ScriptParser::parse_equality_expression()
{
    std::shared_ptr<ExpressionNode> leftOperand = parse_relational_expression();

    while (match_token(TokenType::EqualsEquals) || match_token(TokenType::NotEquals))
    {
        Token operatorToken = previous_token(); // The '==' or '!=' token
        std::shared_ptr<ExpressionNode> rightOperand = parse_relational_expression();

        auto binaryNode = make_ast_node<BinaryExpressionNode>();
        binaryNode->left = leftOperand;
        
        if (operatorToken.type == TokenType::EqualsEquals)
        {
            binaryNode->op = BinaryOperatorKind::Equals;
        }
        else // TokenType::NotEquals
        {
            binaryNode->op = BinaryOperatorKind::NotEquals;
        }
        binaryNode->right = rightOperand;

        if (leftOperand && leftOperand->location.has_value()) {
            finalize_node_location(binaryNode, leftOperand->location.value());
        } else {
            finalize_node_location(binaryNode, operatorToken);
        }
        leftOperand = binaryNode; // For left-associativity
    }
    return leftOperand;
}

std::shared_ptr<ExpressionNode> ScriptParser::parse_relational_expression()
{
    std::shared_ptr<ExpressionNode> leftOperand = parse_additive_expression();

    while (match_token(TokenType::LessThan) || 
           match_token(TokenType::GreaterThan) ||
           match_token(TokenType::LessThanOrEqual) ||
           match_token(TokenType::GreaterThanOrEqual))
    {
        Token operatorToken = previous_token(); 
        std::shared_ptr<ExpressionNode> rightOperand = parse_additive_expression();

        auto binaryNode = make_ast_node<BinaryExpressionNode>();
        binaryNode->left = leftOperand;

        switch (operatorToken.type)
        {
            case TokenType::LessThan:            binaryNode->op = BinaryOperatorKind::LessThan; break;
            case TokenType::GreaterThan:         binaryNode->op = BinaryOperatorKind::GreaterThan; break;
            case TokenType::LessThanOrEqual:     binaryNode->op = BinaryOperatorKind::LessThanOrEqual; break;
            case TokenType::GreaterThanOrEqual:  binaryNode->op = BinaryOperatorKind::GreaterThanOrEqual; break;
            default:
                // Should not happen
                throw create_error(operatorToken, "Internal parser error: Unhandled relational operator token.");
        }
        binaryNode->right = rightOperand;

        if (leftOperand && leftOperand->location.has_value()) {
            finalize_node_location(binaryNode, leftOperand->location.value());
        } else {
            finalize_node_location(binaryNode, operatorToken);
        }
        leftOperand = binaryNode; // For left-associativity (though relational ops are often non-associative, parsing left-to-right is fine)
    }
    return leftOperand;
}

std::shared_ptr<ExpressionNode> ScriptParser::parse_additive_expression()
{
    std::shared_ptr<ExpressionNode> leftOperand = parse_multiplicative_expression();

    while (match_token(TokenType::Plus) || match_token(TokenType::Minus))
    {
        Token operatorToken = previous_token(); // The '+' or '-' token
        std::shared_ptr<ExpressionNode> rightOperand = parse_multiplicative_expression();

        auto binaryNode = make_ast_node<BinaryExpressionNode>();
        binaryNode->left = leftOperand;

        if (operatorToken.type == TokenType::Plus)
        {
            binaryNode->op = BinaryOperatorKind::Add;
        }
        else // TokenType::Minus
        {
            binaryNode->op = BinaryOperatorKind::Subtract;
        }
        binaryNode->right = rightOperand;

        if (leftOperand && leftOperand->location.has_value()) {
            finalize_node_location(binaryNode, leftOperand->location.value());
        } else {
            finalize_node_location(binaryNode, operatorToken);
        }
        leftOperand = binaryNode; // For left-associativity
    }
    return leftOperand;
}

std::shared_ptr<ExpressionNode> ScriptParser::parse_multiplicative_expression()
{
    std::shared_ptr<ExpressionNode> leftOperand = parse_unary_expression();

    while (match_token(TokenType::Asterisk) || 
           match_token(TokenType::Slash) ||
           match_token(TokenType::Percent))
    {
        Token operatorToken = previous_token();
        std::shared_ptr<ExpressionNode> rightOperand = parse_unary_expression();

        auto binaryNode = make_ast_node<BinaryExpressionNode>();
        binaryNode->left = leftOperand;

        switch (operatorToken.type)
        {
            case TokenType::Asterisk: binaryNode->op = BinaryOperatorKind::Multiply; break;
            case TokenType::Slash:    binaryNode->op = BinaryOperatorKind::Divide;   break;
            case TokenType::Percent:  binaryNode->op = BinaryOperatorKind::Modulo;   break;
            default:
                // Should not happen
                throw create_error(operatorToken, "Internal parser error: Unhandled multiplicative operator token.");
        }
        binaryNode->right = rightOperand;

        if (leftOperand && leftOperand->location.has_value()) {
            finalize_node_location(binaryNode, leftOperand->location.value());
        } else {
            finalize_node_location(binaryNode, operatorToken);
        }
        leftOperand = binaryNode; // For left-associativity
    }
    return leftOperand;
}

std::shared_ptr<ExpressionNode> ScriptParser::parse_postfix_expression()
{
    std::shared_ptr<ExpressionNode> expression = parse_primary_expression();

    while (true)
    {
        Token currentPostfixStartToken = current_token(); // For location if expression is null or for operator itself

        if (match_token(TokenType::Dot)) // Member Access: expression.identifier
        {
            SourceLocation accessStartLoc = expression && expression->location.has_value() ?
                                            expression->location.value() :
                                            currentPostfixStartToken.location; // Fallback to location of '.'

            auto accessNode = make_ast_node<MemberAccessExpressionNode>();
            accessNode->target = expression;
            accessNode->memberName = parse_identifier_name("Expected member name after '.'.");
            
            finalize_node_location(accessNode, accessStartLoc);
            expression = accessNode;
        }
        // Check for method call: expression ( args )
        // OR generic method call: expression <typeArgs> ( args )
        else if (check_token(TokenType::OpenParen) || check_token(TokenType::LessThan))
        {
            std::optional<std::vector<std::shared_ptr<TypeNameNode>>> typeArguments = std::nullopt;
            SourceLocation callStartLoc = expression && expression->location.has_value() ? 
                                          expression->location.value() : 
                                          currentPostfixStartToken.location; // Fallback to '<' or '('

            if (check_token(TokenType::LessThan)) {
                // Only attempt to parse type arguments if it looks like a generic method call pattern
                if (can_parse_as_generic_method_arguments()) { 
                    typeArguments = parse_optional_type_argument_list(); // Consumes < ... >
                } else {
                    // It was '<' but not a generic method call pattern.
                    // Break from postfix loop, let relational/binary ops handle it.
                    break; 
                }
            }

            // After potentially parsing type arguments (if any), we must find an OpenParen.
            if (match_token(TokenType::OpenParen))
            {
                auto callNode = make_ast_node<MethodCallExpressionNode>();
                callNode->target = expression;
                callNode->typeArguments = typeArguments; 
                callNode->arguments = parse_argument_list(); // Parses arguments and consumes ')'
                
                finalize_node_location(callNode, callStartLoc);
                expression = callNode;
            }
            else if (typeArguments.has_value())
            {
                // We successfully parsed `<...>` for type arguments, but it wasn't followed by `(...)`.
                // This is a syntax error specific to generic method calls.
                throw create_error(current_token(), "Expected '(' after generic type arguments in method call.");
            }
            else
            {
                // If we checked for '<' but can_parse_as_generic_method_arguments was false,
                // OR if we only checked for '(' and it wasn't there, then it's not a call.
                break; // Not a method call, let other parsers handle the token if possible.
            }
        }
        else if (match_token(TokenType::Increment) || match_token(TokenType::Decrement)) // Postfix ++ or --
        {
            Token consumedOpToken = previous_token(); 
            SourceLocation unaryStartLoc = expression && expression->location.has_value() ?
                                           expression->location.value() :
                                           consumedOpToken.location; 

            auto unaryNode = make_ast_node<UnaryExpressionNode>();
            unaryNode->operand = expression; 

            if (consumedOpToken.type == TokenType::Increment)
            {
                unaryNode->op = UnaryOperatorKind::PostIncrement;
            }
            else 
            {
                unaryNode->op = UnaryOperatorKind::PostDecrement;
            }
            
            finalize_node_location(unaryNode, unaryStartLoc);
            expression = unaryNode;
        }
        else
        {
            break; // No more postfix operators, exit loop
        }
    }
    return expression;
}

std::shared_ptr<ArgumentListNode> ScriptParser::parse_argument_list()
{
    // The opening parenthesis '(' should have been consumed by the caller (e.g., parse_postfix_expression for method calls).
    // This function's main job is to parse the arguments themselves and the closing parenthesis.
    Token startTokenForLocationTracking = previous_token(); // This would be the '(' consumed by the caller.
                                                            // Useful if ArgumentListNode itself needs a precise span.
                                                            // Or, if ArgumentListNode's location is implicit / not critical.

    auto node = make_ast_node<ArgumentListNode>();
    // node->location can be set from the '(' to the ')' by the caller, or we can do it here.
    // Let's assume its location spans the arguments themselves if any, or just the parens.

    if (!check_token(TokenType::CloseParen)) // If there are arguments
    {
        node->arguments.push_back(parse_argument());
        while (match_token(TokenType::Comma))
        {
            if (check_token(TokenType::CloseParen)) { // Allow trailing comma like `(arg1, )` if desired, C# usually doesn't for calls
                throw create_error(current_token(), "Expected expression after comma in argument list, not ')' directly.");
            }
            node->arguments.push_back(parse_argument());
        }
    }

    consume_token(TokenType::CloseParen, "Expected ')' to close argument list.");
    // Finalize location from the token before the first argument (or '(') to ')'
    // If the ArgumentListNode is part of a MethodCall, the MethodCall's location is more important.
    // For now, let's make its location span from the token before the first argument
    // (which is tricky to get here if '(' was consumed by caller) to the ')'.
    // A simpler approach: its location is just the span of its contents, or rely on the parent node's loc.
    // If `node->arguments` is empty, its location would be from `startTokenForLocationTracking` to `previous_token()` (the ')').
    // If it has arguments, from first arg's start to last arg's end.
    // For now, let make_ast_node set an initial loc, and finalize_node_location use that start.
    // The `startTokenForLocationTracking` is the `(`.
    finalize_node_location(node, startTokenForLocationTracking);

    return node;
}

std::shared_ptr<ArgumentNode> ScriptParser::parse_argument()
{
    Token firstTokenOfArgument = current_token(); // The start of the expression for this argument
    auto node = make_ast_node<ArgumentNode>();

    // Your AST for ArgumentNode has an optional<string> name.
    // To support named arguments like `DoSomething(name: "value")`, we would check for `Identifier :` here.
    // For M7, we'll only support positional arguments, so `name` remains std::nullopt.
    // if (check_token(TokenType::Identifier) && peek_token(1).type == TokenType::Colon)
    // {
    //     node->name = consume_token(TokenType::Identifier, "Expected argument name.").lexeme;
    //     consume_token(TokenType::Colon, "Expected ':' after named argument.");
    // }
    // else
    // {
    //     node->name = std::nullopt; // Positional argument
    // }
    node->name = std::nullopt; // For M7, only positional arguments.

    node->expression = parse_expression();
    
    finalize_node_location(node, firstTokenOfArgument);
    return node;
}

std::shared_ptr<ObjectCreationExpressionNode> ScriptParser::parse_object_creation_expression()
{
    Token newKeywordToken = consume_token(TokenType::New, "Expected 'new' keyword.");
    auto node = make_ast_node<ObjectCreationExpressionNode>();
    // node->location is initially set from newKeywordToken
    
    node->type = parse_type_name(); 

    // Arguments are optional
    if (check_token(TokenType::OpenParen))
    {
        consume_token(TokenType::OpenParen, "Expected '(' for object creation arguments or no arguments."); // Consume '('
        node->arguments = parse_argument_list(); // parse_argument_list expects '(' to be consumed and will consume ')'
    }
    else
    {
        // No parentheses, so no arguments.
        // Example: `new MyType;` (if allowed, though C# usually requires `()` for constructorless or default).
        // C# requires `()` even for parameterless constructors, e.g., `new List<int>()`.
        // If your language allows `new TypeName` without `()` for a default constructor,
        // then this `else` block creating an empty ArgumentListNode might be needed,
        // or `arguments` remains `std::nullopt`.
        // For C#-like, `()` are generally expected. So, if `(` is not found, it might be an error
        // unless the grammar allows `new TypeName` to imply a default constructor call without `()`.
        // Your AST has `std::optional<std::shared_ptr<ArgumentListNode>> arguments;`
        // So, if no `()` are present, `arguments` correctly remains `std::nullopt`.
        // This implies that `new Foo` without `()` means no arguments explicitly passed.
        // Let's stick to C# style: if `new TypeName` is followed by something other than `(`,
        // it should be an error *unless* it's valid syntax for array creation without explicit dimensions or something.
        // For object creation, `()` are generally expected.
        // The current check_token(OpenParen) handles this: if no '(', arguments is nullopt.
        // If you wanted to *require* `()`, you'd `consume_token(TokenType::OpenParen, ...)` here unconditionally
        // and `parse_argument_list` would handle empty `()`.

        // Let's refine this: C# `new List<int>();` requires `()`. `new int[5]` is different.
        // For object creation, we expect `()`.
        // The current logic: if `(`, parse args. If not `(`, args is nullopt.
        // This is fine for the AST. Semantic analysis would check if a constructor matched.
        // The `consume_token` for OpenParen was moved inside the `if` block.
    }
    // Add support for object initializers `{ Prop = value }` or collection initializers ` { elem1, elem2 }` in a future milestone.

    finalize_node_location(node, newKeywordToken);
    return node;
}

std::shared_ptr<ConstructorDeclarationNode> ScriptParser::parse_constructor_declaration(
    std::vector<ModifierKind> modifiers,
    const std::string& constructorName, // This is the class name
    const Token& constructorNameToken)  // Token for the class name (used as constructor name)
{
    auto node = make_ast_node<ConstructorDeclarationNode>();
    node->modifiers = std::move(modifiers);
    node->name = constructorName; // Constructor name is same as class name
    // Note: ConstructorDeclarationNode in your AST inherits `std::optional<std::shared_ptr<TypeNameNode>> type;`
    // from MemberDeclarationNode. For constructors, this should be std::nullopt as they don't have a return type.
    node->type = std::nullopt; 
    node->location = constructorNameToken.location;


    // Constructors cannot be generic themselves in C# (the containing type is generic).
    // So, no parsing of <T> here.

    consume_token(TokenType::OpenParen, "Expected '(' to start parameter list for constructor '" + constructorName + "'.");
    node->parameters = parse_parameter_list(); // This will consume the closing ')'

    // Optional constructor initializer (e.g., : base(args) or : this(args)) - Future Milestone
    // if (match_token(TokenType::Colon)) { ... parse constructor initializer ... }

    // Constructor body
    if (check_token(TokenType::OpenBrace))
    {
        node->body = parse_block_statement();
    }
    else
    {
        // C# constructors must have a body. Abstract classes have implicit default constructors if none defined.
        // Interfaces don't have constructors.
        throw create_error(current_token(), "Expected '{' to start constructor body for constructor '" + constructorName + "'.");
    }

    finalize_node_location(node, constructorNameToken);
    return node;
}

std::vector<std::shared_ptr<TypeParameterNode>> ScriptParser::parse_optional_type_parameter_list()
{
    std::vector<std::shared_ptr<TypeParameterNode>> typeParameters;
    if (match_token(TokenType::LessThan))
    {
        // Check for empty list like `<>` which is valid in some contexts (e.g. typeof(List<>))
        // but usually not for defining type parameters. C# requires at least one if <> is present.
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
    Token nameToken = consume_token(TokenType::Identifier, "Expected type parameter name (e.g., T, U).");
    auto node = make_ast_node<TypeParameterNode>();
    node->name = nameToken.lexeme;
    // node->location is set by make_ast_node to nameToken.location

    // Generic constraints (e.g., where T : ISomeInterface) omitted for M9 AST
    // if (match_token(TokenType::Where)) { /* parse constraints */ }

    finalize_node_location(node, nameToken);
    return node;
}

std::optional<std::vector<std::shared_ptr<TypeNameNode>>> ScriptParser::parse_optional_type_argument_list()
{
    if (check_token(TokenType::LessThan))
    {
        // Similar to parse_type_name, we need to be careful here.
        // In an expression like `a < b > c`, `<` and `>` are relational.
        // In `obj.Method<T>()`, they are for type arguments.
        // This usually requires contextual information or more sophisticated lookahead/backtracking.
        // For now, if this function is called, we assume we are in a context where type arguments are expected.
        // The caller (e.g., parse_postfix_expression for method calls) should make this decision.

        Token lessThanToken = consume_token(TokenType::LessThan, "Expected '<' to start explicit type argument list.");
        std::vector<std::shared_ptr<TypeNameNode>> typeArguments;

        // Handle unbound generic method call like `Method<>(...)` if applicable, though less common for explicit args.
        if (check_token(TokenType::GreaterThan)) {
             // This case might be an error or a specific syntax for unbound generic method reference.
             // For now, let's assume if `<` is present, arguments are expected for explicit call.
             // C# `Method<>()` isn't standard for calling; it's usually for typeof or delegate creation.
             // If it means "infer from usage but indicate generic", this is complex.
             // Let's assume an error or that it implies arguments *must* follow if we don't see `>` immediately.
        } else {
            do
            {
                typeArguments.push_back(parse_type_name()); // Each argument is a TypeName
            } while (match_token(TokenType::Comma));
        }
        
        consume_token(TokenType::GreaterThan, "Expected '>' to close explicit type argument list.");
        return typeArguments;
    }
    return std::nullopt; // No '<' found, so no explicit type arguments
}

std::vector<std::shared_ptr<TypeNameNode>> ScriptParser::parse_base_list()
{
    // The colon ':' should have been consumed by the caller (e.g., parse_class_declaration)
    // if it used `match_token(TokenType::Colon)`.
    // If the caller just checked for colon and then called this, this function should consume it.
    // Let's assume the caller (parse_class_declaration) consumed the ':'.

    std::vector<std::shared_ptr<TypeNameNode>> baseTypes;

    // Parse the first base type. There must be at least one if a base list is started.
    baseTypes.push_back(parse_type_name());

    // Parse subsequent base types if a comma is found.
    while (match_token(TokenType::Comma))
    {
        baseTypes.push_back(parse_type_name());
    }

    return baseTypes;
}

std::shared_ptr<IfStatementNode> ScriptParser::parse_if_statement()
{
    Token ifKeywordToken = consume_token(TokenType::If, "Expected 'if' keyword.");
    auto node = make_ast_node<IfStatementNode>();
    // node->location will be initially set by make_ast_node

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
    // node->location initially set by make_ast_node

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
    node->location = forKeywordToken.location; // Initial location

    consume_token(TokenType::OpenParen, "Expected '(' after 'for' keyword.");

    // 1. Declaration or Initializers
    if (!check_token(TokenType::Semicolon)) 
    {
        // Heuristic to distinguish LocalVariableDeclaration from a list of ExpressionStatements
        // This heuristic might need to be more robust (e.g., by attempting to parse as type)
        if (check_token(TokenType::Var) ||
            check_token({TokenType::Int, TokenType::String, TokenType::Bool, TokenType::Void, TokenType::Double, TokenType::Long, TokenType::Char}) ||
            (check_token(TokenType::Identifier) && 
                (peek_token(1).type == TokenType::Identifier || peek_token(1).type == TokenType::LessThan || peek_token(1).type == TokenType::OpenBracket)
            ) // Heuristic for `TypeName varName` or `GenericType<...> varName` or `Type[] varName`
           )
        {
            ParsedDeclarationParts declParts = parse_variable_declaration_parts(true); // Pass context if needed by helper
            
            auto declStatementNode = make_ast_node<LocalVariableDeclarationStatementNode>();
            declStatementNode->isVarDeclaration = declParts.isVar;
            declStatementNode->type = declParts.type;
            declStatementNode->declarators = declParts.declarators;
            finalize_node_location(declStatementNode, declParts.startLocation); // Location for the decl part
            node->declaration = declStatementNode;
        }
        else 
        {
            node->declaration = std::nullopt;
            do { 
                Token exprStart = current_token();
                auto exprStatement = make_ast_node<ExpressionStatementNode>();
                exprStatement->expression = parse_expression();
                finalize_node_location(exprStatement, exprStart);
                node->initializers.push_back(exprStatement);
            } while (match_token(TokenType::Comma));
        }
    }
    consume_token(TokenType::Semicolon, "Expected ';' after for loop initializer/declaration part.");

    // 2. Condition
    if (!check_token(TokenType::Semicolon)) 
    {
        node->condition = parse_expression();
    }
    else
    {
        node->condition = std::nullopt; 
    }
    consume_token(TokenType::Semicolon, "Expected ';' after for loop condition part.");

    // 3. Incrementors
    if (!check_token(TokenType::CloseParen)) 
    {
        do { 
            Token exprStart = current_token();
            auto exprStatement = make_ast_node<ExpressionStatementNode>();
            exprStatement->expression = parse_expression();
            finalize_node_location(exprStatement, exprStart);
            node->incrementors.push_back(exprStatement);
        } while (match_token(TokenType::Comma));
    }
    consume_token(TokenType::CloseParen, "Expected ')' to close for loop header.");

    // 4. Body
    node->body = parse_statement();

    finalize_node_location(node, forKeywordToken);
    return node;
}

std::shared_ptr<ForEachStatementNode> ScriptParser::parse_for_each_statement()
{
    Token foreachKeywordToken = consume_token(TokenType::ForEach, "Expected 'foreach' keyword.");
    auto node = make_ast_node<ForEachStatementNode>();
    node->location = foreachKeywordToken.location; // Initial location

    consume_token(TokenType::OpenParen, "Expected '(' after 'foreach' keyword.");

    // Parse the iteration variable declaration part: TypeName variableName
    // Or 'var variableName' (though your AST ForEachStatementNode directly stores TypeNameNode and string variableName)
    // Let's assume 'var' implies the TypeNameNode for 'variableType' will be a special "var" node
    // or handled during semantic analysis. For parsing, we need a TypeNameNode.

    Token variableTypeStartToken = current_token();
    if (match_token(TokenType::Var))
    {
        auto varTypeNode = make_ast_node<TypeNameNode>();
        varTypeNode->name = "var"; // Special indicator
        finalize_node_location(varTypeNode, previous_token());
        node->variableType = varTypeNode;
    }
    else
    {
        node->variableType = parse_type_name();
    }
    // Ensure the location of the variableType node is set correctly relative to its parsing.
    // `parse_type_name` should handle its own finalization. If it was 'var', it's already done.

    node->variableName = parse_identifier_name("Expected variable name in foreach loop.");

    consume_token(TokenType::In, "Expected 'in' keyword in foreach loop.");

    node->collection = parse_expression();

    consume_token(TokenType::CloseParen, "Expected ')' to close foreach loop header.");

    node->body = parse_statement();

    finalize_node_location(node, foreachKeywordToken);
    return node;
}

std::shared_ptr<BreakStatementNode> ScriptParser::parse_break_statement()
{
    Token breakKeywordToken = consume_token(TokenType::Break, "Expected 'break' keyword.");
    auto node = make_ast_node<BreakStatementNode>();
    // node->location will be set by make_ast_node from breakKeywordToken

    consume_token(TokenType::Semicolon, "Expected ';' after 'break' statement.");
    
    finalize_node_location(node, breakKeywordToken); // Ensure location spans keyword and semicolon
    return node;
}

std::shared_ptr<ContinueStatementNode> ScriptParser::parse_continue_statement()
{
    Token continueKeywordToken = consume_token(TokenType::Continue, "Expected 'continue' keyword.");
    auto node = make_ast_node<ContinueStatementNode>();
    // node->location will be set by make_ast_node from continueKeywordToken

    consume_token(TokenType::Semicolon, "Expected ';' after 'continue' statement.");

    finalize_node_location(node, continueKeywordToken); // Ensure location spans keyword and semicolon
    return node;
}

std::shared_ptr<StructDeclarationNode> ScriptParser::parse_struct_declaration(
    std::vector<ModifierKind> modifiers,
    const Token& structKeywordToken, // The 'struct' token itself, already consumed by caller
    const std::string& structName)
{
    auto node = make_ast_node<StructDeclarationNode>();
    node->modifiers = std::move(modifiers);
    node->name = structName;
    node->location = structKeywordToken.location; // Initial location from 'struct' keyword

    // M9: Parse optional type parameters <T, U> for generic structs
    node->typeParameters = parse_optional_type_parameter_list();

    // M9: Parse optional base list : IInterface1, IInterface2 (Structs can only implement interfaces)
    if (match_token(TokenType::Colon))
    {
        node->baseList = parse_base_list();
    }

    consume_token(TokenType::OpenBrace, "Expected '{' to open struct body.");

    auto oldParent = m_currentParentNode;
    m_currentParentNode = node;

    while (!check_token(TokenType::CloseBrace) && !is_at_end())
    {
        // Structs can have fields, methods, constructors, etc.
        // Pass the struct's name for constructor identification.
        node->members.push_back(parse_member_declaration(current_token(), node->name));
    }
    m_currentParentNode = oldParent;

    consume_token(TokenType::CloseBrace, "Expected '}' to close struct body.");
    
    // Finalize location from the structKeywordToken to the closing '}'
    // The overall TypeDeclarationNode (which is StructDeclarationNode) will have its location
    // correctly set by the caller (`parse_file_level_declaration`) if there were modifiers before.
    finalize_node_location(node, structKeywordToken);
    return node;
}

} // namespace Mycelium::Scripting::Lang