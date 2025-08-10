#pragma once

#include <string>
#include <memory>
#include <vector>
#include "scope.hpp"
#include "type.hpp"

namespace Myre {

// Forward declarations
struct ExpressionNode;
struct BlockStatementNode;

// Access levels
enum class AccessLevel {
    Public,
    Private,
    Protected
};

// Symbol modifiers
enum class SymbolModifiers : uint32_t {
    None = 0,
    Static = 1 << 0,
    Virtual = 1 << 1,
    Override = 1 << 2,
    Abstract = 1 << 3,
    Async = 1 << 4,
    Extern = 1 << 5,
    Enforced = 1 << 6,
    Ref = 1 << 7,
    Inline = 1 << 8
};

inline SymbolModifiers operator|(SymbolModifiers a, SymbolModifiers b) {
    return static_cast<SymbolModifiers>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline SymbolModifiers operator&(SymbolModifiers a, SymbolModifiers b) {
    return static_cast<SymbolModifiers>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

// Base symbol class - all symbols are scope nodes
class Symbol : public ScopeNode {
protected:
    std::string name_;
    AccessLevel access_ = AccessLevel::Private;
    SymbolModifiers modifiers_ = SymbolModifiers::None;
    
public:
    virtual ~Symbol() = default;
    
    // From ScopeNode
    bool is_symbol() const override { return true; }
    Symbol* as_symbol() override { return this; }
    
    // Basic properties
    const std::string& name() const { return name_; }
    AccessLevel access() const { return access_; }
    SymbolModifiers modifiers() const { return modifiers_; }
    
    void set_name(const std::string& name) { name_ = name; }
    void set_access(AccessLevel access) { access_ = access; }
    void add_modifier(SymbolModifiers mod) { modifiers_ = modifiers_ | mod; }
    
    bool has_modifier(SymbolModifiers mod) const {
        return (modifiers_ & mod) != SymbolModifiers::None;
    }
    
    // Type checking
    template<typename T>
    T* as() { return dynamic_cast<T*>(this); }
    
    template<typename T>
    const T* as() const { return dynamic_cast<const T*>(this); }
    
    template<typename T>
    bool is() const { return dynamic_cast<const T*>(this) != nullptr; }
    
    // For debugging/display
    virtual const char* kind_name() const = 0;
    
    // Build qualified name
    std::string get_qualified_name() const;
};

// Base for types and enums
class TypeLikeSymbol : public Symbol {
public:
    // Can be used as a type in declarations
};

// Base for symbols that have a type (variables, parameters, fields, properties, functions)
class TypedSymbol : public Symbol {
protected:
    TypePtr type_;
    
public:
    // Abstract class - cannot be instantiated directly
    virtual ~TypedSymbol() = default;
    
    // Type access
    virtual TypePtr type() const { return type_; }
    virtual void set_type(TypePtr type) { type_ = type; }
};

// Regular type (class/struct)
class TypeSymbol : public TypeLikeSymbol {
public:
    const char* kind_name() const override { return "type"; }
    
    bool is_ref_type() const { return has_modifier(SymbolModifiers::Ref); }
    bool is_abstract() const { return has_modifier(SymbolModifiers::Abstract); }
};

// Enum type
class EnumSymbol : public TypeLikeSymbol {
public:
    const char* kind_name() const override { return "enum"; }
};

// Namespace
class NamespaceSymbol : public Symbol {
public:
    const char* kind_name() const override { return "namespace"; }
};

// Function
class FunctionSymbol : public TypedSymbol {
    std::vector<TypePtr> parameter_types_;
    
public:
    const char* kind_name() const override { return "function"; }
    
    // Override to use the inherited type_ as return type
    void set_return_type(TypePtr type) { type_ = type; }
    void set_parameter_types(std::vector<TypePtr> types) { parameter_types_ = std::move(types); }
    
    TypePtr return_type() const { return type_; }
    const std::vector<TypePtr>& parameter_types() const { return parameter_types_; }
};

// Variable
class VariableSymbol : public TypedSymbol {
public:
    const char* kind_name() const override { return "variable"; }
};

// Parameter
class ParameterSymbol : public TypedSymbol {
public:
    const char* kind_name() const override { return "parameter"; }
};

// Field
class FieldSymbol : public TypedSymbol {
public:
    const char* kind_name() const override { return "field"; }
};

// Property
class PropertySymbol : public TypedSymbol {
public:
    const char* kind_name() const override { return "property"; }
};

// Enum case
class EnumCaseSymbol : public Symbol {
    std::vector<TypePtr> associated_types_;
    
public:
    const char* kind_name() const override { return "enum_case"; }
    
    void set_associated_types(std::vector<TypePtr> types) { associated_types_ = std::move(types); }
    const std::vector<TypePtr>& associated_types() const { return associated_types_; }
    
    bool is_simple() const { return associated_types_.empty(); }
    bool is_tagged() const { return !associated_types_.empty(); }
};

} // namespace Myre