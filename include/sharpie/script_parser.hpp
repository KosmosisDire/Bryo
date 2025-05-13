#pragma once

#include "script_ast.hpp" // Assumed to be simplified accordingly
#include "script_token.hpp"
#include <vector>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <functional> // For std::function

namespace Mycelium::Scripting::Lang
{

// Simplified: No 'isVar' as 'var' keyword is removed.
struct ParsedDeclarationParts
{
    std::shared_ptr<TypeNameNode> type;
    std::vector<std::shared_ptr<VariableDeclaratorNode>> declarators;
    SourceLocation startLocation;
};

class ParseError : public std::runtime_error
{
public:
    ParseError(const std::string& message, const SourceLocation& location)
        : std::runtime_error(format_message(message, location)), m_location(location)
    {
    }

    const SourceLocation& get_location() const
    {
        return m_location;
    }

private:
    SourceLocation m_location;

    static std::string format_message(const std::string& baseMessage, const SourceLocation& loc)
    {
        return "Parse Error (Line " + std::to_string(loc.line_start) +
               ", Col " + std::to_string(loc.column_start) + "): " + baseMessage;
    }
};


class ScriptParser
{
public:
    ScriptParser(const std::vector<Token>& tokens);
    std::shared_ptr<CompilationUnitNode> parse_compilation_unit();

private:
    const std::vector<Token>& m_tokens;
    size_t m_currentIndex;
    std::weak_ptr<AstNode> m_currentParentNode;

    // Core token handling
    bool is_at_end() const;
    const Token& peek_token(int offset = 0) const;
    const Token& current_token() const;
    const Token& previous_token() const;
    const Token& consume_token(TokenType expectedType, const std::string& errorMessage);
    const Token& advance_token();
    bool match_token(TokenType type);
    bool match_token(const std::vector<TokenType>& types);
    bool check_token(TokenType type) const;
    bool check_token(const std::vector<TokenType>& types) const;

    // Error and AST node creation
    ParseError create_error(const Token& token, const std::string& message);
    ParseError create_error(const std::string& message);

    template <typename T, typename... Args>
    std::shared_ptr<T> make_ast_node(Args&&... args)
    {
        auto node = std::make_shared<T>(std::forward<Args>(args)...);
        node->parent = m_currentParentNode;
        if (m_currentIndex < m_tokens.size())
        {
            // Set initial location, will be finalized
            node->location = current_token().location;
        }
        return node;
    }

    void finalize_node_location(std::shared_ptr<AstNode> node, const SourceLocation& startLocation);
    void finalize_node_location(std::shared_ptr<AstNode> node, const Token& startToken);
    void with_parent_context(const std::shared_ptr<AstNode>& newParentNode, const std::function<void()>& parserFunc);

    // Parsing rules (simplified)
    std::shared_ptr<TypeDeclarationNode> parse_file_level_declaration(const Token& declarationStartToken); // Simplified from NamespaceMemberDeclarationNode

    std::vector<ModifierKind> parse_modifiers(); // To support public, private, static
    std::string parse_identifier_name(const std::string& contextMessage = "Expected identifier");
    std::string parse_qualified_identifier(const std::string& contextMessageStart, const std::string& contextMessagePart); // For Type.Member potentially

    ParsedDeclarationParts parse_variable_declaration_parts(); // No 'var'
    std::shared_ptr<ClassDeclarationNode> parse_class_declaration(std::vector<ModifierKind> modifiers, const Token& classKeywordToken, const std::string& className, const Token& actualStartToken); // No generics, no base list

    std::shared_ptr<TypeNameNode> parse_type_name(); // No generics, no arrays
    std::shared_ptr<MemberDeclarationNode> parse_member_declaration(const Token& memberStartToken, const std::optional<std::string>& currentClassName);
    std::shared_ptr<FieldDeclarationNode> parse_field_declaration(std::vector<ModifierKind> modifiers, std::shared_ptr<TypeNameNode> type, const Token& fieldDeclStartToken);
    std::shared_ptr<MethodDeclarationNode> parse_method_declaration(std::vector<ModifierKind> modifiers, std::shared_ptr<TypeNameNode> returnType, const std::string& methodName, const Token& methodDeclStartToken); // No generics
    std::shared_ptr<ConstructorDeclarationNode> parse_constructor_declaration(std::vector<ModifierKind> modifiers, const std::string& constructorName, const Token& constructorNameToken, const Token& actualStartToken);

    std::vector<std::shared_ptr<ParameterDeclarationNode>> parse_parameter_list();
    std::shared_ptr<ParameterDeclarationNode> parse_parameter_declaration(); // No default values

    std::vector<std::shared_ptr<VariableDeclaratorNode>> parse_variable_declarator_list();
    std::shared_ptr<VariableDeclaratorNode> parse_variable_declarator();

    // Statements
    std::shared_ptr<StatementNode> parse_statement();
    std::shared_ptr<BlockStatementNode> parse_block_statement();
    std::shared_ptr<LocalVariableDeclarationStatementNode> parse_local_variable_declaration_statement(); // No 'var'
    std::shared_ptr<ExpressionStatementNode> parse_expression_statement();
    std::shared_ptr<IfStatementNode> parse_if_statement();
    std::shared_ptr<ReturnStatementNode> parse_return_statement();

    // Expressions
    std::shared_ptr<ExpressionNode> parse_expression();
    std::shared_ptr<ExpressionNode> parse_assignment_expression(); // Only basic '='
    // Removed logical_or, logical_and to simplify if conditions for now.
    // If condition will be parse_equality_expression directly or parse_expression.
    std::shared_ptr<ExpressionNode> parse_equality_expression();
    std::shared_ptr<ExpressionNode> parse_relational_expression();
    std::shared_ptr<ExpressionNode> parse_additive_expression();
    std::shared_ptr<ExpressionNode> parse_multiplicative_expression();
    std::shared_ptr<ExpressionNode> parse_unary_expression(); // Keep '!', unary '-'
    std::shared_ptr<ExpressionNode> parse_postfix_expression(); // No generics
    std::shared_ptr<ExpressionNode> parse_primary_expression(); // Simplified literals

    std::shared_ptr<ObjectCreationExpressionNode> parse_object_creation_expression(); // Simplified
    std::shared_ptr<ArgumentListNode> parse_argument_list();
    std::shared_ptr<ArgumentNode> parse_argument(); // No named args
};

} // namespace Mycelium::Scripting::Lang