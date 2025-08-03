#pragma once

#include <string>
#include <memory>
#include <cstdint>
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
    Namespace
};

struct Symbol {
    SymbolKind kind;
    std::string name;
    TypePtr type; // For functions, this is FunctionType. For types, this is the type itself
    AccessLevel access = AccessLevel::Private;
    SymbolModifiers modifiers = SymbolModifiers::None;
    
    // Check if symbol has specific modifier
    inline bool has_modifier(SymbolModifiers flag) const {
        return (modifiers & flag) != SymbolModifiers::None;
    }
    
    // Helper method to set modifiers
    void add_modifier(SymbolModifiers modifier) { modifiers |= modifier; }
};

using SymbolPtr = std::shared_ptr<Symbol>;

} // namespace Myre