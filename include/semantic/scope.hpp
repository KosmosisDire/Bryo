#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include "common/symbol_handle.hpp"

namespace Myre {

// Forward declarations
class Symbol;
class BlockScope;
class NamespaceSymbol;
class TypeSymbol; 
class EnumSymbol;
class FunctionSymbol;
class PropertySymbol;
class TypeLikeSymbol;
class Scope;


// ============= Base Scope Node =============
// Base for anything that participates in the scope tree
class ScopeNode {
public:
    ScopeNode* parent = nullptr;  // Non-owning pointer to parent, not using a handle because the tree should always stay in tact
    SymbolHandle handle;

    // Type checking
    template<typename T>
    T* as() { return dynamic_cast<T*>(this); }
    
    template<typename T>
    const T* as() const { return dynamic_cast<const T*>(this); }
    
    template<typename T>
    bool is() const { return dynamic_cast<const T*>(this) != nullptr; }
    
    // Context queries - walk up tree to find nearest Symbol of specific type
    NamespaceSymbol* get_enclosing_namespace() const;
    TypeSymbol* get_enclosing_type() const;
    EnumSymbol* get_enclosing_enum() const;
    FunctionSymbol* get_enclosing_function() const;
    PropertySymbol* get_enclosing_property() const;
    TypeLikeSymbol* get_enclosing_type_like() const;  // Gets enum or type
    
    // Build fully qualified name from this scope
    std::string build_qualified_name(const std::string& name) const;
    
    virtual ~ScopeNode() = default;
};

// ============= Scope Mixin =============
// Mixin for nodes that can contain symbols
class Scope {
public:
    std::unordered_map<std::string, ScopeNode*> symbols;
    
    // Lookup methods (search children and up tree)
    Symbol* lookup(const std::string& name);
    inline Symbol* lookup(const std::string_view& name) {
        return lookup(std::string(name));
    }
    Symbol* lookup_local(const std::string& name);
    inline Symbol* lookup_local(const std::string_view& name) {
        return lookup_local(std::string(name));
    }
    void add_symbol(const std::string& name, ScopeNode* symbol);
    
    // Must be implemented by classes that inherit both ScopeNode and Scope
    virtual ScopeNode* as_scope_node() = 0;
    virtual const ScopeNode* as_scope_node() const = 0;

    // Type checking (deferred to ScopeNode)
    template<typename T>
    T* scope_as() {
        return as_scope_node()->as<T>();
    }
    
    template<typename T>
    const T* scope_as() const {
        return as_scope_node()->as<T>();
    }
    
    template<typename T>
    bool scope_is() const {
        return as_scope_node()->is<T>();
    }
    
    virtual ~Scope() = default;
};

// ============= Anonymous Block Scopes =============
class BlockScope : public ScopeNode, public Scope { 
public:
    std::string debug_name;  // "if-block", "for-block", "block", etc. (for debugging)
    
    // Implement Scope interface
    ScopeNode* as_scope_node() override { return this; }
    const ScopeNode* as_scope_node() const override { return this; }
    
    BlockScope(const std::string& debug_name = "block") 
        : debug_name(debug_name) {}
};

} // namespace Myre