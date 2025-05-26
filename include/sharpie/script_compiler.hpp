#pragma once

#include "script_ast.hpp"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ExecutionEngine/GenericValue.h" // For JIT return value

#include <string>
#include <memory>
#include <map>
#include <stdexcept>
#include <vector>
#include <optional>

namespace llvm {
    class LLVMContext;
    class Module;
    template <typename FolderTy, typename InserterTy> class IRBuilder;
    class Function;
    class AllocaInst;
    class Value;
    class Type;
    class BasicBlock;
    class StructType;
    class PointerType;
}

namespace Mycelium::Scripting::Lang
{

class ScriptCompiler
{
public:
    ScriptCompiler();
    ~ScriptCompiler();

    // Compiles the AST into the internal LLVM Module.
    // Throws std::runtime_error on critical failure.
    // Module ownership remains with ScriptCompiler until explicitly taken (e.g., for JIT).
    void compile_ast(std::shared_ptr<CompilationUnitNode> ast_root, const std::string& module_name = "MyceliumModule");

    // Returns the LLVM IR as a string.
    std::string get_ir_string() const;

    // Dumps the LLVM IR to llvm::errs() (typically stderr).
    void dump_ir() const;

    // --- JIT Execution ---
    // Initializes LLVM for JIT (call once if needed globally, or it will be called internally).
    static void initialize_jit_engine_dependencies();

    // JIT compiles the current module and executes the specified function.
    // Returns the GenericValue result from the function.
    // Throws std::runtime_error on failure.
    // Takes ownership of the internal LLVM Module.
    llvm::GenericValue jit_execute_function(const std::string& function_name,
                                            const std::vector<llvm::GenericValue>& args = {});
    
    // --- AOT Compilation to Object File ---
    // Initializes LLVM for AOT (call once if needed globally, or it will be called internally).
    static void initialize_aot_engine_dependencies();

    // Compiles the current module to an object file.
    // Throws std::runtime_error on failure.
    // The internal LLVM Module is consumed by this process if successful,
    // but it's generally better to re-compile AST if you need the module again.
    void compile_to_object_file(const std::string& output_filename);


private:
    std::unique_ptr<llvm::LLVMContext> llvmContext;
    std::unique_ptr<llvm::Module> llvmModule; // Module is owned here
    std::unique_ptr<llvm::IRBuilder<>> llvmBuilder;

    std::map<std::string, llvm::AllocaInst*> namedValues;
    llvm::Function* currentFunction = nullptr;
    static std::map<std::string, llvm::Function*> functionProtos;

    // types - This is the proper struct type for MyceliumString
    llvm::StructType *myceliumStringType = nullptr;
    llvm::PointerType* getMyceliumStringPtrTy();
    void declare_all_runtime_functions();

    // Helper to transfer module ownership, e.g., to ExecutionEngine
    std::unique_ptr<llvm::Module> take_module();


    // --- Visitor Methods (snake_case) ---
    llvm::Value* visit(std::shared_ptr<AstNode> node);

    llvm::Value* visit(std::shared_ptr<CompilationUnitNode> node);
    llvm::Value* visit(std::shared_ptr<ClassDeclarationNode> node);
    llvm::Value* visit(std::shared_ptr<NamespaceDeclarationNode> node);
    void visit(std::shared_ptr<ExternalMethodDeclarationNode> node);

    llvm::Function* visit_method_declaration(std::shared_ptr<MethodDeclarationNode> node, const std::string& class_name);

    llvm::Value* visit(std::shared_ptr<StatementNode> node);
    llvm::Value* visit(std::shared_ptr<BlockStatementNode> node);
    llvm::Value* visit(std::shared_ptr<LocalVariableDeclarationStatementNode> node);
    llvm::Value* visit(std::shared_ptr<ExpressionStatementNode> node);
    llvm::Value* visit(std::shared_ptr<IfStatementNode> node);
    llvm::Value* visit(std::shared_ptr<ReturnStatementNode> node);

    llvm::Value* visit(std::shared_ptr<ExpressionNode> node);
    llvm::Value* visit(std::shared_ptr<LiteralExpressionNode> node);
    llvm::Value* visit(std::shared_ptr<IdentifierExpressionNode> node);
    llvm::Value* visit(std::shared_ptr<BinaryExpressionNode> node);
    llvm::Value* visit(std::shared_ptr<AssignmentExpressionNode> node);
    llvm::Value* visit(std::shared_ptr<UnaryExpressionNode> node);
    llvm::Value* visit(std::shared_ptr<MethodCallExpressionNode> node);
    llvm::Value* visit(std::shared_ptr<ObjectCreationExpressionNode> node);
    llvm::Value* visit(std::shared_ptr<ThisExpressionNode> node);

    // --- Helper Methods (snake_case) ---
    llvm::Type* get_llvm_type(std::shared_ptr<TypeNameNode> type_node);
    llvm::Type* get_llvm_type_from_string(const std::string& type_name);
    llvm::StructType* get_string_struct_type();
    std::string llvm_type_to_string(llvm::Type* type) const;

    llvm::AllocaInst* create_entry_block_alloca(llvm::Function* function, const std::string& var_name, llvm::Type* type);

    [[noreturn]] void log_error(const std::string& message, std::optional<SourceLocation> loc = std::nullopt);
};

} // namespace Mycelium::Scripting::Lang