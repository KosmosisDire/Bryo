#pragma once

#include "script_ast.hpp"       // Your new AST structure (even if mostly empty for now)
#include "script_token_types.hpp" // For TokenType enum
#include "script_error.hpp"
#include <string_view>
#include <vector>
#include <optional>
#include <variant>
#include <memory> // For std::shared_ptr
#include <map>

namespace Mycelium::Scripting::Lang
{

// Forward declaration for CompilationUnitNode if not fully defined in script_ast.hpp yet
// struct CompilationUnitNode;

struct CurrentTokenInfo {
    TokenType type = TokenType::Error;
    std::string_view lexeme;
    SourceLocation location;
    std::variant<std::monostate, int64_t, double, std::string, char, bool> literalValue;
};

class ScriptParser {
public:
    ScriptParser(std::string_view source_code, std::string_view fileName);

    std::pair<std::shared_ptr<CompilationUnitNode>, std::vector<ParseError>> parse();

private:
    std::string_view sourceCode;
    std::string_view fileName;
    size_t currentCharOffset;
    int currentLine;
    int currentColumn;
    size_t currentLineStartOffset;

    CurrentTokenInfo currentTokenInfo;
    CurrentTokenInfo previousTokenInfo;
    std::optional<std::string> m_current_class_name; // Added to store current class context for constructor parsing

    std::vector<ParseError> errors;

    // Parsing methods
    std::shared_ptr<UsingDirectiveNode> parse_using_directive();
    std::shared_ptr<NamespaceDeclarationNode> parse_namespace_declaration();
    std::shared_ptr<NamespaceMemberDeclarationNode> parse_namespace_member_declaration(); 
    std::shared_ptr<TypeDeclarationNode> parse_type_declaration(const SourceLocation& decl_start_loc, std::vector<std::pair<ModifierKind, std::shared_ptr<TokenNode>>> modifiers);
    std::shared_ptr<ClassDeclarationNode> parse_class_declaration(const SourceLocation& decl_start_loc, std::vector<std::pair<ModifierKind, std::shared_ptr<TokenNode>>> modifiers);
    std::vector<std::pair<ModifierKind, std::shared_ptr<TokenNode>>> parse_modifiers();
    std::shared_ptr<ParameterDeclarationNode> parse_parameter_declaration();
    std::optional<std::vector<std::shared_ptr<ParameterDeclarationNode>>> parse_parameter_list_content(std::vector<std::shared_ptr<TokenNode>>& commas_list);
    
    // Helper for common parts of method-like declarations (name, generics, parameters)
    // This function will populate the relevant fields in the passed BaseMethodDeclarationNode
    void parse_base_method_declaration_parts(
        std::shared_ptr<BaseMethodDeclarationNode> method_node, // Node to populate (MethodDeclarationNode or ConstructorDeclarationNode)
        const CurrentTokenInfo& method_name_token_info);      // Token info for the method/constructor name

    std::shared_ptr<ConstructorDeclarationNode> parse_constructor_declaration(
        const SourceLocation& decl_start_loc,
        std::vector<std::pair<ModifierKind, std::shared_ptr<TokenNode>>> modifiers,
        const CurrentTokenInfo& constructor_name_token_info); // Name token (should match class name)

    std::shared_ptr<MethodDeclarationNode> parse_method_declaration(
        const SourceLocation& decl_start_loc,
        std::vector<std::pair<ModifierKind, std::shared_ptr<TokenNode>>> modifiers,
        std::shared_ptr<TypeNameNode> return_type, // Explicit return type for methods
        const CurrentTokenInfo& method_name_token_info);

    std::shared_ptr<ExternalMethodDeclarationNode> parse_external_method_declaration();
    std::shared_ptr<MemberDeclarationNode> parse_member_declaration(); // For members inside classes/structs
    std::shared_ptr<FieldDeclarationNode> parse_field_declaration(const SourceLocation& decl_start_loc, std::vector<std::pair<ModifierKind, std::shared_ptr<TokenNode>>> modifiers, std::shared_ptr<TypeNameNode> type);
    std::shared_ptr<IfStatementNode> parse_if_statement();
    std::shared_ptr<WhileStatementNode> parse_while_statement();
    std::shared_ptr<ForStatementNode> parse_for_statement();
    std::shared_ptr<ExpressionNode> parse_assignment_expression();
    std::shared_ptr<ExpressionNode> parse_conditional_expression(); // For ternary operator ?:
    std::shared_ptr<ExpressionNode> parse_logical_or_expression();  // ||
    std::shared_ptr<ExpressionNode> parse_logical_and_expression(); // &&
    std::shared_ptr<ExpressionNode> parse_equality_expression();    // == !=
    std::shared_ptr<ExpressionNode> parse_relational_expression();  // < > <= >= (is, as later)
    std::shared_ptr<ExpressionNode> parse_additive_expression();    // + -
    std::shared_ptr<ExpressionNode> parse_multiplicative_expression(); // * / %
    std::shared_ptr<CompilationUnitNode> parse_compilation_unit();
    std::shared_ptr<ExpressionNode> parse_primary_expression();
    std::shared_ptr<ObjectCreationExpressionNode> parse_object_creation_expression();
    std::shared_ptr<ExpressionNode> parse_unary_expression();
    std::shared_ptr<ExpressionNode> parse_postfix_expression();
    std::shared_ptr<Mycelium::Scripting::Lang::ExpressionStatementNode> parse_expression_statement();
    std::shared_ptr<ExpressionNode> parse_expression();
    std::shared_ptr<TypeNameNode> parse_type_name();
    std::optional<std::shared_ptr<ArgumentListNode>> parse_argument_list();
    std::shared_ptr<BlockStatementNode> parse_block_statement();
    std::shared_ptr<Mycelium::Scripting::Lang::LocalVariableDeclarationStatementNode> parse_local_variable_declaration_statement();
    std::shared_ptr<Mycelium::Scripting::Lang::ReturnStatementNode> parse_return_statement();
    std::shared_ptr<Mycelium::Scripting::Lang::StatementNode> parse_statement();

    // Token consumption and checking
    bool check_token(TokenType type) const;
    bool check_token(const std::vector<TokenType>& types) const;
    bool match_token(TokenType type);
    const CurrentTokenInfo& consume_token(TokenType type, const std::string& error_message);
    bool is_at_end_of_token_stream() const;
    bool can_parse_as_generic_arguments_followed_by_call();

    // AST Node creation helpers
    template<typename T>
    std::shared_ptr<T> make_ast_node(const SourceLocation& start_loc);
    void finalize_node_location(std::shared_ptr<AstNode> node);
    std::shared_ptr<TokenNode> create_token_node(TokenType type, const CurrentTokenInfo& token_info);
    std::shared_ptr<IdentifierNode> create_identifier_node(const CurrentTokenInfo& token_info);

    // Lexing
    CurrentTokenInfo lex_number_literal();
    CurrentTokenInfo lex_string_literal();
    CurrentTokenInfo lex_char_literal();
    CurrentTokenInfo lex_identifier_or_keyword();
    CurrentTokenInfo lex_operator_or_punctuation();
    void skip_whitespace_and_comments();
    void advance_and_lex();

    // Character consumption helpers
    char peek_char(size_t offset = 0) const;
    char consume_char();
    bool is_at_end_of_source() const;

    // Error recording
    void record_error(const std::string& message, const SourceLocation& loc);
    void record_error_at_current(const std::string& message);
    void record_error_at_previous(const std::string& message);
};

} // namespace Mycelium::Scripting::Lang
