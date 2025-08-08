#pragma once

#include <string>
#include <memory>
#include <cstdint>
#include <variant>
#include <vector>
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
struct Scope;
using ScopePtr = std::shared_ptr<Scope>;
struct TypeDefinition;
using TypeDefinitionPtr = std::shared_ptr<TypeDefinition>;
struct Symbol;
using SymbolPtr = std::shared_ptr<Symbol>;
class SymbolTable;

// Symbol-specific data structures
struct VariableInfo {
    TypePtr type;
};

struct FunctionInfo {
    TypePtr return_type;
    std::vector<TypePtr> parameter_types;
    ScopePtr body_scope;  // Function owns its body scope
};

struct TypeInfo {
    TypeDefinitionPtr definition;
    ScopePtr body_scope;  // Type owns its member scope (points to scope in tree)
};

struct NamespaceInfo {
    ScopePtr scope;  // Namespace owns its scope
};

struct Symbol {
    SymbolKind kind;
    std::string name;
    AccessLevel access = AccessLevel::Private;
    SymbolModifiers modifiers = SymbolModifiers::None;
    
    // Symbol-specific data based on kind
    std::variant<
        std::monostate,     // For namespace symbols
        VariableInfo,       // For Variable, Parameter, Field
        FunctionInfo,       // For Function
        TypeInfo,           // For Type
        NamespaceInfo       // For Namespace
    > data;
    
    // Check if symbol has specific modifier
    inline bool has_modifier(SymbolModifiers flag) const {
        return (modifiers & flag) != SymbolModifiers::None;
    }
    
    // Helper method to set modifiers
    void add_modifier(SymbolModifiers modifier) { modifiers |= modifier; }

    private:
    friend SymbolTable;
    static SymbolPtr make_field(const std::string& field_name, TypePtr field_type, AccessLevel access_level = AccessLevel::Private, SymbolModifiers field_modifiers = SymbolModifiers::None) {
        SymbolPtr field_symbol = std::make_shared<Symbol>();
        field_symbol->kind = SymbolKind::Field;
        field_symbol->name = field_name;
        field_symbol->access = access_level;
        field_symbol->modifiers = field_modifiers;
        field_symbol->data = VariableInfo{field_type};
        return field_symbol;
    }

    static SymbolPtr make_function(const std::string& func_name, TypePtr return_type, const std::vector<TypePtr>& param_types, AccessLevel access_level = AccessLevel::Private, SymbolModifiers func_modifiers = SymbolModifiers::None) {
        SymbolPtr func_symbol = std::make_shared<Symbol>();
        func_symbol->kind = SymbolKind::Function;
        func_symbol->name = func_name;
        func_symbol->access = access_level;
        func_symbol->modifiers = func_modifiers;
        func_symbol->data = FunctionInfo{return_type, param_types, nullptr};
        return func_symbol;
    }

    static SymbolPtr make_variable(const std::string& var_name, TypePtr var_type, AccessLevel access_level = AccessLevel::Private, SymbolModifiers var_modifiers = SymbolModifiers::None) {
        SymbolPtr var_symbol = std::make_shared<Symbol>();
        var_symbol->kind = SymbolKind::Variable;
        var_symbol->name = var_name;
        var_symbol->access = access_level;
        var_symbol->modifiers = var_modifiers;
        var_symbol->data = VariableInfo{var_type};
        return var_symbol;
    }

    static SymbolPtr make_parameter(const std::string& param_name, TypePtr param_type, AccessLevel access_level = AccessLevel::Private, SymbolModifiers param_modifiers = SymbolModifiers::None) {
        SymbolPtr param_symbol = std::make_shared<Symbol>();
        param_symbol->kind = SymbolKind::Parameter;
        param_symbol->name = param_name;
        param_symbol->access = access_level;
        param_symbol->modifiers = param_modifiers;
        param_symbol->data = VariableInfo{param_type};
        return param_symbol;
    }


    static SymbolPtr make_type(const std::string& type_name, AccessLevel access_level = AccessLevel::Private, SymbolModifiers type_modifiers = SymbolModifiers::None) {
        SymbolPtr type_symbol = std::make_shared<Symbol>();
        type_symbol->kind = SymbolKind::Type;
        type_symbol->name = type_name;
        type_symbol->access = access_level;
        type_symbol->modifiers = type_modifiers;
        return type_symbol;
    }

    static SymbolPtr make_using(const std::string& namespace_name) {
        SymbolPtr using_symbol = std::make_shared<Symbol>();
        using_symbol->kind = SymbolKind::Using;
        using_symbol->name = namespace_name;
        return using_symbol;
    }

    static SymbolPtr make_namespace(const std::string& namespace_name) {
        SymbolPtr namespace_symbol = std::make_shared<Symbol>();
        namespace_symbol->kind = SymbolKind::Namespace;
        namespace_symbol->name = namespace_name;
        return namespace_symbol;
    }

};

} // namespace Myre