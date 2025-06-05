#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>
#include "ast_base.hpp"
#include "ast_types.hpp"
#include "ast_declarations.hpp"

namespace Mycelium::Scripting::Lang
{
    // Enum to identify primitive struct types
    enum class PrimitiveStructKind
    {
        Int32,
        Int64,
        Boolean,
        Float,
        Double,
        Char,
        String
    };

    // Information about a primitive struct type
    struct PrimitiveStructInfo
    {
        PrimitiveStructKind kind;
        std::string name;                    // e.g., "System.Int32"
        std::string simple_name;             // e.g., "int"
        std::string llvm_primitive_type;     // e.g., "i32"
        
        // The struct that backs this primitive
        std::shared_ptr<StructDeclarationNode> struct_declaration;
        
        // Methods that can be called on this primitive
        std::vector<std::shared_ptr<MethodDeclarationNode>> instance_methods;
        std::vector<std::shared_ptr<MethodDeclarationNode>> static_methods;
        
        PrimitiveStructInfo(PrimitiveStructKind k, const std::string& n, const std::string& sn, const std::string& llvm_type)
            : kind(k), name(n), simple_name(sn), llvm_primitive_type(llvm_type) {}
    };

    // Registry for primitive struct types
    class PrimitiveStructRegistry 
    {
    private:
        std::map<std::string, std::unique_ptr<PrimitiveStructInfo>> primitive_by_name;
        std::map<std::string, PrimitiveStructInfo*> primitive_by_simple_name;
        std::map<PrimitiveStructKind, PrimitiveStructInfo*> primitive_by_kind;

    public:
        void initialize_builtin_primitives();
        
        // Lookup functions
        PrimitiveStructInfo* get_by_name(const std::string& name);
        PrimitiveStructInfo* get_by_simple_name(const std::string& simple_name);
        PrimitiveStructInfo* get_by_kind(PrimitiveStructKind kind);
        
        // Check if a type name refers to a primitive struct
        bool is_primitive_struct(const std::string& type_name);
        bool is_primitive_simple_name(const std::string& simple_name);
        
        // Get all registered primitive structs
        std::vector<PrimitiveStructInfo*> get_all_primitives();
    };

    // Factory functions to create primitive struct AST nodes
    std::shared_ptr<StructDeclarationNode> create_int32_struct();
    std::shared_ptr<StructDeclarationNode> create_boolean_struct();
    std::shared_ptr<StructDeclarationNode> create_string_struct();
    std::shared_ptr<StructDeclarationNode> create_float_struct();
    std::shared_ptr<StructDeclarationNode> create_double_struct();
    std::shared_ptr<StructDeclarationNode> create_char_struct();
    std::shared_ptr<StructDeclarationNode> create_int64_struct();

    // Helper to create method declarations for primitive structs
    std::shared_ptr<MethodDeclarationNode> create_primitive_method(
        const std::string& method_name,
        std::shared_ptr<TypeNameNode> return_type,
        const std::vector<std::shared_ptr<ParameterDeclarationNode>>& parameters,
        bool is_static = false
    );
}
