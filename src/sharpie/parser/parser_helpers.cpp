#include "sharpie/parser/script_parser.hpp"
#include "sharpie/script_ast.hpp" // For AST nodes and SourceLocation
#include <vector> // For check_token overload
#include <algorithm> // For std::find_if in some potential modifier checks (though not currently used in parse_modifiers directly for adding)

namespace Mycelium::Scripting::Lang
{

// AST Node creation helpers
template <typename T>
std::shared_ptr<T> ScriptParser::make_ast_node(const SourceLocation &start_loc)
{
    auto node = std::make_shared<T>();
    node->location = start_loc;
    // If m_current_parent_node is used: node->parent = m_current_parent_node;
    return node;
}

// Explicit template instantiations if needed by your build system or for specific types.
// This is often not required if definitions are in headers or for typical separate compilation.
// If you encounter linker errors for these template methods, you might need to instantiate them
// for all types T they are used with, or move their definitions to the header (if appropriate).
// For now, assuming standard separate compilation works.
template std::shared_ptr<CompilationUnitNode> ScriptParser::make_ast_node<CompilationUnitNode>(const SourceLocation&);
template std::shared_ptr<UsingDirectiveNode> ScriptParser::make_ast_node<UsingDirectiveNode>(const SourceLocation&);
template std::shared_ptr<NamespaceDeclarationNode> ScriptParser::make_ast_node<NamespaceDeclarationNode>(const SourceLocation&);
template std::shared_ptr<ClassDeclarationNode> ScriptParser::make_ast_node<ClassDeclarationNode>(const SourceLocation&);
template std::shared_ptr<MethodDeclarationNode> ScriptParser::make_ast_node<MethodDeclarationNode>(const SourceLocation&);
template std::shared_ptr<ConstructorDeclarationNode> ScriptParser::make_ast_node<ConstructorDeclarationNode>(const SourceLocation&);
template std::shared_ptr<DestructorDeclarationNode> ScriptParser::make_ast_node<DestructorDeclarationNode>(const SourceLocation&);
template std::shared_ptr<ExternalMethodDeclarationNode> ScriptParser::make_ast_node<ExternalMethodDeclarationNode>(const SourceLocation&);
template std::shared_ptr<FieldDeclarationNode> ScriptParser::make_ast_node<FieldDeclarationNode>(const SourceLocation&);
template std::shared_ptr<ParameterDeclarationNode> ScriptParser::make_ast_node<ParameterDeclarationNode>(const SourceLocation&);
template std::shared_ptr<VariableDeclaratorNode> ScriptParser::make_ast_node<VariableDeclaratorNode>(const SourceLocation&);
template std::shared_ptr<BlockStatementNode> ScriptParser::make_ast_node<BlockStatementNode>(const SourceLocation&);
template std::shared_ptr<IfStatementNode> ScriptParser::make_ast_node<IfStatementNode>(const SourceLocation&);
template std::shared_ptr<WhileStatementNode> ScriptParser::make_ast_node<WhileStatementNode>(const SourceLocation&);
template std::shared_ptr<ForStatementNode> ScriptParser::make_ast_node<ForStatementNode>(const SourceLocation&);
template std::shared_ptr<ReturnStatementNode> ScriptParser::make_ast_node<ReturnStatementNode>(const SourceLocation&);
template std::shared_ptr<LocalVariableDeclarationStatementNode> ScriptParser::make_ast_node<LocalVariableDeclarationStatementNode>(const SourceLocation&);
template std::shared_ptr<ExpressionStatementNode> ScriptParser::make_ast_node<ExpressionStatementNode>(const SourceLocation&);
template std::shared_ptr<LiteralExpressionNode> ScriptParser::make_ast_node<LiteralExpressionNode>(const SourceLocation&);
template std::shared_ptr<IdentifierExpressionNode> ScriptParser::make_ast_node<IdentifierExpressionNode>(const SourceLocation&);
template std::shared_ptr<ThisExpressionNode> ScriptParser::make_ast_node<ThisExpressionNode>(const SourceLocation&);
template std::shared_ptr<ParenthesizedExpressionNode> ScriptParser::make_ast_node<ParenthesizedExpressionNode>(const SourceLocation&);
template std::shared_ptr<ObjectCreationExpressionNode> ScriptParser::make_ast_node<ObjectCreationExpressionNode>(const SourceLocation&);
template std::shared_ptr<UnaryExpressionNode> ScriptParser::make_ast_node<UnaryExpressionNode>(const SourceLocation&);
template std::shared_ptr<BinaryExpressionNode> ScriptParser::make_ast_node<BinaryExpressionNode>(const SourceLocation&);
template std::shared_ptr<AssignmentExpressionNode> ScriptParser::make_ast_node<AssignmentExpressionNode>(const SourceLocation&);
template std::shared_ptr<MemberAccessExpressionNode> ScriptParser::make_ast_node<MemberAccessExpressionNode>(const SourceLocation&);
template std::shared_ptr<IndexerExpressionNode> ScriptParser::make_ast_node<IndexerExpressionNode>(const SourceLocation&);
template std::shared_ptr<MethodCallExpressionNode> ScriptParser::make_ast_node<MethodCallExpressionNode>(const SourceLocation&);
template std::shared_ptr<CastExpressionNode> ScriptParser::make_ast_node<CastExpressionNode>(const SourceLocation&);
template std::shared_ptr<ArgumentListNode> ScriptParser::make_ast_node<ArgumentListNode>(const SourceLocation&);
template std::shared_ptr<ArgumentNode> ScriptParser::make_ast_node<ArgumentNode>(const SourceLocation&);
template std::shared_ptr<TypeNameNode> ScriptParser::make_ast_node<TypeNameNode>(const SourceLocation&);
template std::shared_ptr<QualifiedNameNode> ScriptParser::make_ast_node<QualifiedNameNode>(const SourceLocation&); // Added
template std::shared_ptr<IdentifierNode> ScriptParser::make_ast_node<IdentifierNode>(const SourceLocation&);
template std::shared_ptr<TokenNode> ScriptParser::make_ast_node<TokenNode>(const SourceLocation&);
// Add other types as needed. Reviewing full list:
template std::shared_ptr<TypeParameterNode> ScriptParser::make_ast_node<TypeParameterNode>(const SourceLocation&);
template std::shared_ptr<DeclarationNode> ScriptParser::make_ast_node<DeclarationNode>(const SourceLocation&);
template std::shared_ptr<NamespaceMemberDeclarationNode> ScriptParser::make_ast_node<NamespaceMemberDeclarationNode>(const SourceLocation&);
template std::shared_ptr<TypeDeclarationNode> ScriptParser::make_ast_node<TypeDeclarationNode>(const SourceLocation&);
template std::shared_ptr<StructDeclarationNode> ScriptParser::make_ast_node<StructDeclarationNode>(const SourceLocation&);
template std::shared_ptr<MemberDeclarationNode> ScriptParser::make_ast_node<MemberDeclarationNode>(const SourceLocation&);
template std::shared_ptr<BaseMethodDeclarationNode> ScriptParser::make_ast_node<BaseMethodDeclarationNode>(const SourceLocation&);
template std::shared_ptr<StatementNode> ScriptParser::make_ast_node<StatementNode>(const SourceLocation&);
template std::shared_ptr<ForEachStatementNode> ScriptParser::make_ast_node<ForEachStatementNode>(const SourceLocation&);
template std::shared_ptr<BreakStatementNode> ScriptParser::make_ast_node<BreakStatementNode>(const SourceLocation&);
template std::shared_ptr<ContinueStatementNode> ScriptParser::make_ast_node<ContinueStatementNode>(const SourceLocation&);
template std::shared_ptr<ExpressionNode> ScriptParser::make_ast_node<ExpressionNode>(const SourceLocation&);


void ScriptParser::finalize_node_location(std::shared_ptr<AstNode> node)
{
    if (!node || !node->location.has_value())
        return; 

    node->location->lineEnd = previousTokenInfo.location.lineEnd;
    node->location->columnEnd = previousTokenInfo.location.columnEnd;
}

std::shared_ptr<TokenNode> ScriptParser::create_token_node(TokenType type, const CurrentTokenInfo &token_info)
{
    auto node = make_ast_node<TokenNode>(token_info.location); 
    node->tokenType = type;                                    
    node->text = std::string(token_info.lexeme);
    return node;
}

std::shared_ptr<IdentifierNode> ScriptParser::create_identifier_node(const CurrentTokenInfo &token_info)
{
    auto node = make_ast_node<IdentifierNode>(token_info.location);
    node->name = std::string(token_info.lexeme);
    return node;
}

// Token consumption and checking
bool ScriptParser::check_token(TokenType type) const
{
    return !is_at_end_of_token_stream() && currentTokenInfo.type == type;
}

bool ScriptParser::check_token(const std::vector<TokenType>& types) const
{
    if (is_at_end_of_token_stream())
    {
        return false;
    }
    TokenType current_type = currentTokenInfo.type;
    for (TokenType type : types)
    {
        if (current_type == type)
        {
            return true;
        }
    }
    return false;
}

bool ScriptParser::match_token(TokenType type)
{
    if (check_token(type))
    {
        advance_and_lex(); 
        return true;
    }
    return false;
}

const CurrentTokenInfo &ScriptParser::consume_token(TokenType expected_type, const std::string &error_message)
{
    if (check_token(expected_type))
    {
        advance_and_lex(); 
        return previousTokenInfo;
    }
    else
    {
        record_error_at_current(error_message + " Expected " + token_type_to_string(expected_type) +
                                " but got " + token_type_to_string(currentTokenInfo.type) +
                                " ('" + std::string(currentTokenInfo.lexeme) + "').");
        return currentTokenInfo; 
    }
}

bool ScriptParser::is_at_end_of_token_stream() const
{
    return currentTokenInfo.type == TokenType::EndOfFile;
}

bool ScriptParser::can_parse_as_generic_arguments_followed_by_call()
{
    size_t original_char_offset = currentCharOffset;
    int original_line = currentLine;
    int original_column = currentColumn;
    size_t original_line_start_offset = currentLineStartOffset;
    CurrentTokenInfo original_current_token_info = currentTokenInfo;
    CurrentTokenInfo original_previous_token_info = previousTokenInfo;
    std::vector<ParseError> original_errors_backup = errors;
    errors.clear(); 

    bool is_likely_generic_call = false;

    if (!check_token(TokenType::LessThan))
    {
        errors = original_errors_backup; 
        return false;                    
    }
    advance_and_lex(); 

    if (!check_token(TokenType::GreaterThan))
    { 
        bool first_type_arg = true;
        do
        {
            if (!first_type_arg)
            {
                if (match_token(TokenType::Comma)) { }
                else { goto end_trial_parse_helpers; }
            }
            first_type_arg = false;

            if (check_token(TokenType::Identifier) ||
                check_token({TokenType::Bool, TokenType::Int, TokenType::String, TokenType::Long, TokenType::Double, TokenType::Char, TokenType::Void, TokenType::Float}))
            {
                advance_and_lex(); 
                while (check_token(TokenType::Dot))
                {
                    advance_and_lex(); 
                    if (check_token(TokenType::Identifier)) { advance_and_lex(); }
                    else { goto end_trial_parse_helpers; }
                }
                if (check_token(TokenType::LessThan))
                {
                    int angle_bracket_depth = 1;
                    advance_and_lex(); 
                    while (angle_bracket_depth > 0 && !is_at_end_of_token_stream())
                    {
                        if (check_token(TokenType::LessThan)) angle_bracket_depth++;
                        else if (check_token(TokenType::GreaterThan)) angle_bracket_depth--;
                        if (angle_bracket_depth == 0 && check_token(TokenType::GreaterThan)) { break; }
                        advance_and_lex();
                    }
                    if (angle_bracket_depth != 0) goto end_trial_parse_helpers; 
                }
                if (check_token(TokenType::OpenBracket))
                {
                    advance_and_lex(); 
                    if (!match_token(TokenType::CloseBracket)) goto end_trial_parse_helpers; 
                }
            }
            else { goto end_trial_parse_helpers; }
        } while (!check_token(TokenType::GreaterThan) && !is_at_end_of_token_stream());
    }

    if (match_token(TokenType::GreaterThan))
    { 
        if (check_token(TokenType::OpenParen)) { is_likely_generic_call = true; }
    }

end_trial_parse_helpers:
    currentCharOffset = original_char_offset;
    currentLine = original_line;
    currentColumn = original_column;
    currentLineStartOffset = original_line_start_offset;
    currentTokenInfo = original_current_token_info;
    previousTokenInfo = original_previous_token_info;
    errors = original_errors_backup; 

    return is_likely_generic_call;
}

// Error recording
void ScriptParser::record_error(const std::string &message, const SourceLocation &loc)
{
    errors.emplace_back(message, loc);
}

void ScriptParser::record_error_at_current(const std::string &message)
{
    record_error(message, currentTokenInfo.location);
}

void ScriptParser::record_error_at_previous(const std::string &message)
{
    record_error(message, previousTokenInfo.location);
}

} // namespace Mycelium::Scripting::Lang
