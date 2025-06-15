#pragma once

#include "../script_ast.hpp"
#include <string>
#include <memory>
#include <map>
#include <vector>
#include <optional>
#include <set>

// Forward declarations for LLVM types
namespace llvm {
    class StructType;
    class Function;
    class GlobalVariable;
}

namespace Mycelium::Scripting::Lang
{

// Forward declarations to resolve circular dependencies
struct VariableSymbol;
struct MethodSymbol;
struct ClassSymbol;

// Enhanced variable symbol with semantic information
struct VariableSymbol {
    std::string name;
    std::shared_ptr<TypeNameNode> type;
    SourceLocation declaration_location;
    bool is_used = false;
    
    // Enhanced semantic information
    bool is_parameter = false;
    bool is_field = false;
    std::string owning_scope; // Method, class, or namespace name
    bool is_definitely_assigned = false; // For definite assignment analysis
    const ClassSymbol* class_info = nullptr; // Reference to class type info for class types
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

// UNIFIED CLASS SYMBOL - Contains both semantic AND LLVM information
struct ClassSymbol {
    // === SEMANTIC INFORMATION ===
    std::string name;
    SourceLocation declaration_location;
    std::vector<std::string> field_names;
    std::vector<std::shared_ptr<TypeNameNode>> field_types;
    std::vector<MethodSymbol> methods;
    
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
    
    // === LLVM CODE GENERATION INFORMATION ===
    // (Previously duplicated in ClassTypeInfo - now unified here)
    uint32_t type_id = 0;
    llvm::StructType* fieldsType = nullptr; 
    std::map<std::string, unsigned> field_indices; // Field name -> index mapping
    std::vector<std::shared_ptr<TypeNameNode>> field_ast_types; // Store AST TypeNameNode for each field
    llvm::Function* destructor_func = nullptr; // Pointer to the LLVM function for the destructor
    
    // VTable support for polymorphism
    llvm::GlobalVariable* vtable_global = nullptr; // Global variable containing the vtable
    llvm::StructType* vtable_type = nullptr;       // LLVM type for the vtable struct
    
    // Convenience methods
    bool has_llvm_types_generated() const { return fieldsType != nullptr; }
    bool has_virtual_methods() const { return !virtual_method_order.empty(); }
    std::vector<std::string> get_all_field_names() const { return field_names; } // Includes inherited
};

} // namespace Mycelium::Scripting::Lang