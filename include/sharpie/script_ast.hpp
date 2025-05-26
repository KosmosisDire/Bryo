#pragma once

#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <iostream>
#include <sstream>
#include <variant>
#include "script_ast_location.hpp"
#include "script_token_types.hpp"

namespace Mycelium::Scripting::Lang
{
    struct AstNode;
    struct TokenNode;
    struct IdentifierNode;
    struct TypeNameNode;
    struct QualifiedNameNode;
    struct ExpressionNode;
    struct StatementNode;
    struct BlockStatementNode;
    struct DeclarationNode;
    struct NamespaceMemberDeclarationNode;
    struct MemberDeclarationNode;
    struct ParameterDeclarationNode;
    struct VariableDeclaratorNode;
    struct ArgumentListNode;
    struct UsingDirectiveNode;
    struct NamespaceDeclarationNode;
    struct CompilationUnitNode;
    struct TypeDeclarationNode;
    struct ClassDeclarationNode;
    struct StructDeclarationNode;
    struct FieldDeclarationNode;
    struct MethodDeclarationNode;
    struct ConstructorDeclarationNode;
    struct ExternalMethodDeclarationNode;
    struct LocalVariableDeclarationStatementNode;
    struct ExpressionStatementNode;
    struct IfStatementNode;
    struct WhileStatementNode;
    struct ForStatementNode;
    struct ForEachStatementNode;
    struct ReturnStatementNode;
    struct BreakStatementNode;
    struct ContinueStatementNode;
    struct LiteralExpressionNode;
    struct IdentifierExpressionNode;
    struct UnaryExpressionNode;
    struct BinaryExpressionNode;
    struct AssignmentExpressionNode;
    struct MethodCallExpressionNode;
    struct MemberAccessExpressionNode;
    struct ObjectCreationExpressionNode;
    struct ThisExpressionNode;
    struct TypeParameterNode;
    struct ArgumentNode;
    struct CastExpressionNode;
    struct IndexerExpressionNode;

    enum class ModifierKind
    {
        Public,
        Private,
        Protected,
        Internal,
        Static,
        Readonly
    };
    enum class LiteralKind
    {
        Integer,
        Long,
        Float,
        Double,
        String,
        Char,
        Boolean,
        Null,
    };
    enum class UnaryOperatorKind
    {
        LogicalNot,
        UnaryPlus,
        UnaryMinus,
        PreIncrement,
        PostIncrement,
        PreDecrement,
        PostDecrement
    };
    enum class BinaryOperatorKind
    {
        Add,
        Subtract,
        Multiply,
        Divide,
        Modulo,
        LogicalAnd,
        LogicalOr,
        Equals,
        NotEquals,
        LessThan,
        GreaterThan,
        LessThanOrEqual,
        GreaterThanOrEqual
    };
    enum class AssignmentOperatorKind
    {
        Assign,
        AddAssign,
        SubtractAssign,
        MultiplyAssign,
        DivideAssign,
        ModuloAssign
    };

    inline std::string to_string(ModifierKind kind)
    {
        switch (kind)
        {
        case ModifierKind::Public: return "public";
        case ModifierKind::Private: return "private";
        case ModifierKind::Protected: return "protected";
        case ModifierKind::Internal: return "internal";
        case ModifierKind::Static: return "static";
        case ModifierKind::Readonly: return "readonly";
        default: return "unknown";
        }
    }

    inline std::string modifiers_to_string(const std::vector<std::pair<ModifierKind, std::shared_ptr<TokenNode>>> &modifiers)
    {
        if (modifiers.empty())
            return "";
        std::stringstream ss;
        for (size_t i = 0; i < modifiers.size(); ++i)
        {
            ss << to_string(modifiers[i].first) << (i == modifiers.size() - 1 ? "" : " ");
        }
        return ss.str();
    }
    inline std::string to_string(LiteralKind kind)
    {
        switch (kind)
        {
        case LiteralKind::Integer: return "integer";
        case LiteralKind::String: return "string";
        case LiteralKind::Boolean: return "boolean";
        case LiteralKind::Null: return "null";
        case LiteralKind::Char: return "char";
        case LiteralKind::Float: return "float";
        default: return "unknown";
        }
    }
    inline std::string to_string(UnaryOperatorKind op)
    {
        switch (op)
        {
        case UnaryOperatorKind::LogicalNot: return "!";
        case UnaryOperatorKind::UnaryPlus: return "+";
        case UnaryOperatorKind::UnaryMinus: return "-";
        case UnaryOperatorKind::PreIncrement: return "++";
        case UnaryOperatorKind::PostIncrement: return "++";
        case UnaryOperatorKind::PreDecrement: return "--";
        case UnaryOperatorKind::PostDecrement: return "--";
        default: return "unknown";
        }
    }
    inline std::string to_string(BinaryOperatorKind op)
    {
        switch (op)
        {
        case BinaryOperatorKind::Add: return "+";
        case BinaryOperatorKind::Subtract: return "-";
        case BinaryOperatorKind::Multiply: return "*";
        case BinaryOperatorKind::Divide: return "/";
        case BinaryOperatorKind::Modulo: return "%";
        case BinaryOperatorKind::LogicalAnd: return "&&";
        case BinaryOperatorKind::LogicalOr: return "||";
        case BinaryOperatorKind::Equals: return "==";
        case BinaryOperatorKind::NotEquals: return "!=";
        case BinaryOperatorKind::LessThan: return "<";
        case BinaryOperatorKind::GreaterThan: return ">";
        case BinaryOperatorKind::LessThanOrEqual: return "<=";
        case BinaryOperatorKind::GreaterThanOrEqual: return ">=";
        default: return "unknown";
        }
    }
    inline std::string to_string(AssignmentOperatorKind op)
    {
        switch (op)
        {
        case AssignmentOperatorKind::Assign: return "=";
        case AssignmentOperatorKind::AddAssign: return "+=";
        case AssignmentOperatorKind::SubtractAssign: return "-=";
        case AssignmentOperatorKind::MultiplyAssign: return "*=";
        case AssignmentOperatorKind::DivideAssign: return "/=";
        case AssignmentOperatorKind::ModuloAssign: return "%=";
        default: return "unknown";
        }
    }

