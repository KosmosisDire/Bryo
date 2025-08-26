#pragma once

#include "symbol_table.hpp"
#include "type_system.hpp"
#include "ast/ast.hpp"
#include "symbol.hpp"
#include "conversions.hpp"
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>
#include "type.hpp"

namespace Myre
{

    /**
     * @class TypeResolver
     * @brief Performs full semantic analysis, including type checking, type inference,
     * symbol resolution, and AST annotation.
     *
     * This class is the core of the "middle-end". It validates the semantic
     * correctness of the program and enriches the AST with the information
     * needed for subsequent stages like code generation.
     */
    class TypeResolver : public DefaultVisitor
    {
    private:
        SymbolTable &symbolTable;
        TypeSystem &typeSystem;
        std::vector<std::string> errors;

        // --- Core of the Unification Solver ---
        std::unordered_map<TypePtr, TypePtr> substitution;
        std::unordered_set<TypePtr> pendingConstraints;

        // --- Intermediate State ---
        // Maps each AST node to its resolved semantic type during analysis.
        std::unordered_map<Node *, TypePtr> nodeTypes;

        // --- Generic Context Tracking ---
        // Maps type parameter names to their TypeParameter types in current generic context
        std::unordered_map<std::string, TypePtr> currentTypeParameters;

    public:
        int passes_to_run = 10;
        TypeResolver(SymbolTable &symbol_table)
            : symbolTable(symbol_table), typeSystem(symbol_table.get_type_system()) {}

        const std::vector<std::string> &get_errors() const { return errors; }

        bool resolve(CompilationUnit *unit);

    private:
        // --- Core Unification Logic ---
        TypePtr apply_substitution(TypePtr type);
        void unify(TypePtr t1, TypePtr t2, Node *error_node, const std::string &context);
        bool has_pending_constraints();
        void report_final_errors();
        void report_error(Node *error_node, const std::string &message);
        void update_ast_with_final_types(CompilationUnit *unit);

        // Single method that handles ALL expression annotations
        // Symbol is optional - many expressions don't have associated symbols
        void annotate_expression(Expression *expr, TypePtr type, Symbol *symbol = nullptr);

        Scope *get_containing_scope(Node *node);

        // Computes lvalue status by inspecting expression and symbol
        bool compute_lvalue_status(Expression *expr, Symbol *symbol);

        // Sets the appropriate symbol field based on expression type
        void set_expression_symbol(Expression *expr, Symbol *symbol);

        // Gets symbol from expression if it has one
        Symbol *get_expression_symbol(Expression *expr);

        // --- Visitor Overrides ---
        // [Keep all existing visitor declarations unchanged]
        void visit(LiteralExpr *node) override;
        void visit(ArrayLiteralExpr *node) override;
        void visit(NameExpr *node) override;
        void visit(BinaryExpr *node) override;
        void visit(AssignmentExpr *node) override;
        void visit(CallExpr *node) override;
        void visit(MemberAccessExpr *node) override;
        void visit(UnaryExpr *node) override;
        void visit(IndexerExpr *node) override;
        void visit(ConditionalExpr *node) override;
        void visit(IfExpr *node) override;
        void visit(CastExpr *node) override;
        void visit(ThisExpr *node) override;
        void visit(NewExpr *node) override;
        void visit(ReturnStmt *node) override;
        void visit(ForStmt *node) override;
        void visit(WhileStmt *node) override;
        void visit(VariableDecl *node) override;
        void visit(PropertyDecl *node) override;
        void visit(PropertyAccessor *node) override;
        void visit(FunctionDecl *node) override;
        void visit(ParameterDecl *node) override;
        void visit(TypedIdentifier *node) override;
        void visit(TypeDecl *node) override;
        void visit(GenericTypeExpr *node) override;
        void visit(PointerTypeExpr *node) override;
        void visit(TypeParameterDecl *node) override;

        // --- Helper Methods ---
        TypePtr get_node_type(Node *node);
        void set_node_type(Node *node, TypePtr type);
        TypePtr resolve_expr_type(Expression *type_expr, Scope *scope);
        TypePtr infer_function_return_type(Block *body);
        ConversionKind check_conversion(TypePtr from, TypePtr to);
        bool check_implicit_conversion(TypePtr from, TypePtr to, Node* error_node, const std::string& context);
        bool check_explicit_conversion(TypePtr from, TypePtr to, Node* error_node, const std::string& context);
        FunctionSymbol* resolve_overload(const std::vector<FunctionSymbol*>& overloads, const std::vector<TypePtr>& argTypes);
        int count_conversions(const std::vector<ConversionKind> &conversions, ConversionKind kind);
    };

} // namespace Myre