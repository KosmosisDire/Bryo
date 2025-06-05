#pragma once

#include "llvm/IR/Type.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "../script_ast.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace llvm {
    class StructType;
    class Function;
    class GlobalVariable;
}

namespace Mycelium::Scripting::Lang
{

/**
 * Information about a class type used by the compiler
 */
struct ClassTypeInfo {
    std::string name;
    uint32_t type_id;
    llvm::StructType* fieldsType; 
    std::vector<std::string> field_names_in_order;
    std::map<std::string, unsigned> field_indices;
    std::vector<std::shared_ptr<TypeNameNode>> field_ast_types; // Store AST TypeNameNode for each field
    llvm::Function* destructor_func = nullptr; // Pointer to the LLVM function for the destructor
    
    // VTable support for polymorphism
    llvm::GlobalVariable* vtable_global = nullptr; // Global variable containing the vtable
    llvm::StructType* vtable_type = nullptr;       // LLVM type for the vtable struct
};

} // namespace Mycelium::Scripting::Lang
