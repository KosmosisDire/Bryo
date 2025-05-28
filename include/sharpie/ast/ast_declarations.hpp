#pragma once

#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <variant>
#include "ast_base.hpp"       // For AstNode, IdentifierNode, TokenNode, ModifierKind
#include "ast_types.hpp"      // For TypeNameNode, QualifiedNameNode, TypeParameterNode
#include "ast_expressions.hpp" // For ExpressionNode
#include "ast_statements.hpp"  // For BlockStatementNode

namespace Mycelium::Scripting::Lang
{
    // Base class for all declarations
    struct DeclarationNode : AstNode
    {
        std::shared_ptr<IdentifierNode> name;
        std::vector<std::pair<ModifierKind, std::shared_ptr<TokenNode>>> modifiers; // Store token for each modifier
    };

    struct NamespaceMemberDeclarationNode : DeclarationNode
    {
    };

    struct UsingDirectiveNode : AstNode // Not strictly a DeclarationNode but often grouped with them
    {
        std::shared_ptr<TokenNode> usingKeyword;
        std::variant<std::shared_ptr<IdentifierNode>, std::shared_ptr<QualifiedNameNode>> namespaceName;
        std::shared_ptr<TokenNode> semicolonToken;
    };

    struct NamespaceDeclarationNode : NamespaceMemberDeclarationNode
    {
        std::shared_ptr<TokenNode> namespaceKeyword;
        std::vector<std::shared_ptr<UsingDirectiveNode>> usings;
        std::shared_ptr<TokenNode> openBraceToken;
        std::vector<std::shared_ptr<NamespaceMemberDeclarationNode>> members;
        std::shared_ptr<TokenNode> closeBraceToken;
    };

    // Forward declare ExternalMethodDeclarationNode for CompilationUnitNode
    struct ExternalMethodDeclarationNode; 

    struct CompilationUnitNode : AstNode // Top-level node for a file
    {
        std::vector<std::shared_ptr<UsingDirectiveNode>> usings;
        std::vector<std::shared_ptr<ExternalMethodDeclarationNode>> externs;
        std::optional<std::shared_ptr<TokenNode>> fileScopeNamespaceKeyword;
        std::optional<std::variant<std::shared_ptr<IdentifierNode>, std::shared_ptr<QualifiedNameNode>>> fileScopedNamespaceName;
        std::optional<std::shared_ptr<TokenNode>> fileScopeNamespaceSemicolon;
        std::vector<std::shared_ptr<NamespaceMemberDeclarationNode>> members;
    };

    struct MemberDeclarationNode : DeclarationNode // Base for class/struct members
    {
        std::optional<std::shared_ptr<TypeNameNode>> type; // Return type for methods, type for fields
    };

    struct TypeDeclarationNode : NamespaceMemberDeclarationNode // Base for class, struct
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

    struct VariableDeclaratorNode : AstNode // Used for fields and local variables
    {
        std::shared_ptr<IdentifierNode> name;
        std::optional<std::shared_ptr<TokenNode>> equalsToken;
        std::optional<std::shared_ptr<ExpressionNode>> initializer;
    };

    struct FieldDeclarationNode : MemberDeclarationNode
    {
        std::vector<std::shared_ptr<VariableDeclaratorNode>> declarators;
        std::vector<std::shared_ptr<TokenNode>> declaratorCommas;
        std::shared_ptr<TokenNode> semicolonToken;
    };

    struct ParameterDeclarationNode : DeclarationNode
    {
        std::shared_ptr<TypeNameNode> type;
        std::optional<std::shared_ptr<TokenNode>> equalsToken;
        std::optional<std::shared_ptr<ExpressionNode>> defaultValue;
    };

    struct BaseMethodDeclarationNode : MemberDeclarationNode // Common for Method, Constructor, Destructor, Extern
    {
        std::shared_ptr<TokenNode> externKeyword; // Only for extern methods, but part of base for structure
        std::optional<std::shared_ptr<TokenNode>> genericOpenAngleBracketToken;
        std::vector<std::shared_ptr<TypeParameterNode>> typeParameters;
        std::vector<std::shared_ptr<TokenNode>> typeParameterCommas;
        std::optional<std::shared_ptr<TokenNode>> genericCloseAngleBracketToken;
        std::shared_ptr<TokenNode> openParenToken;
        std::vector<std::shared_ptr<ParameterDeclarationNode>> parameters;
        std::vector<std::shared_ptr<TokenNode>> parameterCommas;
        std::shared_ptr<TokenNode> closeParenToken;
        std::optional<std::shared_ptr<BlockStatementNode>> body; // Optional for extern methods or interface methods
        std::shared_ptr<TokenNode> semicolonToken; // For methods without body (extern, abstract, interface)
    };

    struct MethodDeclarationNode : BaseMethodDeclarationNode
    {
    };

    struct ConstructorDeclarationNode : BaseMethodDeclarationNode
    {
    };

    struct DestructorDeclarationNode : BaseMethodDeclarationNode
    {
        std::shared_ptr<TokenNode> tildeToken; // Token for '~'
    };

    struct ExternalMethodDeclarationNode : BaseMethodDeclarationNode // Definition
    {
    };
}
