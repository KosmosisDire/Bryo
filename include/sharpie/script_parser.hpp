#pragma once

#include "script_ast.hpp"
#include "script_token.hpp"
#include <vector>
#include <memory>
#include <optional>
#include <stdexcept> 
#include <string>

namespace Mycelium::Scripting::Lang
{

class ParseError : public std::runtime_error
{
public:
    ParseError(const std::string& message, const SourceLocation& location)
        : std::runtime_error(format_message(message, location)), m_location(location)
    {
    }

    ParseError(const std::string& message, const Token& token)
        : std::runtime_error(format_message(message, token.location)), m_location(token.location)
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

    ParseError create_error(const Token& token, const std::string& message);
    ParseError create_error(const std::string& message); 

    template <typename T, typename... Args>
    std::shared_ptr<T> make_ast_node(Args&&... args)
    {
        auto node = std::make_shared<T>(std::forward<Args>(args)...);
        node->parent = m_currentParentNode;
        if (m_currentIndex < m_tokens.size())
        {
            node->location = current_token().location; 
        }
        return node;
    }

    template <typename NodeType, typename Func>
    std::shared_ptr<NodeType> with_parent_context(std::shared_ptr<NodeType> newParentNode, Func parserFunc)
    {
        auto oldParent = m_currentParentNode;
        m_currentParentNode = newParentNode;
        auto resultNode = parserFunc(); 
        m_currentParentNode = oldParent;
        return resultNode; 
    }

    void finalize_node_location(std::shared_ptr<AstNode> node, const SourceLocation& startLocation);
    void finalize_node_location(std::shared_ptr<AstNode> node, const Token& startToken);

    std::shared_ptr<UsingDirectiveNode> parse_using_directive();
    std::shared_ptr<NamespaceMemberDeclarationNode> parse_file_level_declaration();
    std::string parse_file_scoped_namespace_directive(); 

    std::shared_ptr<ClassDeclarationNode> parse_class_declaration(std::vector<ModifierKind> modifiers, const Token& startToken, const std::string& name);
    std::shared_ptr<TypeDeclarationNode> parse_type_declaration(std::vector<ModifierKind> modifiers, const Token& nameToken);

    std::vector<ModifierKind> parse_modifiers();
    std::string parse_identifier_name(const std::string& contextMessage = "Expected identifier");
    std::string parse_qualified_identifier(const std::string& contextMessageStart, const std::string& contextMessagePart);

public: 
    std::shared_ptr<TypeNameNode> parse_type_name();
    std::shared_ptr<MemberDeclarationNode> parse_member_declaration(const Token& startToken);
    std::shared_ptr<FieldDeclarationNode> parse_field_declaration(std::vector<ModifierKind> mods, std::shared_ptr<TypeNameNode> type, const Token& start);
    std::shared_ptr<ExpressionNode> parse_expression();
    std::shared_ptr<ExpressionNode> parse_assignment_expression();
    std::shared_ptr<ExpressionNode> parse_primary_expression();
    std::vector<std::shared_ptr<VariableDeclaratorNode>> parse_variable_declarator_list(std::shared_ptr<TypeNameNode> type);
    std::shared_ptr<VariableDeclaratorNode> parse_variable_declarator(std::shared_ptr<TypeNameNode> type);
    std::shared_ptr<LocalVariableDeclarationStatementNode> parse_local_variable_declaration_statement();
    std::shared_ptr<StatementNode> parse_statement();
    std::shared_ptr<BlockStatementNode> parse_block_statement();
    std::shared_ptr<MethodDeclarationNode> parse_method_declaration(std::vector<ModifierKind> mods, std::shared_ptr<TypeNameNode> retType, const std::string& name, const Token& start);
    std::vector<std::shared_ptr<ParameterDeclarationNode>> parse_parameter_list();
    std::shared_ptr<ParameterDeclarationNode> parse_parameter_declaration();
    std::shared_ptr<ReturnStatementNode> parse_return_statement();
    std::shared_ptr<ExpressionStatementNode> parse_expression_statement();
    std::shared_ptr<ExpressionNode> parse_unary_expression();
    std::shared_ptr<ExpressionNode> parse_logical_or_expression();
    std::shared_ptr<ExpressionNode> parse_logical_and_expression();
    std::shared_ptr<ExpressionNode> parse_equality_expression();
    std::shared_ptr<ExpressionNode> parse_relational_expression();
    std::shared_ptr<ExpressionNode> parse_additive_expression();
    std::shared_ptr<ExpressionNode> parse_multiplicative_expression();
    std::shared_ptr<ExpressionNode> parse_postfix_expression();
    std::shared_ptr<ArgumentListNode> parse_argument_list();
    std::shared_ptr<ArgumentNode> parse_argument();
    std::shared_ptr<ObjectCreationExpressionNode> parse_object_creation_expression();
    std::shared_ptr<ConstructorDeclarationNode> parse_constructor_declaration(std::vector<ModifierKind> mods, const std::string& name, const Token& start);
    std::vector<std::shared_ptr<TypeParameterNode>> parse_optional_type_parameter_list();
    std::shared_ptr<TypeParameterNode> parse_type_parameter();
    std::optional<std::vector<std::shared_ptr<TypeNameNode>>> parse_optional_type_argument_list();
    std::vector<std::shared_ptr<TypeNameNode>> parse_base_list();
    std::shared_ptr<IfStatementNode> parse_if_statement();
    std::shared_ptr<WhileStatementNode> parse_while_statement();
    std::shared_ptr<ForStatementNode> parse_for_statement();
    std::shared_ptr<ForEachStatementNode> parse_for_each_statement();
    std::shared_ptr<BreakStatementNode> parse_break_statement();
    std::shared_ptr<ContinueStatementNode> parse_continue_statement();
    std::shared_ptr<StructDeclarationNode> parse_struct_declaration(std::vector<ModifierKind> mods, const Token& start, const std::string& name);
};

} // namespace Mycelium::Scripting::Lang