    // Base AST Node
    struct AstNode
    {
        static inline long long idCounter = 0;
        std::weak_ptr<AstNode> parent;
        std::optional<SourceLocation> location; // Overall location of the node
        long long id;

        AstNode();
        virtual ~AstNode() = default;
    };

    // New: Represents a lexical token
    struct TokenNode : AstNode
    {
        std::string text;
        TokenType tokenType;
    };

    // New: Represents an identifier
    struct IdentifierNode : AstNode // Could also be an ExpressionNode if identifiers are always expressions
    {
        std::string name;
        // 'location' from AstNode will store its precise location.

        IdentifierNode(std::string n = "") : name(std::move(n)) {}
    };

    // --- Names and Types ---
    struct QualifiedNameNode; // Forward declare for TypeNameNode

    struct TypeNameNode : AstNode
    {
        // A TypeName can be a simple Identifier or a QualifiedName
        std::variant<std::shared_ptr<IdentifierNode>, std::shared_ptr<QualifiedNameNode>> name_segment;

        std::optional<std::shared_ptr<TokenNode>> openAngleBracketToken;
        std::vector<std::shared_ptr<TypeNameNode>> typeArguments;
        std::vector<std::shared_ptr<TokenNode>> typeArgumentCommas;
        std::optional<std::shared_ptr<TokenNode>> closeAngleBracketToken;

        std::optional<std::shared_ptr<TokenNode>> openSquareBracketToken;
        std::optional<std::shared_ptr<TokenNode>> closeSquareBracketToken; // Assuming single rank array T[]
        bool is_array() const { return openSquareBracketToken.has_value(); }
    };

    struct QualifiedNameNode : AstNode // Used within TypeNameNode or for namespace names
    {
        std::shared_ptr<TypeNameNode> left; // Could be IdentifierNode or another QualifiedNameNode
        std::shared_ptr<TokenNode> dotToken;
        std::shared_ptr<IdentifierNode> right;
    };

    struct TypeParameterNode : AstNode
    {
        std::shared_ptr<IdentifierNode> name;
        // Potentially: std::vector<TypeNameNode> constraints;
    };

    // --- Declarations ---
    struct DeclarationNode : AstNode
    {
        std::shared_ptr<IdentifierNode> name;
        std::vector<std::pair<ModifierKind, std::shared_ptr<TokenNode>>> modifiers; // Store token for each modifier
    };

    struct NamespaceMemberDeclarationNode : DeclarationNode
    {
    };

    struct UsingDirectiveNode : AstNode
    {
        std::shared_ptr<TokenNode> usingKeyword;
        // Using fully qualified name structure
        std::variant<std::shared_ptr<IdentifierNode>, std::shared_ptr<QualifiedNameNode>> namespaceName;
        std::shared_ptr<TokenNode> semicolonToken;
    };

    struct NamespaceDeclarationNode : NamespaceMemberDeclarationNode
    {
        std::shared_ptr<TokenNode> namespaceKeyword;
        // 'name' inherited from NamespaceMemberDeclarationNode which uses IdentifierNode.
        // For qualified namespace names, the parser would build nested NamespaceDeclarationNodes
        // or this 'name' itself could be a QualifiedNameNode. For simplicity, keep as IdentifierNode.
        std::vector<std::shared_ptr<UsingDirectiveNode>> usings;
        std::shared_ptr<TokenNode> openBraceToken;
        std::vector<std::shared_ptr<NamespaceMemberDeclarationNode>> members;
        std::shared_ptr<TokenNode> closeBraceToken;
    };

