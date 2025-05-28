#pragma once

#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <variant>
#include "ast_base.hpp" // For AstNode, IdentifierNode, TokenNode

namespace Mycelium::Scripting::Lang
{
    // Forward declaration for QualifiedNameNode if it's used by TypeNameNode before its definition
    // However, in this file, TypeNameNode is defined first and uses QualifiedNameNode.
    // So, QualifiedNameNode needs to be defined or forward-declared before TypeNameNode.
    // Let's define QualifiedNameNode first or ensure it's forward-declared if it remains after TypeNameNode.
    // For simplicity, we can forward declare it here if its full definition comes later in this file,
    // or ensure it's defined before its first use in TypeNameNode.

    // To maintain consistency with the original script_ast.hpp structure where TypeNameNode comes first:
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
        std::shared_ptr<TypeNameNode> left; // Could be IdentifierNode or another QualifiedNameNode (actually TypeNameNode)
        std::shared_ptr<TokenNode> dotToken;
        std::shared_ptr<IdentifierNode> right;
    };

    struct TypeParameterNode : AstNode
    {
        std::shared_ptr<IdentifierNode> name;
        // Potentially: std::vector<TypeNameNode> constraints;
    };
}
