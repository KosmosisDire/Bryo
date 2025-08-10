#pragma once

#include <string>
#include <memory>
#include <vector>
#include "scope.hpp"
#include "symbol.hpp"
#include "type_system.hpp"

namespace Myre {

// ============= Symbol Table =============
class SymbolTable {
    std::unique_ptr<Symbol> global_symbol;  // Root (always a Symbol with kind=Namespace)
    ScopeNode* current;                      // Current position (Symbol or BlockScope)
    TypeSystem type_system;
    std::vector<Symbol*> unresolved_symbols;
    int next_block_id = 0;                  // For generating unique block keys
    
public:
    SymbolTable();
    
    // Enter named scopes (creates Symbols)
    Symbol* enter_namespace(const std::string& name);
    Symbol* enter_type(const std::string& name);
    Symbol* enter_function(const std::string& name, TypePtr return_type, 
                          std::vector<TypePtr> params);
    
    // Enter anonymous scope (creates BlockScope)
    BlockScope* enter_block(const std::string& debug_name = "block");
    
    // Exit current scope (works for both Symbol and BlockScope)
    void exit_scope();
    
    // Define symbols in current scope
    Symbol* define_variable(const std::string& name, TypePtr type);
    Symbol* define_parameter(const std::string& name, TypePtr type);
    Symbol* define_field(const std::string& name, TypePtr type);
    
    // Context queries (walk up tree to find nearest Symbol of type)
    Symbol* get_current_namespace() const;
    Symbol* get_current_type() const;
    Symbol* get_current_function() const;
    ScopeNode* get_current_scope() const { return current; }
    
    // Lookup
    Symbol* lookup(const std::string& name);
    TypePtr resolve_type_name(const std::string& name);
    TypePtr resolve_type_name(const std::string& name, ScopeNode* scope);
    
    // Type system access
    TypeSystem& get_type_system() { return type_system; }
    
    // Unresolved symbols tracking
    const std::vector<Symbol*>& get_unresolved_symbols() const { return unresolved_symbols; }
    void mark_symbol_resolved(Symbol* symbol);
    
    // Helper to build fully qualified name in current scope
    std::string build_qualified_name(const std::string& name) const;
    
    // Debug/display functions
    std::string to_string() const;
    
private:
    // Helper to add a child to current scope
    void add_child(const std::string& key, std::unique_ptr<ScopeNode> child);
};

} // namespace Myre