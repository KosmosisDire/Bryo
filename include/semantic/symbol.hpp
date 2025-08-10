#pragma once

#include <string>
#include <memory>
#include <cstdint>
#include <variant>
#include <vector>
#include "scope.hpp"
#include "type.hpp"

namespace Myre {

// ============= Access Modifiers =============
enum class AccessLevel {
    Public,
    Private,
    Protected
};

// ============= Symbol Modifiers =============
enum class SymbolModifiers : uint32_t {
    None = 0,
    Static = 1 << 0,
    Virtual = 1 << 1,    // For functions
    Override = 1 << 2,   // For functions
    Abstract = 1 << 3,   // For functions/types
    Async = 1 << 4,      // For functions
    Extern = 1 << 5,     // For functions
    Enforced = 1 << 6,   // For functions (Myre-specific)
    Ref = 1 << 7,        // For types (ref type)
    Inline = 1 << 8      // For functions
};

// Enable bitwise operations for SymbolModifiers
inline SymbolModifiers operator|(SymbolModifiers a, SymbolModifiers b) {
    return static_cast<SymbolModifiers>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline SymbolModifiers operator&(SymbolModifiers a, SymbolModifiers b) {
    return static_cast<SymbolModifiers>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline SymbolModifiers& operator|=(SymbolModifiers& a, SymbolModifiers b) {
    a = a | b;
    return a;
}

// ============= Symbols =============
enum class SymbolKind {
    Field,
    Function,
    Variable,
    Parameter,
    Property,
    Type,
    Namespace,
    Using
};

// Forward declarations
struct ExpressionNode;
class SymbolTable;

// ============= Symbol (Named Entities) =============
class Symbol : public ScopeNode {
public:
    SymbolKind kind;          // Namespace, Type, Function, Variable, Parameter, Field
    std::string name;          // Symbol name
    AccessLevel access = AccessLevel::Private;        // Public, Private, Protected
    SymbolModifiers modifiers = SymbolModifiers::None; // Static, Virtual, Override, Abstract, Ref, etc.
    
    // Type information
    TypePtr type;              // Variable/Field/Parameter → their type
                              // Function → return type
                              // Type/Namespace → nullptr
    
    // Function-specific
    std::vector<TypePtr> parameter_types;  // Empty for non-functions
    
    // Type inference
    ExpressionNode* initializer = nullptr;  // For variables needing type inference
    bool needs_inference = false;
    
    // Override base class methods
    Symbol* as_symbol() override { return this; }
    bool is_symbol() const override { return true; }
    
    // Helper methods
    std::string get_qualified_name() const;
    bool is_type() const { return kind == SymbolKind::Type; }
    bool is_function() const { return kind == SymbolKind::Function; }
    bool is_namespace() const { return kind == SymbolKind::Namespace; }
    bool is_variable() const { return kind == SymbolKind::Variable; }
    bool is_parameter() const { return kind == SymbolKind::Parameter; }
    bool is_field() const { return kind == SymbolKind::Field; }
    
    // Check if symbol has specific modifier
    inline bool has_modifier(SymbolModifiers flag) const {
        return (modifiers & flag) != SymbolModifiers::None;
    }
    
    // Helper method to set modifiers
    void add_modifier(SymbolModifiers modifier) { modifiers |= modifier; }
};

} // namespace Myre