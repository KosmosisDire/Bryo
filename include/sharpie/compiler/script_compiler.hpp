#pragma once

#include "../script_ast.hpp" // Updated path
#include "class_type_info.hpp"
#include "scope_manager.hpp"
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
    // PointerType forward declaration might not be needed if we always use llvm::Type* for opaque ptr
}

// All AST nodes (CompilationUnitNode, TypeNameNode, AstNode, etc.) are included via ../script_ast.hpp
// SourceLocation is also included via ../script_ast.hpp -> ast_base.hpp -> ast_location.hpp
namespace Mycelium::Scripting::Lang
{

class ScriptCompiler
{
public:
    ScriptCompiler();
    ~ScriptCompiler();

    void compile_ast(std::shared_ptr<CompilationUnitNode> ast_root, const std::string& module_name = "MyceliumModule");
    std::string get_ir_string() const;
    void dump_ir() const;

    static void initialize_jit_engine_dependencies();
    llvm::GenericValue jit_execute_function(const std::string& function_name,
                                            const std::vector<llvm::GenericValue>& args = {});
    
    static void initialize_aot_engine_dependencies();
    void compile_to_object_file(const std::string& output_filename);

private:
    // Forward declaration for internal structs
    struct ExpressionVisitResult;
    struct VariableInfo;

    std::unique_ptr<llvm::LLVMContext> llvmContext;
    std::unique_ptr<llvm::Module> llvmModule;
    std::unique_ptr<llvm::IRBuilder<>> llvmBuilder;

    static bool jit_initialized;
    static bool aot_initialized;

    std::map<std::string, VariableInfo> namedValues;
    llvm::Function* currentFunction = nullptr;

    llvm::StructType *myceliumStringType = nullptr;
    llvm::StructType *myceliumObjectHeaderType = nullptr;
    
    // Scope-based object lifecycle management
    std::unique_ptr<ScopeManager> scope_manager;
    
    std::map<std::string, ClassTypeInfo> classTypeRegistry;
    uint32_t next_type_id = 0;
    std::map<llvm::Function*, const ClassTypeInfo*> functionReturnClassInfoMap; // Map LLVM function to its return ClassTypeInfo if object
    
    // Primitive struct registry for supporting methods on primitives
    PrimitiveStructRegistry primitive_registry;

    struct ExpressionVisitResult {
        llvm::Value* value = nullptr; // Primary value, e.g., result of an operation, or fields_ptr for an object
        const ClassTypeInfo* classInfo = nullptr; // Static type info if 'value' is an object
        llvm::Value* header_ptr = nullptr; // Direct pointer to the object's header (for ARC), if applicable
        PrimitiveStructInfo* primitive_info = nullptr; // Primitive type info for method chaining on primitive values

        ExpressionVisitResult(llvm::Value* v = nullptr, const ClassTypeInfo* ci = nullptr, llvm::Value* hp = nullptr)
            : value(v), classInfo(ci), header_ptr(hp), primitive_info(nullptr) {}
    };

    struct VariableInfo {
        llvm::AllocaInst* alloca = nullptr;
        const ClassTypeInfo* classInfo = nullptr; 
        std::shared_ptr<TypeNameNode> declaredTypeNode = nullptr;
    };


    // Loop context tracking for break/continue statements
    struct LoopContext {
        llvm::BasicBlock* exit_block;     // Where 'break' should jump
        llvm::BasicBlock* continue_block; // Where 'continue' should jump
        
        LoopContext(llvm::BasicBlock* exit, llvm::BasicBlock* cont)
            : exit_block(exit), continue_block(cont) {}
    };
    std::vector<LoopContext> loop_context_stack;



    llvm::Type* getMyceliumStringPtrTy(); // Returns opaque ptr (llvm::Type*)
    llvm::Type* getMyceliumObjectHeaderPtrTy(); // Returns opaque ptr (llvm::Type*)
    void declare_all_runtime_functions();
    std::unique_ptr<llvm::Module> take_module();

    // --- Visitor Methods (snake_case) ---
    llvm::Value* visit(std::shared_ptr<AstNode> node); 

    llvm::Value* visit(std::shared_ptr<CompilationUnitNode> node);
    llvm::Value* visit(std::shared_ptr<ClassDeclarationNode> node);
    llvm::Value* visit(std::shared_ptr<NamespaceDeclarationNode> node);
    void visit(std::shared_ptr<ExternalMethodDeclarationNode> node);
    llvm::Function* visit_method_declaration(std::shared_ptr<MethodDeclarationNode> node, const std::string& class_name);
    
