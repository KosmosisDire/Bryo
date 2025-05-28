#pragma once

#include <memory>
#include <string>
#include <optional>
#include <vector> // For ModifierKind in DeclarationNode (transitively) & other uses
#include "ast_location.hpp"
#include "ast_enums.hpp" // For ModifierKind, LiteralKind etc.
#include "../common/script_token_types.hpp" // For TokenType, Changed path

namespace Mycelium::Scripting::Lang
{
    // Forward declarations for major AST node categories
    struct ExpressionNode;
    struct StatementNode;
    struct DeclarationNode;
    // Add other major categories if they are directly referenced by base nodes
    // or if it helps break cycles/improve clarity.
    // For now, these seem sufficient for ast_base.

    // Base AST Node
    struct AstNode
    {
        static inline long long idCounter = 0;
        std::weak_ptr<AstNode> parent;
        std::optional<SourceLocation> location; // Overall location of the node
        long long id;

        AstNode(); // Implementation will be in a .cpp file
        virtual ~AstNode() = default;
    };

    // Represents a lexical token
    struct TokenNode : AstNode
    {
        std::string text;
        TokenType tokenType;
        // Location is inherited from AstNode
    };

    // Represents an identifier
    struct IdentifierNode : AstNode 
    {
        std::string name;
        // 'location' from AstNode will store its precise location.

        IdentifierNode(std::string n = "") : name(std::move(n)) {}
    };
}
