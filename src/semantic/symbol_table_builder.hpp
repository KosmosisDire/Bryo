#pragma once

#include "ast/ast.hpp"
#include "symbol_table.hpp"
#include <vector>
#include <string>

namespace Bryo
{

    // Forward declarations
    class SymbolTable;
    class TypeSystem;

    class SymbolTableBuilder : public DefaultVisitor
    {
    private:
        SymbolTable &symbolTable;
        TypeSystem &typeSystem;
        std::vector<std::string> errors;

        // === Helper Methods ===

        // Get current scope handle
        SymbolHandle get_current_handle();

        // Annotate node with current scope
        void annotate_scope(BaseSyntax *node);

        // Visit block contents without creating a new scope
        void visit_block_contents(BlockSyntax *block);

        // Create unresolved type from type expression (does not actually resolve)
        TypePtr create_unresolved_type(BaseExprSyntax *typeExpr);

        // Helper for property accessors
        void visit_property_accessor(PropertyAccessorSyntax *accessor, TypePtr propType);

    public:
        // Constructor
        explicit SymbolTableBuilder(SymbolTable &st);

        // Main entry point
        void collect(CompilationUnitSyntax *unit);

        // Get collected errors
        const std::vector<std::string> &get_errors() const { return errors; }
        bool has_errors() const { return !errors.empty(); }

        // === Visitor Overrides ===

        // Base node - annotates all nodes with scope
        void visit(BaseSyntax *node) override;

        // Root
        void visit(CompilationUnitSyntax *node) override;

        // === Declarations - These create symbols ===
        void visit(NamespaceDeclSyntax *node) override;
        void visit(TypeDeclSyntax *node) override;
        void visit(FunctionDeclSyntax *node) override;
        void visit(VariableDeclSyntax *node) override;
        void visit(PropertyDeclSyntax *node) override;
        void visit(EnumCaseDeclSyntax *node) override;

        // === Statements with scopes ===
        void visit(BlockSyntax *node) override;
        void visit(WhileStmtSyntax *node) override;
        void visit(ForStmtSyntax *node) override;
        void visit(IfStmtSyntax *node) override;

        // === Type expressions ===
        void visit(ArrayTypeSyntax *node) override;
        void visit(PointerTypeSyntax *node) override;
        void visit(TypeParameterDeclSyntax *node) override;
    };

} // namespace Bryo