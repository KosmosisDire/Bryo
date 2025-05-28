#pragma once

#include "../script_ast.hpp" // Updated path
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
    struct ClassTypeInfo; 
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
    
    struct ClassTypeInfo {
        std::string name;
        uint32_t type_id;
        llvm::StructType* fieldsType; 
        std::vector<std::string> field_names_in_order;
        std::map<std::string, unsigned> field_indices;
        std::vector<std::shared_ptr<TypeNameNode>> field_ast_types; // Store AST TypeNameNode for each field
        llvm::Function* destructor_func = nullptr; // Pointer to the LLVM function for the destructor
    };
    std::map<std::string, ClassTypeInfo> classTypeRegistry;
    uint32_t next_type_id = 0;

    struct ExpressionVisitResult {
        llvm::Value* value = nullptr;
        const ClassTypeInfo* classInfo = nullptr; 

        ExpressionVisitResult(llvm::Value* v = nullptr, const ClassTypeInfo* ci = nullptr)
            : value(v), classInfo(ci) {}
    };

    struct VariableInfo {
        llvm::AllocaInst* alloca = nullptr;
        const ClassTypeInfo* classInfo = nullptr; 
        std::shared_ptr<TypeNameNode> declaredTypeNode = nullptr;
    };

    std::map<llvm::AllocaInst*, llvm::Value*> current_function_arc_locals;

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
    llvm::Value* visit(std::shared_ptr<StatementNode> node);
    llvm::Value* visit(std::shared_ptr<BlockStatementNode> node);
    llvm::Value* visit(std::shared_ptr<LocalVariableDeclarationStatementNode> node);
    llvm::Value* visit(std::shared_ptr<ExpressionStatementNode> node);
    llvm::Value* visit(std::shared_ptr<IfStatementNode> node);
    llvm::Value* visit(std::shared_ptr<ReturnStatementNode> node);
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

    [[noreturn]] void log_error(const std::string& message, std::optional<SourceLocation> loc = std::nullopt);
};

} // namespace Mycelium::Scripting::Lang
