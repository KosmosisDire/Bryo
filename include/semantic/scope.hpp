#pragma once

#include <string>
#include <memory>
#include <unordered_map>

namespace Myre {

// Forward declarations
class Symbol;
class BlockScope;

// ============= Base Scope Node =============
// Unified tree structure for both named symbols and anonymous scopes
class ScopeNode {
public:
    ScopeNode* parent = nullptr;  // Non-owning pointer to parent
    std::unordered_map<std::string, std::unique_ptr<ScopeNode>> children;
    
    // Lookup walks up the tree
    Symbol* lookup(const std::string& name);
    Symbol* lookup_local(const std::string& name);
    
    // Type identification
    virtual Symbol* as_symbol() { return nullptr; }
    virtual BlockScope* as_block() { return nullptr; }
    virtual bool is_symbol() const { return false; }
    virtual bool is_block() const { return false; }
    
    // Context queries - walk up tree to find nearest Symbol of type
    Symbol* get_enclosing_namespace() const;
    Symbol* get_enclosing_type() const;
    Symbol* get_enclosing_function() const;
    
    // Build fully qualified name from this scope
    std::string build_qualified_name(const std::string& name) const;
    
    virtual ~ScopeNode() = default;
};

// ============= Anonymous Block Scopes =============
class BlockScope : public ScopeNode {
public:
    std::string debug_name;  // "if-block", "for-block", "block", etc. (for debugging)
    
    // Override base class methods
    BlockScope* as_block() override { return this; }
    bool is_block() const override { return true; }
    
    BlockScope(const std::string& debug_name = "block") 
        : debug_name(debug_name) {}
};

} // namespace Myre