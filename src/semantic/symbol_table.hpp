#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include "symbol.hpp"
#include "type_system.hpp"

namespace Bryo
{

class SymbolTable {
public:
    explicit SymbolTable(TypeSystem& type_system);
    
    // Scope management
    void push_scope(Symbol* scope);
    void pop_scope();
    Symbol* get_current_scope();
    ContainerSymbol* get_current_container();
    
    // Symbol definition
    NamespaceSymbol* define_namespace(const std::string& name);
    TypeSymbol* define_type(const std::string& name, TypePtr type);
    FunctionSymbol* define_function(const std::string& name, TypePtr return_type);
    FieldSymbol* define_field(const std::string& name, TypePtr type);
    PropertySymbol* define_property(const std::string& name, TypePtr type);
    ParameterSymbol* define_parameter(const std::string& name, TypePtr type, uint32_t index);
    LocalSymbol* define_local(const std::string& name, TypePtr type);
    
    // Symbol resolution
    Symbol* resolve(const std::string& name);
    Symbol* resolve(const std::vector<std::string>& parts);
    Symbol* resolve_local(const std::string& name);
    Symbol* resolve_local(const std::vector<std::string>& parts);
    FunctionSymbol* resolve_function(const std::string& name, const std::vector<TypePtr>& arg_types);
    
    // Access to global namespace
    NamespaceSymbol* get_global_namespace();
    
    // Access to type system (needed by symbol_table_builder)
    TypeSystem& get_type_system() { return types; }
    const TypeSystem& get_type_system() const { return types; }
    
    // Merge another symbol table into this one
    std::vector<std::string> merge(SymbolTable& other);
    
    // Debugging
    std::string to_string() const;
    
private:
    TypeSystem& types;
    std::unique_ptr<NamespaceSymbol> global_namespace;
    Symbol* current_scope = nullptr;
    
    // Recursively merge source namespace into target namespace
    void merge_namespace(NamespaceSymbol* target, NamespaceSymbol* source, 
                        std::vector<std::string>& conflicts);
    
    // Update parent pointers recursively after moving a symbol
    void update_parent_pointers(Symbol* symbol, Symbol* new_parent);
};

} // namespace Bryo