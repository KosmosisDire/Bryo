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

namespace Bryo
{

    /**
     * @struct CodeGenError
     * @brief Represents a single error that occurred during code generation.
     */
    struct CodeGenError
    {
        std::string message;
        SourceRange location;

        std::string to_string() const
        {
            if (location.start.line > 0)
            {
                return "Error at " + std::to_string(location.start.line) + ":" +
                       std::to_string(location.start.column) + " - " + message;
            }
            return "General Error - " + message;
        }
    };

    class CodeGenerator : public Visitor
    {
    private:
        // LLVM core objects
        llvm::LLVMContext *context;
        std::unique_ptr<llvm::Module> module;
        std::unique_ptr<llvm::IRBuilder<>> builder;

        // Symbol table reference
        SymbolTable &symbol_table;

        // Current function being generated
        llvm::Function *current_function = nullptr;

        // Local variable storage (Symbol* -> alloca instruction)
        std::unordered_map<ScopeNode *, llvm::Value *> locals;

        // Local variable types (Symbol* -> LLVM type)
        std::unordered_map<ScopeNode *, llvm::Type *> local_types;

        // Type cache to avoid recreating LLVM types
        std::unordered_map<TypePtr, llvm::Type *> type_cache;

        // Cache for user-defined struct types (TypeSymbol -> llvm::Type)
        std::unordered_map<TypeSymbol *, llvm::Type *> defined_types;

        // Stack for expression evaluation
        std::stack<llvm::Value *> value_stack;

        struct LoopContext {
            llvm::BasicBlock* breakTarget;    // Where 'break' jumps to
            llvm::BasicBlock* continueTarget; // Where 'continue' jumps to
        };
        std::stack<LoopContext> loop_stack;

        // Track which functions have been declared
        std::unordered_set<std::string> declared_functions;

        // Error tracking
        std::vector<CodeGenError> errors;

        // === Helper Methods ===
        void debug_print_module_state(const std::string& phase);
        void report_error(const BaseSyntax *node, const std::string &message);
        void report_general_error(const std::string &message);
        llvm::Type *get_llvm_type(TypePtr type);
        void push_value(llvm::Value *val);
        llvm::Value *pop_value();
        llvm::Value *create_constant(LiteralExprSyntax *literal);
        void ensure_terminator();
        Scope *get_containing_scope(BaseSyntax *node);
        Symbol *get_expression_symbol(BaseExprSyntax *expr);
        std::string build_qualified_name(NameExpr *name_expr);

        // Core expression generation helpers
        llvm::Value* genLValue(BaseExprSyntax* expr);  // Returns address
        llvm::Value* genRValue(BaseExprSyntax* expr);  // Returns value
        llvm::Value* genExpression(BaseExprSyntax* expr, bool wantAddress = false);

        // Helper method to cast between primitive types
        llvm::Value* castPrimitive(llvm::Value* value, PrimitiveType::Kind sourceKind, PrimitiveType::Kind targetKind, BaseSyntax* node);

        // Storage-aware loading/storing
        llvm::Value* loadValue(llvm::Value* ptr, TypePtr type);
        void storeValue(llvm::Value* value, llvm::Value* ptr, TypePtr type);
        
        // Type-aware value extraction
        llvm::Value* ensureValue(llvm::Value* val, TypePtr type);
        llvm::Value* ensureAddress(llvm::Value* val, TypePtr type);

        // type properties
        bool isUnsignedType(TypePtr type);
        bool isSignedType(TypePtr type);
        bool isFloatingPointType(TypePtr type);
        bool isIntegerType(TypePtr type);

        // --- Pre-declaration passes ---
        void declare_all_types_in_scope(Scope *scope);
        void declare_all_functions_in_scope(Scope *scope);
        llvm::Function *declare_function_from_symbol(FunctionSymbol *func_symbol);
        void generate_property_getter(PropertyDeclSyntax *prop_decl, TypeSymbol *type_symbol, llvm::StructType *struct_type);

    public:
        CodeGenerator(SymbolTable &st, const std::string &module_name, llvm::LLVMContext *ctx)
            : symbol_table(st), context(ctx)
        {
            module = std::make_unique<llvm::Module>(module_name, *context);
            builder = std::make_unique<llvm::IRBuilder<>>(*context);
        }

        // === Main API ===

        std::unique_ptr<llvm::Module> generate(CompilationUnitSyntax *unit);
        void declare_all_functions();
        void declare_all_types();
        void generate_builtin_functions();
        void generate_definitions(CompilationUnitSyntax *unit);
        std::unique_ptr<llvm::Module> release_module() { return std::move(module); }
        const std::vector<CodeGenError> &get_errors() const { return errors; }

        // === Visitor Method Overrides ===

        void visit(BaseSyntax *node) override;
        void visit(BaseExprSyntax *node) override;
        void visit(BaseStmtSyntax *node) override;
        void visit(BaseDeclSyntax *node) override;

        // Root
        void visit(CompilationUnitSyntax *node) override;

        // Declarations
        void visit(NamespaceDeclSyntax *node) override;
        void visit(TypeDeclSyntax *node) override;
        void visit(FunctionDeclSyntax *node) override;
        void visit(VariableDeclSyntax *node) override;
        void visit(PropertyDeclSyntax *node) override;
        void visit(ParameterDeclSyntax *node) override;

        // Statements
        void visit(Block *node) override;
        void visit(ExpressionStmtSyntax *node) override;
        void visit(ReturnStmtSyntax *node) override;

        // Expressions
        void visit(BinaryExpr *node) override;
        void visit(UnaryExpr *node) override;
        void visit(AssignmentExpr *node) override;
        void visit(CallExpr *node) override;
        void visit(NameExpr *node) override;
        void visit(LiteralExprSyntax *node) override;
        void visit(NewExpr *node) override;
        void visit(SimpleNameExprSyntax *node) override;

        // Errors
        void visit(MissingSyntax *node) override;
        void visit(MissingSyntax *node) override;

        // --- Unimplemented visitors will be caught by the base overrides ---
        void visit(TypedIdentifier *n) override {}
        void visit(ArrayLiteralExprSyntax *n) override;
        void visit(QualifiedNameSyntax *n) override;
        void visit(IndexerExprSyntax *n) override;
        void visit(CastExpr *n) override;
        void visit(ThisExpr *n) override;
        void visit(LambdaExpr *n) override;
        void visit(ConditionalExpr *n) override;
        void visit(TypeOfExpr *n) override;
        void visit(SizeOfExpr *n) override;
        void visit(IfStmt *n) override;
        void visit(BreakStmtSyntax *n) override;
        void visit(ContinueStmtSyntax *n) override;
        void visit(WhileStmtSyntax *n) override;
        void visit(ForStmtSyntax *n) override;
        void visit(UsingDirectiveSyntax *n) override;
        void visit(ConstructorDeclSyntax *n) override;
        void visit(PropertyAccessorSyntax *n) override;
        void visit(EnumCaseDeclSyntax *n) override;

        // Type expressions (now regular expressions) - treated as regular expressions
        void visit(ArrayTypeSyntax *n) override;
        void visit(FunctionTypeExpr *n) override;
        void visit(GenericTypeExpr *n) override;
        void visit(PointerTypeExpr *n) override;
        void visit(TypeParameterDeclSyntax *n) override;
    };

} // namespace Bryo