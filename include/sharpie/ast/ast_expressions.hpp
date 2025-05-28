#pragma once

#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <variant>
#include "ast_base.hpp"   // For AstNode, TokenNode, IdentifierNode, Enums (via transitive include)
#include "ast_types.hpp"  // For TypeNameNode

namespace Mycelium::Scripting::Lang
{
    // Base class for all expressions
    struct ExpressionNode : AstNode
    {
    };

    struct LiteralExpressionNode : ExpressionNode
    {
        LiteralKind kind;
        std::string valueText;           // The raw string value from the token
        std::shared_ptr<TokenNode> token; // The actual token for location and raw text
    };

    struct IdentifierExpressionNode : ExpressionNode
    {
        std::shared_ptr<IdentifierNode> identifier;
    };

    struct ParenthesizedExpressionNode : ExpressionNode
    {
        std::shared_ptr<TokenNode> openParenToken;
        std::shared_ptr<ExpressionNode> expression;
        std::shared_ptr<TokenNode> closeParenToken;
    };

    struct UnaryExpressionNode : ExpressionNode
    {
        UnaryOperatorKind opKind;
        std::shared_ptr<TokenNode> operatorToken;
        std::shared_ptr<ExpressionNode> operand;
        bool isPostfix; // Differentiates ++x from x++
    };

    struct BinaryExpressionNode : ExpressionNode
    {
        std::shared_ptr<ExpressionNode> left;
        BinaryOperatorKind opKind;
        std::shared_ptr<TokenNode> operatorToken;
        std::shared_ptr<ExpressionNode> right;
    };

    struct AssignmentExpressionNode : ExpressionNode
    {
        std::shared_ptr<ExpressionNode> target;
        AssignmentOperatorKind opKind;
        std::shared_ptr<TokenNode> operatorToken;
        std::shared_ptr<ExpressionNode> source;
    };

    struct ArgumentNode : AstNode // Note: ArgumentNode itself is not an ExpressionNode but part of call/creation expressions
    {
        std::optional<std::shared_ptr<IdentifierNode>> nameLabel; // For named arguments
        std::optional<std::shared_ptr<TokenNode>> colonToken;     // For named arguments
        std::shared_ptr<ExpressionNode> expression;
    };

    struct ArgumentListNode : AstNode // Used by MethodCall and ObjectCreation, also not an ExpressionNode itself
    {
        std::shared_ptr<TokenNode> openParenToken;
        std::vector<std::shared_ptr<ArgumentNode>> arguments;
        std::vector<std::shared_ptr<TokenNode>> commas;
        std::shared_ptr<TokenNode> closeParenToken;
    };

    struct MethodCallExpressionNode : ExpressionNode
    {
        std::shared_ptr<ExpressionNode> target; // e.g., IdentifierExpressionNode, MemberAccessExpressionNode

        std::optional<std::shared_ptr<TokenNode>> genericOpenAngleBracketToken;
        std::optional<std::vector<std::shared_ptr<TypeNameNode>>> typeArguments;
        std::optional<std::vector<std::shared_ptr<TokenNode>>> typeArgumentCommas;
        std::optional<std::shared_ptr<TokenNode>> genericCloseAngleBracketToken;

        std::shared_ptr<ArgumentListNode> argumentList;
    };

    struct MemberAccessExpressionNode : ExpressionNode
    {
        std::shared_ptr<ExpressionNode> target;
        std::shared_ptr<TokenNode> dotToken; // Or other access operator token
        std::shared_ptr<IdentifierNode> memberName;
    };

    struct ObjectCreationExpressionNode : ExpressionNode
    {
        std::shared_ptr<TokenNode> newKeyword;
        std::shared_ptr<TypeNameNode> type;
        std::optional<std::shared_ptr<ArgumentListNode>> argumentList; // Arguments can be optional
    };

    struct ThisExpressionNode : ExpressionNode
    {
        std::shared_ptr<TokenNode> thisKeyword;
    };

    struct CastExpressionNode : ExpressionNode
    {
        std::shared_ptr<TokenNode> openParenToken;
        std::shared_ptr<TypeNameNode> targetType;
        std::shared_ptr<TokenNode> closeParenToken;
        std::shared_ptr<ExpressionNode> expression; // The expression being cast
    };

    struct IndexerExpressionNode : ExpressionNode
    {
        std::shared_ptr<ExpressionNode> target;
        std::shared_ptr<TokenNode> openBracketToken;
        std::shared_ptr<ExpressionNode> indexExpression; // For simple T[idx]
        // For T[idx1, idx2], you'd use something like std::vector<std::shared_ptr<ArgumentNode>> arguments;
        // and std::vector<std::shared_ptr<TokenNode>> commas;
        std::shared_ptr<TokenNode> closeBracketToken;
    };
}
