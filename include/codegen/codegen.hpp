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

class CodeGenerator : public StructuralVisitor {
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
    std::unordered_map<Symbol*, llvm::Value*> locals;
    
    // Local variable types (Symbol* -> LLVM type)
    // Required for LLVM 19+ opaque pointers
    std::unordered_map<Symbol*, llvm::Type*> local_types;
    
    // Type cache to avoid recreating LLVM types
    std::unordered_map<TypePtr, llvm::Type*> type_cache;
    
    // Stack for expression evaluation
    std::stack<llvm::Value*> value_stack;
    
    // Track which functions have been declared
    std::unordered_set<std::string> declared_functions;
    
    // Error tracking
    std::vector<std::string> errors;
    
    // === Helper Methods ===
    
    // Type conversion
    llvm::Type* get_llvm_type(TypePtr type);
    
    // Stack management
    void push_value(llvm::Value* val);
    llvm::Value* pop_value();
    
    // Constants
    llvm::Value* create_constant(LiteralExpressionNode* literal);
    
    // Function utilities
    void ensure_terminator();
    llvm::Function* declare_function_from_symbol(FunctionSymbol* func_symbol);
    
    // Symbol table traversal
    void declare_all_functions_in_scope(Scope* scope);
    
public:
    CodeGenerator(SymbolTable& st, const std::string& module_name, llvm::LLVMContext* ctx)
        : symbol_table(st), context(ctx) {
        module = std::make_unique<llvm::Module>(module_name, *context);
        builder = std::make_unique<llvm::IRBuilder<>>(*context);
    }
    
    // === Main API ===
    
    // Generate code for a compilation unit (single file)
    std::unique_ptr<llvm::Module> generate(CompilationUnitNode* unit);
    
    // Multi-file support: declare all functions then generate bodies
    void declare_all_functions();
    void generate_definitions(CompilationUnitNode* unit);
    
    // Release ownership of the module
    std::unique_ptr<llvm::Module> release_module() { return std::move(module); }
    
    // Get errors
    const std::vector<std::string>& get_errors() const { return errors; }
    
    // === Visitor Methods ===
    
    // Declarations
    void visit(CompilationUnitNode* node) override;
    void visit(NamespaceDeclarationNode* node) override;
    void visit(FunctionDeclarationNode* node) override;
    void visit(VariableDeclarationNode* node) override;
    void visit(ParameterNode* node) override;
    
    // Statements
    void visit(BlockStatementNode* node) override;
    void visit(ExpressionStatementNode* node) override;
    void visit(IfStatementNode* node) override;
    void visit(ReturnStatementNode* node) override;
    void visit(WhileStatementNode* node) override;
    void visit(ForStatementNode* node) override;
    void visit(ForInStatementNode* node) override;
    void visit(BreakStatementNode* node) override;
    void visit(ContinueStatementNode* node) override;
    void visit(EmptyStatementNode* node) override;
    
    // Expressions
    void visit(BinaryExpressionNode* node) override;
    void visit(UnaryExpressionNode* node) override;
    void visit(AssignmentExpressionNode* node) override;
    void visit(CallExpressionNode* node) override;
    void visit(IdentifierExpressionNode* node) override;
    void visit(LiteralExpressionNode* node) override;
    void visit(ParenthesizedExpressionNode* node) override;
    
    // Default handlers for unsupported nodes
    void visit(ExpressionNode* node) override {
        errors.push_back("Unsupported expression type: " + std::string(node->node_type_name()));
    }
    
    void visit(StatementNode* node) override {
        errors.push_back("Unsupported statement type: " + std::string(node->node_type_name()));
    }
    
    void visit(DeclarationNode* node) override {
        errors.push_back("Unsupported declaration type: " + std::string(node->node_type_name()));
    }
    
    void visit(ErrorNode* node) override {
        errors.push_back("Encountered error node in AST");
    }
};

} // namespace Myre