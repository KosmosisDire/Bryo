#pragma once

#include <string>
#include "scope.hpp"
#include "type_registry.hpp"

namespace Myre {

// ============= Symbol Table =============
class SymbolTable {
    ScopePtr currentScope;
    ScopePtr globalScope;
    TypeRegistry typeRegistry;
    
public:
    SymbolTable();
    
    void enter_namespace(const std::string& name);
    void enter_type(const std::string& name, SymbolPtr typeSymbol);
    void enter_function();
    void enter_block();
    void exit_scope();
    
    bool define(SymbolPtr sym);
    SymbolPtr lookup(const std::string& name);
    
    ScopePtr get_current_scope();
    TypeRegistry& get_type_registry();
    std::string get_current_namespace() const;
    
    // Debug/display functions
    std::string to_string() const;
};

} // namespace Myre