#pragma once

#include <string>
#include <memory>
#include <vector>
#include "scope.hpp"
#include "symbol.hpp"
#include "type_system.hpp"

namespace Myre {

class SymbolTable {
    std::unique_ptr<NamespaceSymbol> global_symbol;  // Root namespace
    ScopeNode* current;                             // Current scope
    TypeSystem type_system;
    std::vector<Symbol*> unresolved_symbols;        // Changed from UnscopedSymbol*
    int next_block_id = 0;
    
public:
    SymbolTable();
    
    // Enter named scopes
    NamespaceSymbol* enter_namespace(const std::string& name);
    TypeSymbol* enter_type(const std::string& name);
    EnumSymbol* enter_enum(const std::string& name);
    FunctionSymbol* enter_function(const std::string& name, TypePtr return_type, 
                                  std::vector<TypePtr> params);
    
    // Enter anonymous scope
    BlockScope* enter_block(const std::string& debug_name = "block");
    
    // Exit current scope
    void exit_scope();
    
    // Define symbols in current scope
    VariableSymbol* define_variable(const std::string& name, TypePtr type);
    ParameterSymbol* define_parameter(const std::string& name, TypePtr type);
    FieldSymbol* define_field(const std::string& name, TypePtr type);
    PropertySymbol* define_property(const std::string& name, TypePtr type);
    EnumCaseSymbol* define_enum_case(const std::string& name, std::vector<TypePtr> params = {});
    
    // Context queries
    NamespaceSymbol* get_current_namespace() const;
    TypeSymbol* get_current_type() const;
    EnumSymbol* get_current_enum() const;
    FunctionSymbol* get_current_function() const;
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