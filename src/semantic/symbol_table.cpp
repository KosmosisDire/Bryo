#include "semantic/symbol_table.hpp"

namespace Myre {

SymbolTable::SymbolTable() {
    globalScope = std::make_shared<Scope>();
    globalScope->kind = Scope::Global;
    currentScope = globalScope;
}

void SymbolTable::enter_namespace(const std::string& name) {
    auto newScope = std::make_shared<Scope>();
    newScope->kind = Scope::Namespace;
    newScope->name = name;
    newScope->parent = currentScope;
    currentScope = newScope;
}

void SymbolTable::enter_type(const std::string& name, SymbolPtr typeSymbol) {
    auto newScope = std::make_shared<Scope>();
    newScope->kind = Scope::Type;
    newScope->name = name;
    newScope->parent = currentScope;
    newScope->scopeDefinition = typeSymbol;
    currentScope = newScope;
}

void SymbolTable::enter_function() {
    auto newScope = std::make_shared<Scope>();
    newScope->kind = Scope::Function;
    newScope->parent = currentScope;
    currentScope = newScope;
}

void SymbolTable::enter_block() {
    auto newScope = std::make_shared<Scope>();
    newScope->kind = Scope::Block;
    newScope->parent = currentScope;
    currentScope = newScope;
}

void SymbolTable::exit_scope() {
    if (auto parent = currentScope->parent.lock()) {
        currentScope = parent;
    }
}

bool SymbolTable::define(SymbolPtr sym) {
    return currentScope->define(sym);
}

SymbolPtr SymbolTable::lookup(const std::string& name) {
    return currentScope->lookup(name);
}

ScopePtr SymbolTable::get_current_scope() { 
    return currentScope; 
}

TypeRegistry& SymbolTable::get_type_registry() { 
    return typeRegistry; 
}

std::string SymbolTable::get_current_namespace() const {
    auto scope = currentScope;
    while (scope && scope->kind != Scope::Namespace) {
        scope = scope->parent.lock();
    }
    return scope ? scope->get_full_name() : "";
}

std::string SymbolTable::to_string() const {
    std::string result = "=== SYMBOL TABLE ===\n";
    result += "Current Namespace: " + get_current_namespace() + "\n";
    result += "Current Scope: " + currentScope->get_full_name() + "\n\n";
    
    result += "Scope Hierarchy:\n";
    result += globalScope->to_string_recursive(0);
    
    return result;
}

} // namespace Myre