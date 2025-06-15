#pragma once

#include "symbol_types.hpp"
#include <string>
#include <memory>
#include <map>
#include <vector>
#include <optional>
#include <set>

namespace Mycelium::Scripting::Lang
{

/**
 * Enhanced symbol table with comprehensive semantic information
 * Maintains backward compatibility while adding detailed semantic data
 */
class SymbolTable {
public:
    // Import symbol types from separate header
    using VariableSymbol = ::Mycelium::Scripting::Lang::VariableSymbol;
    using MethodSymbol = ::Mycelium::Scripting::Lang::MethodSymbol;
    using ClassSymbol = ::Mycelium::Scripting::Lang::ClassSymbol;
    
private:
    std::vector<std::map<std::string, VariableSymbol>> variable_scopes;
    std::map<std::string, ClassSymbol> classes;
    std::map<std::string, MethodSymbol> methods; // Global method registry
    
public:
    // Scope management
    void push_scope();
    void pop_scope();
    
    // Variable symbols
    void declare_variable(const VariableSymbol& symbol);
    VariableSymbol* find_variable(const std::string& name);
    bool is_variable_declared_in_current_scope(const std::string& name);
    void mark_variable_used(const std::string& name);
    
    // Class symbols - UNIFIED ACCESS
    void declare_class(const ClassSymbol& symbol);
    ClassSymbol* find_class(const std::string& name);
    const ClassSymbol* find_class(const std::string& name) const;
    
    // Method symbols
    void declare_method(const MethodSymbol& symbol);
    MethodSymbol* find_method(const std::string& qualified_name);
    const MethodSymbol* find_method(const std::string& qualified_name) const;
    
    // Enhanced semantic analysis methods
    void mark_method_as_forward_declared(const std::string& qualified_name);
    void mark_method_as_defined(const std::string& qualified_name);
    void mark_class_as_forward_declared(const std::string& class_name);
    void mark_class_as_defined(const std::string& class_name);
    
    // Forward declaration resolution
    std::vector<MethodSymbol*> get_forward_declared_methods();
    std::vector<ClassSymbol*> get_forward_declared_classes();
    bool has_unresolved_forward_declarations();
    
    // Enhanced lookup methods
    MethodSymbol* find_method_in_class(const std::string& class_name, const std::string& method_name);
    const MethodSymbol* find_method_in_class(const std::string& class_name, const std::string& method_name) const;
    const VariableSymbol* find_field_in_class(const std::string& class_name, const std::string& field_name) const;
    std::vector<MethodSymbol*> get_constructors_for_class(const std::string& class_name);
    MethodSymbol* get_destructor_for_class(const std::string& class_name);
    
    // Scope analysis
    std::string get_current_scope_name();
    std::vector<std::string> get_available_variables_in_scope();
    
    // Access to underlying data - SIMPLIFIED (no more separate registries)
    const std::map<std::string, ClassSymbol>& get_classes() const { return classes; }
    std::map<std::string, ClassSymbol>& get_classes() { return classes; }
    const std::map<std::string, MethodSymbol>& get_methods() const { return methods; }
};

} // namespace Mycelium::Scripting::Lang