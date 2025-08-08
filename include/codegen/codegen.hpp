#pragma once

#include "ast/ast.hpp"
#include "semantic/symbol_table.hpp"
#include "semantic/error_collector.hpp"
#include "codegen/ir_builder.hpp"
#include "codegen/ir_command.hpp"
#include <vector>

namespace Myre {

// Simple code generator that visits AST and emits IR commands
class CodeGenerator : public StructuralVisitor {
private:
    SymbolTable& symbolTable;
    ErrorCollector& errors;
    SimpleIRBuilder builder_;
    
    // Current state
    ValueRef current_value_;
    FunctionType* current_function_;
    bool in_loop_;
    
public:
    CodeGenerator(SymbolTable& symbols, ErrorCollector& errors)
        : symbolTable(symbols)
        , errors(errors)
        , current_value_(ValueRef::invalid())
        , current_function_(nullptr)
        , in_loop_(false) {}
    
    // Main entry point
    std::vector<Command> generate_code(CompilationUnitNode* unit) {
        builder_.clear();
        
        // Visit the compilation unit
        if (unit) {
            unit->accept(this);
        }
        
        return builder_.commands();
    }
    
    // Visitor methods for expressions
    void visit(LiteralExpressionNode* node) override;
    void visit(IdentifierExpressionNode* node) override;
    void visit(BinaryExpressionNode* node) override;
    void visit(UnaryExpressionNode* node) override;
    void visit(AssignmentExpressionNode* node) override;
    void visit(CallExpressionNode* node) override;
    void visit(MemberAccessExpressionNode* node) override;
    void visit(ParenthesizedExpressionNode* node) override;
    
    // Visitor methods for statements
    void visit(BlockStatementNode* node) override;
    void visit(ExpressionStatementNode* node) override;
    void visit(IfStatementNode* node) override;
    void visit(WhileStatementNode* node) override;
    void visit(ForStatementNode* node) override;
    void visit(ReturnStatementNode* node) override;
    void visit(BreakStatementNode* node) override;
    void visit(ContinueStatementNode* node) override;
    void visit(VariableDeclarationNode* node) override;
    
    // Visitor methods for declarations
    void visit(FunctionDeclarationNode* node) override;
    void visit(CompilationUnitNode* node) override;
    
    // Placeholder visitors for unimplemented nodes
    void visit(ErrorNode* node) override { /* Skip error nodes */ }
    void visit(TokenNode* node) override {}
    void visit(IdentifierNode* node) override {}
    void visit(ThisExpressionNode* node) override {}
    void visit(NewExpressionNode* node) override {}
    void visit(CastExpressionNode* node) override {}
    void visit(IndexerExpressionNode* node) override {}
    void visit(TypeOfExpressionNode* node) override {}
    void visit(SizeOfExpressionNode* node) override {}
    void visit(EmptyStatementNode* node) override {}
    void visit(ForInStatementNode* node) override {}
    void visit(NamespaceDeclarationNode* node) override {}
    void visit(UsingDirectiveNode* node) override {}
    void visit(TypeDeclarationNode* node) override {}
    void visit(InterfaceDeclarationNode* node) override {}
    void visit(EnumDeclarationNode* node) override {}
    void visit(MemberDeclarationNode* node) override {}
    void visit(ParameterNode* node) override {}
    void visit(QualifiedNameNode* node) override {}
    void visit(TypeNameNode* node) override {}
    void visit(ArrayTypeNameNode* node) override {}
    void visit(GenericTypeNameNode* node) override {}
    void visit(GenericParameterNode* node) override {}
    void visit(MatchExpressionNode* node) override {}
    void visit(ConditionalExpressionNode* node) override {}
    void visit(RangeExpressionNode* node) override {}
    void visit(FieldKeywordExpressionNode* node) override {}
    void visit(ValueKeywordExpressionNode* node) override {}
    void visit(PropertyDeclarationNode* node) override {}
    void visit(PropertyAccessorNode* node) override {}
    void visit(ConstructorDeclarationNode* node) override {}
    void visit(EnumCaseNode* node) override {}
    void visit(MatchArmNode* node) override {}
    void visit(MatchPatternNode* node) override {}
    void visit(EnumPatternNode* node) override {}
    void visit(RangePatternNode* node) override {}
    void visit(ComparisonPatternNode* node) override {}
    void visit(WildcardPatternNode* node) override {}
    void visit(LiteralPatternNode* node) override {}
    
private:
    // Helper methods
    IRType convert_type(Type* type);
    ValueRef emit_type_conversion(ValueRef value, IRType target_type);
    void emit_function_prologue(FunctionDeclarationNode* node);
    void emit_function_epilogue();
};

} // namespace Myre