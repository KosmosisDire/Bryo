#pragma once

#include <string>
#include <memory>
#include <unordered_map>

namespace Myre {

// Forward declarations
class Symbol;
class BlockScope;
class NamespaceSymbol;
class TypeSymbol; 
class EnumSymbol;
class FunctionSymbol;
class TypeLikeSymbol;
class Scope;

// ============= Base Scope Node =============
// Base for anything that participates in the scope tree
class ScopeNode {
public:
    ScopeNode* parent = nullptr;  // Non-owning pointer to parent
    
    // Type identification - virtual so subclasses can override
    virtual Symbol* as_symbol() { return nullptr; }
    virtual bool is_symbol() const { return false; }
    virtual BlockScope* as_block() { return nullptr; }
    virtual bool is_block() const { return false; }
    
    // Safe casting helpers to avoid naked dynamic_cast usage
    Scope* as_scope();
    const Scope* as_scope() const;
    bool is_scope() const;
    
    // Context queries - walk up tree to find nearest Symbol of specific type
    NamespaceSymbol* get_enclosing_namespace() const;
    TypeSymbol* get_enclosing_type() const;
    EnumSymbol* get_enclosing_enum() const;
    FunctionSymbol* get_enclosing_function() const;
    TypeLikeSymbol* get_enclosing_type_like() const;  // Gets enum or type
    
    // Build fully qualified name from this scope
    std::string build_qualified_name(const std::string& name) const;
    
    virtual ~ScopeNode() = default;
};

// ============= Scope Mixin =============
// Mixin for nodes that can contain symbols
class Scope {
public:
    std::unordered_map<std::string, std::unique_ptr<ScopeNode>> symbols;
    
    // Lookup methods (search children and up tree)
    Symbol* lookup(const std::string& name);
    Symbol* lookup_local(const std::string& name);
    void add_symbol(const std::string& name, std::unique_ptr<ScopeNode> symbol);
    
    // Must be implemented by classes that inherit both ScopeNode and Scope
    virtual ScopeNode* as_scope_node() = 0;
    virtual const ScopeNode* as_scope_node() const = 0;
    
    virtual ~Scope() = default;
};

// ============= Anonymous Block Scopes =============
class BlockScope : public ScopeNode, public Scope {
public:
    std::string debug_name;  // "if-block", "for-block", "block", etc. (for debugging)
    
    // Override base class methods
    BlockScope* as_block() override { return this; }
    bool is_block() const override { return true; }
    
    // Implement Scope interface
    ScopeNode* as_scope_node() override { return this; }
    const ScopeNode* as_scope_node() const override { return this; }
    
    BlockScope(const std::string& debug_name = "block") 
        : debug_name(debug_name) {}
};

} // namespace Myre