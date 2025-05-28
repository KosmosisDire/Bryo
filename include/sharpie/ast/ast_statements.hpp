#pragma once

#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <variant>
#include "ast_base.hpp"       // For AstNode, TokenNode, IdentifierNode
#include "ast_types.hpp"      // For TypeNameNode
#include "ast_expressions.hpp" // For ExpressionNode

namespace Mycelium::Scripting::Lang
{
    // Forward declaration for nodes that might be defined in ast_declarations.hpp but used here
    struct VariableDeclaratorNode; // Used in LocalVariableDeclarationStatementNode
    // BlockStatementNode is defined in this file.

    // Base class for all statements
    struct StatementNode : AstNode
    {
    };

    struct BlockStatementNode : StatementNode // Definition moved here
    {
        std::shared_ptr<TokenNode> openBraceToken;
        std::vector<std::shared_ptr<StatementNode>> statements;
        std::shared_ptr<TokenNode> closeBraceToken;
    };

    struct ExpressionStatementNode : StatementNode
    {
        std::shared_ptr<ExpressionNode> expression;
        std::shared_ptr<TokenNode> semicolonToken;
    };

    struct IfStatementNode : StatementNode
    {
        std::shared_ptr<TokenNode> ifKeyword;
        std::shared_ptr<TokenNode> openParenToken;
        std::shared_ptr<ExpressionNode> condition;
        std::shared_ptr<TokenNode> closeParenToken;
        std::shared_ptr<StatementNode> thenStatement;
        std::optional<std::shared_ptr<TokenNode>> elseKeyword;
        std::optional<std::shared_ptr<StatementNode>> elseStatement;
    };

    struct WhileStatementNode : StatementNode
    {
        std::shared_ptr<TokenNode> whileKeyword;
        std::shared_ptr<TokenNode> openParenToken;
        std::shared_ptr<ExpressionNode> condition;
        std::shared_ptr<TokenNode> closeParenToken;
        std::shared_ptr<StatementNode> body;
    };

    struct LocalVariableDeclarationStatementNode : StatementNode
    {
        std::optional<std::shared_ptr<TokenNode>> varKeywordToken; // if isVarDeclaration
        std::shared_ptr<TypeNameNode> type;                          // If varKeywordToken is present, TypeNameNode might just wrap the 'var' token or be simplified

        std::vector<std::shared_ptr<VariableDeclaratorNode>> declarators;
        std::vector<std::shared_ptr<TokenNode>> declaratorCommas;
        std::shared_ptr<TokenNode> semicolonToken;
        bool is_var_declaration() const { return varKeywordToken.has_value(); }
    };

    struct ForStatementNode : StatementNode
    {
        std::shared_ptr<TokenNode> forKeyword;
        std::shared_ptr<TokenNode> openParenToken;

        // For loop initializer can be a declaration or list of expressions
        std::variant<std::shared_ptr<LocalVariableDeclarationStatementNode>, // Semicolon is part of this node
                     std::vector<std::shared_ptr<ExpressionNode>>>
            initializers;
        std::vector<std::shared_ptr<TokenNode>> initializerCommas; // Only if expression list
        std::shared_ptr<TokenNode> firstSemicolonToken;           // Semicolon after initializers (if not decl) or after declaration

        std::optional<std::shared_ptr<ExpressionNode>> condition;
        std::shared_ptr<TokenNode> secondSemicolonToken;

        std::vector<std::shared_ptr<ExpressionNode>> incrementors;
        std::vector<std::shared_ptr<TokenNode>> incrementorCommas;
        std::shared_ptr<TokenNode> closeParenToken;
        std::shared_ptr<StatementNode> body;
    };

    struct ForEachStatementNode : StatementNode
    {
        std::shared_ptr<TokenNode> foreachKeyword;
        std::shared_ptr<TokenNode> openParenToken;
        std::shared_ptr<TypeNameNode> variableType; // Could be 'var'
        std::shared_ptr<IdentifierNode> variableName;
        std::shared_ptr<TokenNode> inKeyword;
        std::shared_ptr<ExpressionNode> collection;
        std::shared_ptr<TokenNode> closeParenToken;
        std::shared_ptr<StatementNode> body;
    };

    struct ReturnStatementNode : StatementNode
    {
        std::shared_ptr<TokenNode> returnKeyword;
        std::optional<std::shared_ptr<ExpressionNode>> expression;
        std::shared_ptr<TokenNode> semicolonToken;
    };

    struct BreakStatementNode : StatementNode
    {
        std::shared_ptr<TokenNode> breakKeyword;
        std::shared_ptr<TokenNode> semicolonToken;
    };
    struct ContinueStatementNode : StatementNode
    {
        std::shared_ptr<TokenNode> continueKeyword;
        std::shared_ptr<TokenNode> semicolonToken;
    };
}
