#pragma once

#include <vector>
#include <string>
#include <memory> // For std::unique_ptr
#include <iostream> // For printing
#include <optional> // For optional block name

namespace Mycelium::UI::Lang {

// Forward declare nodes that might reference each other (optional here, good practice)
struct BlockNode;

// Base class for all AST nodes
struct AstNode {
    virtual ~AstNode() = default;
    virtual void print(int indent = 0) const = 0;
};

// Node representing a block like "Box { ... }" or "Box("Name") { ... }"
struct BlockNode : AstNode {
    std::string typeIdentifier;                   // e.g., "Box"
    std::optional<std::string> nameArgument;      // e.g., "Main" if Box("Main")
    std::vector<std::unique_ptr<AstNode>> statements; // Nested statements (Blocks, Properties, etc.)

    BlockNode(std::string typeId) : typeIdentifier(std::move(typeId)) {}

    void print(int indent = 0) const override; // Implementation in ast.cpp
};


// Root node for the entire program
struct ProgramNode : AstNode {
    std::vector<std::unique_ptr<AstNode>> definitions; // Top-level definitions (currently only Blocks)

    void print(int indent = 0) const override; // Implementation in ast.cpp
};

struct ValueNode : AstNode { /* ... */ };

struct NumberLiteralNode : ValueNode {
    double value; // Or int, depending on needs
    bool isPercentage; // To handle '100%' vs '100'
    // std::string unit; // Optional: for "px", "em" later
    NumberLiteralNode(double val, bool perc = false) : value(val), isPercentage(perc) {}
    void print(int indent = 0) const override;
};

struct StringLiteralValueNode : ValueNode {
    std::string value;
    StringLiteralValueNode(std::string val) : value(std::move(val)) {}
    void print(int indent = 0) const override;
};

struct PropertyNode : AstNode { // Or make it inherit from a new "StatementNode" base
    std::string name;
    std::unique_ptr<ValueNode> value;

    PropertyNode(std::string n, std::unique_ptr<ValueNode> v)
        : name(std::move(n)), value(std::move(v)) {}

    void print(int indent = 0) const override;
};

} // namespace Mycelium::UI::Lang
