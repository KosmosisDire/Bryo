#pragma once

#include <string>
#include <memory>
#include <vector>

namespace Myre {

// Forward declarations
struct Scope;
struct Symbol;
enum class SymbolModifiers : uint32_t;
using ScopePtr = std::shared_ptr<Scope>;
using SymbolPtr = std::shared_ptr<Symbol>;

// Forward declaration
struct Type;
using TypePtr = std::shared_ptr<Type>;

// ============= Type Definitions =============
// Represents a declared type (type Player { ... })
struct TypeDefinition {
    std::string name;
    std::string full_name;  // namespace.name
    
    // The scope containing all members
    ScopePtr member_scope;
    
    // Type characteristics
    SymbolModifiers modifiers;  // abstract, ref, static, etc.
    TypePtr base_type;          // Parent type (if any)
    
    // Generic parameters (if any)
    std::vector<std::string> type_parameters;  // ["T", "U"]
    
    // Constructor
    TypeDefinition(const std::string& name, const std::string& full_name);
    
    // Helper methods
    bool is_ref_type() const;
    bool is_abstract() const;
    bool is_generic() const;
    
    // Look up a member in this type
    SymbolPtr lookup_member(const std::string& name);
    
    // Add a member to this type
    bool add_member(SymbolPtr member);
};

using TypeDefinitionPtr = std::shared_ptr<TypeDefinition>;

} // namespace Myre