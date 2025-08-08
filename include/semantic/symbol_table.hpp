#pragma once

#include <string>
#include <algorithm>
#include "scope.hpp"
#include "type_system.hpp"  // Contains TypeSystem
#include <stack>

namespace Myre {

// ============= Symbol Table =============
class SymbolTable {
    ScopePtr currentScope;
    ScopePtr globalScope;
    TypeSystem typeSystem;

    std::stack<SymbolPtr> namespaceStack;
    std::stack<SymbolPtr> typeStack;
    std::stack<SymbolPtr> functionStack;

    std::vector<SymbolPtr> unresolvedSymbols;

    
public:
    SymbolTable();
    
    SymbolPtr enter_namespace(const std::string& name);
    SymbolPtr enter_type(const std::string& name);
    SymbolPtr enter_function(const std::string& name, TypePtr return_type, const std::vector<TypePtr>& param_types);
    ScopePtr enter_block(const std::string& name = "");
    void exit_scope();
    
    // Symbol creation helpers
    SymbolPtr make_var(const std::string& name, TypePtr type, AccessLevel access = AccessLevel::Private);
    SymbolPtr make_parameter(const std::string& name, TypePtr type, AccessLevel access = AccessLevel::Private);
    
    bool define(SymbolPtr sym);
    SymbolPtr lookup(const std::string& name);
    TypePtr resolve_type_name(const std::string& type_name, ScopePtr scope, bool null_is_unresolved = false);
    
    ScopePtr get_current_scope();
    TypeSystem& get_type_system();
    const std::vector<SymbolPtr> get_unresolved_symbols() const { return unresolvedSymbols; }
    void set_symbol_resolved(SymbolPtr symbol) {
        auto it = std::find(unresolvedSymbols.begin(), unresolvedSymbols.end(), symbol);
        if (it != unresolvedSymbols.end()) {
            unresolvedSymbols.erase(it);
        }
    }

    SymbolPtr currentNamespace;
    SymbolPtr currentType;
    SymbolPtr currentFunction;
    
    // Helper to build fully qualified name in current scope
    std::string build_qualified_name(const std::string& name) const;
    
    // Debug/display functions
    std::string to_string() const;
};

} // namespace Myre