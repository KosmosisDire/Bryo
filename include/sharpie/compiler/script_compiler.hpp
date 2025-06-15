#pragma once

#include "sharpie/script_ast.hpp"
#include "sharpie/semantic_analyzer/semantic_analyzer.hpp"
#include "sharpie/compiler/scope_manager.hpp"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/ExecutionEngine/GenericValue.h"

#include <string>
#include <memory>
#include <map>
#include <vector>
#include <optional>

namespace llvm {
    class Function;
}

namespace Mycelium::Scripting::Lang
{

/**
 * @class ScriptCompiler
 * @brief Orchestrates the entire compilation pipeline from AST to executable code.
 *
 * SIMPLIFIED ARCHITECTURE:
 * - Single source of truth: SymbolTable with unified ClassSymbol
 * - No duplicate class registries
 * - Direct access to semantic information during code generation
 */
class ScriptCompiler
{
public:
    ScriptCompiler();
    ~ScriptCompiler();

    // --- Main Compilation Pipeline ---
    void compile_ast(std::shared_ptr<CompilationUnitNode> ast_root, const std::string& module_name = "MyceliumModule");
    
    // --- Post-Compilation Actions ---
    std::string get_ir_string() const;
    void dump_ir() const;
    
    // --- Semantic Analysis Interface ---
    const SemanticIR* get_semantic_ir() const;
    bool has_semantic_errors() const;

    // --- JIT / AOT Execution ---
    static void initialize_jit_engine_dependencies();
    llvm::GenericValue jit_execute_function(const std::string& function_name,
                                            const std::vector<llvm::GenericValue>& args = {});
    
    static void initialize_aot_engine_dependencies();
    void compile_to_object_file(const std::string& output_filename);

private:
    // --- Core LLVM & Compiler Objects ---
    std::unique_ptr<llvm::LLVMContext> llvm_context;
    std::unique_ptr<llvm::Module> llvm_module;
    std::unique_ptr<llvm::IRBuilder<>> builder;
    std::unique_ptr<ScopeManager> scope_manager;

    // --- Semantic Analysis (SINGLE SOURCE OF TRUTH) ---
    std::unique_ptr<SemanticAnalyzer> semantic_analyzer;
    std::unique_ptr<SemanticIR> semantic_ir;
    
    // --- Type Registries & Info ---
    PrimitiveStructRegistry primitive_registry;
    uint32_t next_type_id = 0;
    
    // --- JIT / AOT State ---
    static bool jit_initialized;
    static bool aot_initialized;

    // --- Private Helper Methods ---
    void initialize_for_new_compilation(const std::string& module_name);
    void declare_all_runtime_functions();
    void assign_type_ids_to_classes();
    std::unique_ptr<llvm::Module> take_module(); // For passing ownership to JIT/AOT
};

} // namespace Mycelium::Scripting::Lang