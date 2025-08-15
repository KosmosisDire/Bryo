// codegen.hpp - LLVM Code Generator with Pre-declaration Support
#pragma once

#include "ast/ast.hpp"
#include "semantic/symbol_table.hpp"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <memory>

namespace Myre {

/**
 * @struct CodeGenError
 * @brief Represents a single error that occurred during code generation.
 */
struct CodeGenError {
    std::string message;
    SourceRange location;

    std::string to_string() const {
        return "Error at " + std::to_string(location.start.line) + ":" +
               std::to_string(location.start.column) + " - " + message;
    }
};

class CodeGenerator : public DefaultVisitor {
private:
    // LLVM core objects
    llvm::LLVMContext* context;
    std::unique_ptr<llvm::Module> module;
    std::unique_ptr<llvm::IRBuilder<>> builder;
    
    // Symbol table reference
    SymbolTable& symbol_table;
    
    // Current function being generated
    llvm::Function* current_function = nullptr;
    
    // Local variable storage (Symbol* -> alloca instruction)
    std::unordered_map<ScopeNode*, llvm::Value*> locals;
    
    // Local variable types (Symbol* -> LLVM type)
    // Required for LLVM 19+ opaque pointers
    std::unordered_map<ScopeNode*, llvm::Type*> local_types;
    
    // Type cache to avoid recreating LLVM types
    std::unordered_map<TypePtr, llvm::Type*> type_cache;
    
    // Stack for expression evaluation
    std::stack<llvm::Value*> value_stack;
    
    // Track which functions have been declared
    std::unordered_set<std::string> declared_functions;
    
    // Error tracking
    std::vector<CodeGenError> errors;
    
    // === Helper Methods ===
    
    // Error reporting
    void report_error(const Node* node, const std::string& message);
    void report_general_error(const std::string& message);

    // Type conversion
    llvm::Type* get_llvm_type(TypePtr type);
    llvm::Type* get_llvm_type_from_ref(TypeRef* type_ref);
    
    // Stack management
    void push_value(llvm::Value* val);
    llvm::Value* pop_value();
    
    // Constants
    llvm::Value* create_constant(LiteralExpr* literal);
    
    // Function utilities
    void ensure_terminator();
    llvm::Function* declare_function_from_symbol(FunctionSymbol* func_symbol);
    
    // Symbol table traversal
    void declare_all_functions_in_scope(Scope* scope);
    
    // Get symbol from node's resolved handle
    Scope* get_containing_scope(Node* node);
    
    // Build qualified name from name expression
    std::string build_qualified_name(NameExpr* name_expr);
    
public:
    CodeGenerator(SymbolTable& st, const std::string& module_name, llvm::LLVMContext* ctx)
        : symbol_table(st), context(ctx) {
        module = std::make_unique<llvm::Module>(module_name, *context);
        builder = std::make_unique<llvm::IRBuilder<>>(*context);
    }
    
    // === Main API ===
    
    // Generate code for a compilation unit
    std::unique_ptr<llvm::Module> generate(CompilationUnit* unit);
    
    // Multi-file support: declare all functions then generate bodies
    void declare_all_functions();
    void generate_definitions(CompilationUnit* unit);
    
    // Release ownership of the module
    std::unique_ptr<llvm::Module> release_module() { return std::move(module); }
    
    // Get errors
    const std::vector<CodeGenError>& get_errors() const { return errors; }
    
    // === Visitor Methods (override from DefaultVisitor) ===
    
    // Root
    void visit(CompilationUnit* node) override;
    
    // Declarations
    void visit(NamespaceDecl* node) override;
    void visit(FunctionDecl* node) override;
    void visit(VariableDecl* node) override;
    void visit(ParameterDecl* node) override;
    void visit(TypeDecl* node) override;
    
    // Statements
    void visit(Block* node) override;
    void visit(ExpressionStmt* node) override;
    void visit(ReturnStmt* node) override;
    void visit(WhileStmt* node) override;
    void visit(ForStmt* node) override;
    void visit(ForInStmt* node) override;
    void visit(BreakStmt* node) override;
    void visit(ContinueStmt* node) override;
    
    // Expressions  
    void visit(BinaryExpr* node) override;
    void visit(UnaryExpr* node) override;
    void visit(AssignmentExpr* node) override;
    void visit(CallExpr* node) override;
    void visit(NameExpr* node) override;
    void visit(LiteralExpr* node) override;
    void visit(IfExpr* node) override;
    void visit(ConditionalExpr* node) override;
    
    // Error handling
    void visit(ErrorExpression* node) override;
    void visit(ErrorStatement* node) override;
    void visit(ErrorTypeRef* node) override;
    
    // Default handlers for unsupported nodes
    void visit(Expression* node) override {
        report_error(node, "Codegen for this expression type is not yet implemented.");
    }
    
    void visit(Statement* node) override {
        if (auto* decl = dynamic_cast<Declaration*>(node)) {
             visit(decl);
        } else {
             report_error(node, "Codegen for this statement type is not yet implemented.");
        }
    }
    
    void visit(Declaration* node) override {
        report_error(node, "Codegen for this declaration type is not yet implemented.");
    }
};

} // namespace Myre