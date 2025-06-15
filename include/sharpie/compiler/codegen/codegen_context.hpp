#pragma once

#include "sharpie/compiler/scope_manager.hpp"
#include "sharpie/semantic_analyzer/semantic_ir.hpp"
#include "sharpie/script_ast.hpp"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Function.h"

#include <map>
#include <string>
#include <vector>
#include <memory>

// Forward declarations to keep this header cleaner
namespace llvm {
    class Value;
    class AllocaInst;
    class BasicBlock;
}

namespace Mycelium::Scripting::Lang::CodeGen {

// Forward-declare structs that will be defined below.
struct VariableInfo;
struct LoopContext;

// Holds all transient state and references needed during a single compilation run.
// An instance of this is created by ScriptCompiler and passed to the CodeGenerator.
struct CodeGenContext {
    // LLVM Core Objects
    llvm::LLVMContext& llvm_context;
    llvm::Module& llvm_module;
    llvm::IRBuilder<>& builder;

    // Compiler State & Semantic Information
    ScopeManager& scope_manager;
    SymbolTable& symbol_table;
    PrimitiveStructRegistry& primitive_registry;

    // Per-function state, managed by the CodeGenerator
    llvm::Function* current_function = nullptr;
    std::map<std::string, VariableInfo>& named_values;
    std::map<llvm::Function*, const SymbolTable::ClassSymbol*>& function_return_class_info_map;
    std::vector<LoopContext>& loop_context_stack;
};

// Represents a local variable or parameter in the current function scope.
struct VariableInfo {
    llvm::AllocaInst* alloca = nullptr;
    const SymbolTable::ClassSymbol* classInfo = nullptr;
    std::shared_ptr<TypeNameNode> declaredTypeNode = nullptr;
};

// Represents the context for a loop (for break/continue).
struct LoopContext {
    llvm::BasicBlock* exit_block;
    llvm::BasicBlock* continue_block;
    
    LoopContext(llvm::BasicBlock* exit, llvm::BasicBlock* cont) 
        : exit_block(exit), continue_block(cont) {}
};

} // namespace Mycelium::Scripting::Lang::CodeGen