    struct CompilationUnitNode : AstNode
    {
        std::vector<std::shared_ptr<UsingDirectiveNode>> usings;
        std::vector<std::shared_ptr<ExternalMethodDeclarationNode>> externs;
        // For file-scoped namespaces:
        std::optional<std::shared_ptr<TokenNode>> fileScopeNamespaceKeyword;
        std::optional<std::variant<std::shared_ptr<IdentifierNode>, std::shared_ptr<QualifiedNameNode>>> fileScopedNamespaceName;
        std::optional<std::shared_ptr<TokenNode>> fileScopeNamespaceSemicolon;

        std::vector<std::shared_ptr<NamespaceMemberDeclarationNode>> members; // Regular (braced) namespaces or types
    };

    struct TypeDeclarationNode : NamespaceMemberDeclarationNode
    {
        std::shared_ptr<TokenNode> typeKeywordToken; // 'class', 'struct'

        std::optional<std::shared_ptr<TokenNode>> genericOpenAngleBracketToken;
        std::vector<std::shared_ptr<TypeParameterNode>> typeParameters;
        std::vector<std::shared_ptr<TokenNode>> typeParameterCommas;
        std::optional<std::shared_ptr<TokenNode>> genericCloseAngleBracketToken;

        std::optional<std::shared_ptr<TokenNode>> baseListColonToken;
        std::vector<std::shared_ptr<TypeNameNode>> baseList;
        std::vector<std::shared_ptr<TokenNode>> baseListCommas;

        std::shared_ptr<TokenNode> openBraceToken;
        std::vector<std::shared_ptr<MemberDeclarationNode>> members;
        std::shared_ptr<TokenNode> closeBraceToken;
    };


    struct ClassDeclarationNode : TypeDeclarationNode
    {
    };
    struct StructDeclarationNode : TypeDeclarationNode
    {
    };

    struct MemberDeclarationNode : DeclarationNode
    {
        std::optional<std::shared_ptr<TypeNameNode>> type; // Return type for methods, type for fields
    };

    struct VariableDeclaratorNode : AstNode // Used for fields and local variables
    {
        std::shared_ptr<IdentifierNode> name;
        std::optional<std::shared_ptr<TokenNode>> equalsToken;
        std::optional<std::shared_ptr<ExpressionNode>> initializer;
    };

    struct FieldDeclarationNode : MemberDeclarationNode
    {
        // 'type' from MemberDeclarationNode is the common type.
        // 'name' from DeclarationNode is often not used if multiple declarators.
        std::vector<std::shared_ptr<VariableDeclaratorNode>> declarators;
        std::vector<std::shared_ptr<TokenNode>> declaratorCommas;
        std::shared_ptr<TokenNode> semicolonToken;
    };

    struct ParameterDeclarationNode : DeclarationNode // Name from DeclarationNode
    {
        std::shared_ptr<TypeNameNode> type;
        std::optional<std::shared_ptr<TokenNode>> equalsToken;
        std::optional<std::shared_ptr<ExpressionNode>> defaultValue;
    };

    struct BaseMethodDeclarationNode : MemberDeclarationNode // Common parts for Method and Constructor
    {
        std::shared_ptr<TokenNode> externKeyword;
        std::optional<std::shared_ptr<TokenNode>> genericOpenAngleBracketToken;
        std::vector<std::shared_ptr<TypeParameterNode>> typeParameters;
        std::vector<std::shared_ptr<TokenNode>> typeParameterCommas;
        std::optional<std::shared_ptr<TokenNode>> genericCloseAngleBracketToken;

        std::shared_ptr<TokenNode> openParenToken;
        std::vector<std::shared_ptr<ParameterDeclarationNode>> parameters;
        std::vector<std::shared_ptr<TokenNode>> parameterCommas;
        std::shared_ptr<TokenNode> closeParenToken;
        std::optional<std::shared_ptr<BlockStatementNode>> body;
        std::shared_ptr<TokenNode> semicolonToken;
    };

    struct MethodDeclarationNode : BaseMethodDeclarationNode
    {
    };

    struct ConstructorDeclarationNode : BaseMethodDeclarationNode
    {
    };

    struct ExternalMethodDeclarationNode : BaseMethodDeclarationNode
    {
    };

    // --- Statements ---
    struct StatementNode : AstNode
    {
    };

    struct BlockStatementNode : StatementNode
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

    // --- Expressions ---
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

    struct ArgumentNode : AstNode
    {
        std::optional<std::shared_ptr<IdentifierNode>> nameLabel; // For named arguments
        std::optional<std::shared_ptr<TokenNode>> colonToken;     // For named arguments
        std::shared_ptr<ExpressionNode> expression;
    };

    struct ArgumentListNode : AstNode // Used by MethodCall and ObjectCreation
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