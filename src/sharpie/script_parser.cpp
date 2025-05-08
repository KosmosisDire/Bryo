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
    Token startOfDeclarationToken = current_token();
    std::vector<ModifierKind> modifiers = parse_modifiers();

    Token keywordToken = current_token();

    if (keywordToken.type == TokenType::Class || keywordToken.type == TokenType::Struct)
    {
        auto typeNode = parse_type_declaration(std::move(modifiers), keywordToken);
        if (typeNode) { 
            SourceLocation trueStart = modifiers.empty() ? keywordToken.location : startOfDeclarationToken.location;
            // Ensure the node's start location is correctly set if modifiers were present.
            // The node itself gets its location from keywordToken within parse_class_declaration.
            // We need to adjust it here if modifiers made the declaration start earlier.
            if (typeNode->location.has_value()) {
                if (trueStart.line_start < typeNode->location.value().line_start ||
                    (trueStart.line_start == typeNode->location.value().line_start &&
                     trueStart.column_start < typeNode->location.value().column_start)) {
                    
                    SourceLocation updatedLocation = typeNode->location.value();
                    updatedLocation.line_start = trueStart.line_start;
                    updatedLocation.column_start = trueStart.column_start;
                    typeNode->location = updatedLocation;
                }
            } else { // If node location wasn't set, use trueStart.
                 finalize_node_location(typeNode, trueStart);
            }
        }
        return typeNode;
    }
    else
    {
        throw create_error(startOfDeclarationToken, "Expected class, struct, enum, or interface declaration at file level.");
    }
}

std::shared_ptr<TypeDeclarationNode> ScriptParser::parse_type_declaration(std::vector<ModifierKind> modifiers, const Token& keywordToken)
{
    if (keywordToken.type != TokenType::Class && keywordToken.type != TokenType::Struct) {
        throw create_error(keywordToken, "Internal error: parse_type_declaration called with unexpected token.");
    }
    advance_token(); 

    std::string name = parse_identifier_name("Expected type name after '" + keywordToken.lexeme + "'.");

    if (keywordToken.type == TokenType::Class)
    {
        return parse_class_declaration(std::move(modifiers), keywordToken, name);
    }
    else if (keywordToken.type == TokenType::Struct)
    {
        throw create_error(keywordToken, "Struct parsing not yet implemented.");
    }
    throw create_error(keywordToken, "Expected 'class' or 'struct'.");
}

std::shared_ptr<ClassDeclarationNode> ScriptParser::parse_class_declaration(
    std::vector<ModifierKind> modifiers,
    const Token& classKeywordToken, 
    const std::string& className)
{
    auto node = make_ast_node<ClassDeclarationNode>();
    node->modifiers = std::move(modifiers);
    node->name = className;
    node->location = classKeywordToken.location;

    if (check_token(TokenType::LessThan))
    {
        // node->typeParameters = parse_optional_type_parameter_list(); // M9
    }

    if (check_token(TokenType::Colon))
    {
        // node->baseList = parse_base_list(); // M9
    }

    consume_token(TokenType::OpenBrace, "Expected '{' to open class body.");

    auto oldParent = m_currentParentNode;
    m_currentParentNode = node;

    while (!check_token(TokenType::CloseBrace) && !is_at_end())
    {
        // In M2, we start parsing actual members.
        node->members.push_back(parse_member_declaration(current_token()));
    }
    m_currentParentNode = oldParent;

    consume_token(TokenType::CloseBrace, "Expected '}' to close class body.");
    finalize_node_location(node, classKeywordToken);
    return node;
}


std::shared_ptr<TypeNameNode> ScriptParser::parse_type_name() {
    if (check_token(TokenType::Identifier) ||
        check_token({TokenType::Int, TokenType::String, TokenType::Bool, TokenType::Void, TokenType::Long, TokenType::Double, TokenType::Char})) {
        Token typeToken = advance_token();
        auto node = make_ast_node<TypeNameNode>();
        node->name = typeToken.lexeme;
        finalize_node_location(node, typeToken);
        return node;
    }
    throw create_error("Expected type name.");
}