    // Two-pass method compilation support
    llvm::Function* declare_method_signature(std::shared_ptr<MethodDeclarationNode> node, const std::string& class_name);
    void compile_method_body(std::shared_ptr<MethodDeclarationNode> node, const std::string& class_name);
    llvm::Function* declare_constructor_signature(std::shared_ptr<ConstructorDeclarationNode> node, const std::string& class_name);
    void compile_constructor_body(std::shared_ptr<ConstructorDeclarationNode> node, const std::string& class_name);
    llvm::Function* declare_destructor_signature(std::shared_ptr<DestructorDeclarationNode> node, const std::string& class_name);
    llvm::Function* compile_destructor_body(std::shared_ptr<DestructorDeclarationNode> node, const std::string& class_name);
    llvm::Value* visit(std::shared_ptr<StatementNode> node);
    llvm::Value* visit(std::shared_ptr<BlockStatementNode> node);
    llvm::Value* visit(std::shared_ptr<LocalVariableDeclarationStatementNode> node);
    llvm::Value* visit(std::shared_ptr<ExpressionStatementNode> node);
    llvm::Value* visit(std::shared_ptr<IfStatementNode> node);
    llvm::Value* visit(std::shared_ptr<WhileStatementNode> node);
    llvm::Value* visit(std::shared_ptr<ForStatementNode> node);
    llvm::Value* visit(std::shared_ptr<ReturnStatementNode> node);
    llvm::Value* visit(std::shared_ptr<BreakStatementNode> node);
    llvm::Value* visit(std::shared_ptr<ContinueStatementNode> node);
    llvm::Function* visit(std::shared_ptr<ConstructorDeclarationNode> node, const std::string& class_name);
    llvm::Function* visit(std::shared_ptr<DestructorDeclarationNode> node, const std::string& class_name);

    ExpressionVisitResult visit(std::shared_ptr<ExpressionNode> node);
    ExpressionVisitResult visit(std::shared_ptr<LiteralExpressionNode> node);
    ExpressionVisitResult visit(std::shared_ptr<IdentifierExpressionNode> node);
    ExpressionVisitResult visit(std::shared_ptr<BinaryExpressionNode> node);
    ExpressionVisitResult visit(std::shared_ptr<AssignmentExpressionNode> node);
    ExpressionVisitResult visit(std::shared_ptr<UnaryExpressionNode> node);
    ExpressionVisitResult visit(std::shared_ptr<MethodCallExpressionNode> node);
    ExpressionVisitResult visit(std::shared_ptr<ObjectCreationExpressionNode> node);
    ExpressionVisitResult visit(std::shared_ptr<ThisExpressionNode> node);
    ExpressionVisitResult visit(std::shared_ptr<CastExpressionNode> node);
    ExpressionVisitResult visit(std::shared_ptr<MemberAccessExpressionNode> node);
    ExpressionVisitResult visit(std::shared_ptr<ParenthesizedExpressionNode> node);

    // --- Helper Methods (snake_case) ---
    llvm::Value* getHeaderPtrFromFieldsPtr(llvm::Value* fieldsPtr, llvm::StructType* fieldsLLVMType);
    llvm::Value* getFieldsPtrFromHeaderPtr(llvm::Value* headerPtr, llvm::StructType* fieldsLLVMType);
    llvm::Type* get_llvm_type(std::shared_ptr<TypeNameNode> type_node);
    llvm::Type* get_llvm_type_from_string(const std::string& type_name, std::optional<SourceLocation> loc = std::nullopt);
    std::string llvm_type_to_string(llvm::Type* type) const;

    llvm::AllocaInst* create_entry_block_alloca(llvm::Function* function, const std::string& var_name, llvm::Type* type);

    // --- Primitive struct helper methods ---
    std::string get_primitive_name_from_llvm_type(llvm::Type* type);
    ExpressionVisitResult handle_primitive_method_call(std::shared_ptr<MethodCallExpressionNode> node, PrimitiveStructInfo* primitive_info, llvm::Value* instance_ptr);



    [[noreturn]] void log_error(const std::string& message, std::optional<SourceLocation> loc = std::nullopt);
};

} // namespace Mycelium::Scripting::Lang
