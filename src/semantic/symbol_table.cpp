#include "semantic/symbol_table.hpp"

namespace Myre {

SymbolTable::SymbolTable() {
    globalScope = std::make_shared<Scope>();
    globalScope->kind = Scope::Global;
    currentScope = globalScope;
}

SymbolPtr SymbolTable::enter_namespace(const std::string& name) {
    auto newScope = std::make_shared<Scope>();
    newScope->kind = Scope::Namespace;
    newScope->name = name;
    newScope->parent = currentScope;

    auto sym = Symbol::make_namespace(name);
    NamespaceInfo nsInfo;
    nsInfo.scope = newScope;
    sym->data = nsInfo;
    newScope->scopeDefinition = sym;
    namespaceStack.push(sym);
    currentNamespace = sym;
    define(sym);

    currentScope = newScope;
    return sym;
}

SymbolPtr SymbolTable::enter_type(const std::string& name) {
    auto newScope = std::make_shared<Scope>();
    newScope->kind = Scope::Type;
    newScope->name = name;
    newScope->parent = currentScope;

    auto sym = Symbol::make_type(name);
    newScope->scopeDefinition = sym;
    typeStack.push(sym);
    currentType = sym;
    define(sym); 
    
    currentScope = newScope;
    return sym;
}

SymbolPtr SymbolTable::enter_function(const std::string& name, TypePtr return_type, const std::vector<TypePtr>& param_types) {
    auto newScope = std::make_shared<Scope>();
    newScope->kind = Scope::Function;
    newScope->parent = currentScope;
    newScope->name = name;

    auto sym = Symbol::make_function(name, return_type, param_types);
    FunctionInfo funcInfo;
    funcInfo.return_type = return_type;
    funcInfo.parameter_types = param_types;
    funcInfo.body_scope = newScope;
    sym->data = funcInfo;
    newScope->scopeDefinition = sym;
    functionStack.push(sym);
    currentFunction = sym;
    define(sym);

    if (currentType) {
        // If inside a type, add function to type definition
        auto type_info = std::get<TypeInfo>(currentType->data);
        type_info.definition->add_member(sym);
    }
    
    currentScope = newScope;
    return sym;
}

ScopePtr SymbolTable::enter_block(const std::string& name) {
    auto newScope = std::make_shared<Scope>();
    newScope->kind = Scope::Block;
    newScope->parent = currentScope;
    currentScope->unnamedChildren.push_back(newScope);
    currentScope = newScope;
    if (!name.empty()) {
        newScope->name = name;
    }
    
    return currentScope;
}

void SymbolTable::exit_scope() {
    // pop off the current scope depending on the type
    if (currentScope->kind == Scope::Function) {
        functionStack.pop();
        if (functionStack.empty()) {
            currentFunction = nullptr;
        } else {
            currentFunction = functionStack.top();
        }
    } else if (currentScope->kind == Scope::Type) {
        typeStack.pop();
        if (typeStack.empty()) {
            currentType = nullptr;
        } else {
            currentType = typeStack.top();
        }
    } else if (currentScope->kind == Scope::Namespace) {
        namespaceStack.pop();
        if (namespaceStack.empty()) {
            currentNamespace = nullptr;
        } else {
            currentNamespace = namespaceStack.top();
        }
    }

    if (auto parent = currentScope->parent.lock()) {
        currentScope = parent;
    }
}

SymbolPtr SymbolTable::make_var(const std::string& name, TypePtr type, AccessLevel access) {
    SymbolPtr sym;
    
    // Decide whether to create a field or variable based on current context
    if (currentType && !currentFunction) {
        sym = Symbol::make_field(name, type, access);
    } else {
        sym = Symbol::make_variable(name, type, access);
    }
    
    if (!define(sym)) {
        return nullptr; // Already defined
    }
    
    // If in a type, also add to type definition
    if (currentType && !currentFunction) {
        auto type_info = std::get<TypeInfo>(currentType->data);
        type_info.definition->add_member(sym);
    }
    
    // Check if this symbol has an unresolved type and needs inference
    if (std::holds_alternative<UnresolvedType>(type->value)) {
        unresolvedSymbols.push_back(sym);
    }
    
    return sym;
}

SymbolPtr SymbolTable::make_parameter(const std::string& name, TypePtr type, AccessLevel access) {
    auto sym = Symbol::make_parameter(name, type, access);
    if (!define(sym)) {
        return nullptr; // Already defined
    }

    if (currentFunction) {
        auto func_info = std::get<FunctionInfo>(currentFunction->data);
        func_info.body_scope->define(sym); // Parameters go in function scope
    }

    // Check if this symbol has an unresolved type and needs inference
    if (std::holds_alternative<UnresolvedType>(type->value)) {
        unresolvedSymbols.push_back(sym);
    }

    return sym;
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

TypeSystem& SymbolTable::get_type_system() { 
    return typeSystem; 
}

TypePtr SymbolTable::resolve_type_name(const std::string& type_name, ScopePtr scope, bool null_is_unresolved) 
{
    // first check if it is a built-in type
    auto prim = typeSystem.get_primitive(type_name);
    if (prim) {
        return prim;
    }
    
    auto symbol = scope->lookup(type_name);

    if (!symbol)
    {
        return null_is_unresolved ? nullptr : typeSystem.get_unresolved_type();
    }

    if (auto* type_info = std::get_if<TypeInfo>(&symbol->data))
    {
        return typeSystem.get_type_reference(type_info->definition->full_name);
    }

    return null_is_unresolved ? nullptr : typeSystem.get_unresolved_type();
}

std::string SymbolTable::build_qualified_name(const std::string& name) const {
    std::string scopeFullName = currentScope->get_full_name();
    if (scopeFullName.empty()) {
        return name;
    }
    return scopeFullName + "." + name;
}

std::string SymbolTable::to_string() const {
    std::string result = "=== SYMBOL TABLE ===\n";
    if (currentNamespace)
        result += "Current Namespace: " + currentNamespace->name  + "\n";
        
    if (currentScope)
        result += "Current Scope: " + currentScope->get_full_name() + "\n\n";
    
    result += "Scope Hierarchy:\n";
    result += globalScope->to_string(0);

    // Display all unresolved symbols
    if (!unresolvedSymbols.empty()) {
        result += "\nUnresolved Symbols:\n";
        for (const auto& sym : unresolvedSymbols) {
            result += " - " + sym->name + "\n";
        }
    }
    
    return result;
}

} // namespace Myre