std::shared_ptr<MemberDeclarationNode> ScriptParser::parse_member_declaration(const Token& startOfMemberToken)
{
    std::vector<ModifierKind> modifiers = parse_modifiers();
    
    // Store the effective start token for the member (could be a modifier or the type/name)
    // If modifiers were present, startOfMemberToken (passed in) is the true start.
    // If no modifiers, the current token (type/name) is the start.
    Token effectiveStartToken = modifiers.empty() ? current_token() : startOfMemberToken;

    // Attempt to parse a type name. This could be a return type for a method/field,
    // or it could be the class name itself if it's a constructor.
    std::shared_ptr<TypeNameNode> potentialType = parse_type_name();
    Token nameOrOperatorToken = current_token(); // This should be the member's name or an operator token.

    if (nameOrOperatorToken.type == TokenType::Identifier)
    {
        // We have: [modifiers] TypeName Identifier ...
        // This could be a field, a method, or (if TypeName was actually the class name) a constructor.
        std::string memberName = nameOrOperatorToken.lexeme; // advance_token() below will consume it if it's a method/constructor
        
        if (peek_token(1).type == TokenType::OpenParen) // Next token is '(', so it's a method or constructor
        {
            advance_token(); // Consume the identifier (method name)
            // TODO M8: Distinguish constructor vs method.
            // For now, assume method. If `potentialType->name == memberName` and current class context matches, it's a constructor.
            // This distinction requires knowing the current class name, which parse_member_declaration doesn't easily have.
            // A common approach is for the caller (parse_class_declaration) to pass the class name.
            // Or, if it's a constructor, the `potentialType` would actually be the constructor's name,
            // and `MemberDeclarationNode::type` (return type) would be nullopt.
            // For M4, we'll treat it as a method. Constructors will refine this in M8.

            // If `potentialType`'s name matches `memberName` AND it's within a class context,
            // it's likely a constructor. Constructors don't have a return type in the AST's `type` field.
            // (Your ConstructorDeclarationNode doesn't have a `type` field, but MemberDeclarationNode does).
            // This logic will be tricky. For now, let's assume methods.
            // We'll need to adjust this for constructors in M8.

            // Let's assume for M4, if it looks like `TypeName Identifier (`, it's a method.
            return parse_method_declaration(std::move(modifiers), potentialType, memberName, effectiveStartToken);
        }
        else
        {
            // Not followed by '(', so assume it's a field.
            // `parse_field_declaration` expects to be AT the first field name identifier.
            // We have already parsed `potentialType`. `nameOrOperatorToken` is the first field name.
            // `parse_field_declaration` will consume `nameOrOperatorToken` via its `parse_variable_declarator_list`.
            return parse_field_declaration(std::move(modifiers), potentialType, effectiveStartToken);
        }
    }
    // Handle other member types like operators, indexers, events in future milestones.
    // else if (nameOrOperatorToken.type == TokenType::Operator) { ... }
    else
    {
        throw create_error(nameOrOperatorToken, "Expected identifier for member name after type.");
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
    Token startToken = current_token(); // Could be 'var' or a type name
    auto node = make_ast_node<LocalVariableDeclarationStatementNode>();

    if (match_token(TokenType::Var))
    {
        node->isVarDeclaration = true;
        // TypeNameNode will be null or a special "var" representation if needed later by semantic analysis.
        // For the AST, TypeNameNode is std::shared_ptr, so it can be null.
        // Your AST struct does not make type optional for LocalVariableDeclarationStatementNode,
        // but it does have isVarDeclaration. If isVarDeclaration is true, type might be ignored or inferred.
        // Let's ensure TypeNameNode is not null if not var, and potentially create a dummy for 'var' if AST requires.
        // Your AST: std::shared_ptr<TypeNameNode> type;
        // This means it *can* be nullptr. For `var`, we can leave it null, or create a special 'var' TypeNameNode.
        // Let's leave it null for now if `isVarDeclaration` is true.
        // However, the provided AST for LocalVariableDeclarationStatementNode has a non-optional TypeNameNode.
        // This seems like a slight mismatch if `var` is to be supported by leaving `type` unset.
        // Let's assume if `isVarDeclaration` is true, the `type` field is conceptually ignored by consumers,
        // or we should make `type` optional in the AST.
        // For now, if `isVarDeclaration` is true, we'll assign a dummy TypeNameNode to satisfy the AST.
        auto varTypeNode = make_ast_node<TypeNameNode>();
        varTypeNode->name = "var"; // Special name
        finalize_node_location(varTypeNode, previous_token()); // Location of 'var' keyword
        node->type = varTypeNode;
    }
    else
    {
        node->isVarDeclaration = false;
        node->type = parse_type_name();
    }

    node->declarators = parse_variable_declarator_list(node->type);

    consume_token(TokenType::Semicolon, "Expected ';' after local variable declaration.");
    finalize_node_location(node, startToken);
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
    else if (check_token(TokenType::Var))
    {
        return parse_local_variable_declaration_statement();
    }
    else if (check_token({TokenType::Int, TokenType::String, TokenType::Bool, TokenType::Void, TokenType::Double, TokenType::Long, TokenType::Char}) &&
             peek_token(1).type == TokenType::Identifier)
    {
        return parse_local_variable_declaration_statement();
    }
    else if (check_token(TokenType::Identifier))
    {
        if (peek_token(1).type == TokenType::Identifier)
        {
            TokenType lookahead2Type = peek_token(2).type;
            if (lookahead2Type == TokenType::Semicolon ||
                lookahead2Type == TokenType::Assign ||
                lookahead2Type == TokenType::Comma)
            {
                return parse_local_variable_declaration_statement();
            }
        }
        // If not a clear declaration, it falls through to become an expression statement.
    }
    // Add other specific statement checks here in future milestones:
    // else if (check_token(TokenType::If)) return parse_if_statement(); // M6
    // else if (check_token(TokenType::While)) return parse_while_statement(); // M6
    // else if (check_token(TokenType::For)) return parse_for_statement(); // M10
    // else if (check_token(TokenType::ForEach)) return parse_for_each_statement(); // M10
    // else if (check_token(TokenType::Break)) return parse_break_statement(); // M10
    // else if (check_token(TokenType::Continue)) return parse_continue_statement(); // M10


    // If no other specific statement keyword is matched, assume it's an expression statement.
    // This must be the last check or a fallback.
    // An empty statement (just a semicolon) would be an error if parse_expression() fails on ';'.
    // However, parse_expression_statement expects an expression then a semicolon.
    // An empty statement ``;` is not directly handled here unless parse_expression() can parse "nothing"
    // which it cannot. Typically, empty statements are explicitly parsed or ignored.
    // For now, an isolated ';' will likely lead to parse_expression() failing.
    if (check_token(TokenType::Semicolon)) {
        // This would be an empty statement.
        // If you want to support them, create an EmptyStatementNode or similar.
        // For now, let parse_expression_statement handle it, which might error if parse_expression fails.
        // A dedicated EmptyStatementNode would be better.
        // For now, we let it fall to parse_expression_statement which will likely error
        // if the expression part is truly empty.
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
    const std::string& methodName, // Name already parsed by caller
    const Token& methodStartToken) // Token for method name or first significant token of method
{
    auto node = make_ast_node<MethodDeclarationNode>();
    node->modifiers = std::move(modifiers);
    node->type = returnType; // This is the return type
    node->name = methodName;
    // Initial location set by make_ast_node, will be finalized.
    // methodStartToken might be the return type's token, or method name token.
    // For proper span, we need the first token of the whole declaration (modifier or return type).
    // Let's assume methodStartToken is the actual method name token for now,
    // and the caller of parse_member_declaration will handle the overall span.
    // Or, more accurately, the first token that distinguished this as a method.
    node->location = methodStartToken.location;


    // Optional generic type parameters for the method itself (e.g., <T>) - M9
    if (check_token(TokenType::LessThan))
    {
        // node->typeParameters = parse_optional_type_parameter_list(); // M9
    }

    consume_token(TokenType::OpenParen, "Expected '(' to start parameter list for method '" + methodName + "'.");
    node->parameters = parse_parameter_list(); // This will consume the closing ')'

    // Method body or semicolon for abstract/interface methods
    if (check_token(TokenType::OpenBrace))
    {
        node->body = parse_block_statement();
    }
    else if (check_token(TokenType::Semicolon))
    {
        consume_token(TokenType::Semicolon, "Expected ';' for method without a body (e.g., abstract or interface method).");
        node->body = std::nullopt; // No body
    }
    else
    {
        throw create_error(current_token(), "Expected '{' to start method body or ';' for abstract/interface method after parameter list for method '" + methodName + "'.");
    }

    finalize_node_location(node, methodStartToken); // Finalize from method name to end of body or semicolon
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
    Token firstTokenOfParam = current_token(); // Could be type
    auto node = make_ast_node<ParameterDeclarationNode>();
    
    // Modifiers for parameters (e.g., 'ref', 'out', 'params') are not in the AST `ParameterDeclarationNode`'s
    // `modifiers` list directly, but would be handled by parsing specific keywords if supported.
    // For now, ParameterDeclarationNode inherits `modifiers` from DeclarationNode,
    // but C# parameters don't use standard access modifiers. We'll ignore parsing modifiers here for now.
    // If your language supports `readonly int param`, then `parse_modifiers()` would be called.
    // node->modifiers = parse_modifiers(); // If parameters can have modifiers like readonly

    node->type = parse_type_name();
    node->name = parse_identifier_name("Expected parameter name.");

    // Optional default value (e.g., int x = 10) - Future Milestone
    if (match_token(TokenType::Assign))
    {
        // node->defaultValue = parse_expression(); // For a later milestone
        throw create_error(previous_token(), "Parameter default values not yet supported.");
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

std::shared_ptr<ExpressionNode> ScriptParser::parse_logical_or_expression() { return parse_logical_and_expression(); }
std::shared_ptr<ExpressionNode> ScriptParser::parse_logical_and_expression() { return parse_equality_expression(); }
std::shared_ptr<ExpressionNode> ScriptParser::parse_equality_expression() { return parse_relational_expression(); }
std::shared_ptr<ExpressionNode> ScriptParser::parse_relational_expression() { return parse_additive_expression(); }
std::shared_ptr<ExpressionNode> ScriptParser::parse_additive_expression() { return parse_multiplicative_expression(); }
std::shared_ptr<ExpressionNode> ScriptParser::parse_multiplicative_expression() { return parse_unary_expression(); } 
std::shared_ptr<ExpressionNode> ScriptParser::parse_postfix_expression() { return parse_primary_expression(); }
std::shared_ptr<ArgumentListNode> ScriptParser::parse_argument_list() { throw std::logic_error("parse_argument_list not fully implemented"); }
std::shared_ptr<ArgumentNode> ScriptParser::parse_argument() { throw std::logic_error("parse_argument not fully implemented"); }

std::shared_ptr<ObjectCreationExpressionNode> ScriptParser::parse_object_creation_expression()
{
    Token newKeywordToken = consume_token(TokenType::New, "Expected 'new' keyword.");
    auto node = make_ast_node<ObjectCreationExpressionNode>();
    
    node->type = parse_type_name(); // TypeNameNode here will handle generic instantiations like new List<int>() in M9

    // Arguments are optional and will be parsed in M8
    if (check_token(TokenType::OpenParen))
    {
        // node->arguments = parse_argument_list(); // M7/M8
        // For M3, if arguments are present, it's an error or unhandled.
        throw create_error(current_token(), "Object creation arguments not yet supported in Milestone 3.");
    }
    else
    {
        node->arguments = std::nullopt; // No arguments
    }

    finalize_node_location(node, newKeywordToken);
    return node;
}
std::shared_ptr<ConstructorDeclarationNode> ScriptParser::parse_constructor_declaration(std::vector<ModifierKind> mods, const std::string& name, const Token& start) { throw std::logic_error("parse_constructor_declaration not fully implemented"); }
std::vector<std::shared_ptr<TypeParameterNode>> ScriptParser::parse_optional_type_parameter_list() { return {}; }
std::shared_ptr<TypeParameterNode> ScriptParser::parse_type_parameter() { throw std::logic_error("parse_type_parameter not fully implemented"); }
std::optional<std::vector<std::shared_ptr<TypeNameNode>>> ScriptParser::parse_optional_type_argument_list() { return std::nullopt; }
std::vector<std::shared_ptr<TypeNameNode>> ScriptParser::parse_base_list() { return {}; }
std::shared_ptr<IfStatementNode> ScriptParser::parse_if_statement() { throw std::logic_error("parse_if_statement not fully implemented"); }
std::shared_ptr<WhileStatementNode> ScriptParser::parse_while_statement() { throw std::logic_error("parse_while_statement not fully implemented"); }
std::shared_ptr<ForStatementNode> ScriptParser::parse_for_statement() { throw std::logic_error("parse_for_statement not fully implemented"); }
std::shared_ptr<ForEachStatementNode> ScriptParser::parse_for_each_statement() { throw std::logic_error("parse_for_each_statement not fully implemented"); }
std::shared_ptr<BreakStatementNode> ScriptParser::parse_break_statement() { throw std::logic_error("parse_break_statement not fully implemented"); }
std::shared_ptr<ContinueStatementNode> ScriptParser::parse_continue_statement() { throw std::logic_error("parse_continue_statement not fully implemented"); }
std::shared_ptr<StructDeclarationNode> ScriptParser::parse_struct_declaration(std::vector<ModifierKind> mods, const Token& start, const std::string& name) { throw std::logic_error("parse_struct_declaration not fully implemented"); }

} // namespace Mycelium::Scripting::Lang