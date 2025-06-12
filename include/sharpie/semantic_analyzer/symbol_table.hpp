// include/sharpie/semantic_analyzer/symbol_table.hpp

#pragma once

#include "../script_ast.hpp"
#include "../compiler/class_type_info.hpp"
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
    // Enhanced variable symbol with semantic information
    struct VariableSymbol {
        std::string name;
        std::shared_ptr<TypeNameNode> type;
        SourceLocation declaration_location;
        bool is_used = false;
        const ClassTypeInfo* class_info = nullptr; // For object types
        
        // Enhanced semantic information
        bool is_parameter = false;
        bool is_field = false;
        std::string owning_scope; // Method, class, or namespace name
        bool is_definitely_assigned = false; // For definite assignment analysis
    };
    
    // Enhanced method symbol with forward declaration tracking
    struct MethodSymbol {
        std::string name;
        std::string qualified_name; // e.g., "ClassName.methodName"
        std::shared_ptr<TypeNameNode> return_type;
        std::vector<std::shared_ptr<ParameterDeclarationNode>> parameters;
        SourceLocation declaration_location;
        bool is_static = false;
        bool is_used = false;
        
        // Enhanced semantic information
        bool is_forward_declared = false; // Declared but not yet defined
        bool is_defined = false; // Has implementation/body
        std::string containing_class; // Empty for free functions
        std::vector<std::string> parameter_names; // For easier lookup
        std::vector<std::shared_ptr<TypeNameNode>> parameter_types; // For easier lookup
        bool is_constructor = false;
        bool is_destructor = false;
        bool is_external = false; // extern functions
        bool is_virtual = false; // virtual methods for polymorphism
    };
    
    // Enhanced class symbol with inheritance and comprehensive member tracking
    struct ClassSymbol {
        std::string name;
        SourceLocation declaration_location;
        std::vector<std::string> field_names;
        std::vector<std::shared_ptr<TypeNameNode>> field_types;
        std::vector<MethodSymbol> methods;
        ClassTypeInfo type_info; // For IR generation
        
        // Enhanced semantic information
        std::string base_class; // For single inheritance
        std::vector<std::string> interfaces; // For interface implementation
        std::map<std::string, MethodSymbol> method_registry; // Methods by name for fast lookup
        std::map<std::string, VariableSymbol> field_registry; // Fields by name for fast lookup
        bool is_forward_declared = false; // Declared but not yet defined
        bool is_defined = false; // Has full definition
        std::vector<std::string> constructors; // Constructor qualified names
        std::string destructor; // Destructor qualified name (if any)
        std::vector<std::string> virtual_method_order; // Order of virtual methods for VTable layout
    };
    
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
    
    // Class symbols
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
    VariableSymbol* find_field_in_class(const std::string& class_name, const std::string& field_name);
    std::vector<MethodSymbol*> get_constructors_for_class(const std::string& class_name);
    MethodSymbol* get_destructor_for_class(const std::string& class_name);
    
    // Scope analysis
    std::string get_current_scope_name();
    std::vector<std::string> get_available_variables_in_scope();
    
    // Access to underlying data for ScriptCompiler compatibility
    const std::map<std::string, ClassSymbol>& get_classes() const { return classes; }
    std::map<std::string, ClassSymbol>& get_classes() { return classes; }
    const std::map<std::string, MethodSymbol>& get_methods() const { return methods; }
};

} // namespace Mycelium::Scripting::Lang