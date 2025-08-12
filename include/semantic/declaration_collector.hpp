#pragma once

#include "ast/ast.hpp"
#include "symbol_table.hpp"
#include <vector>
#include <string>

namespace Myre {

class  DeclarationCollector : public StructuralVisitor {
private:
    SymbolTable& symbolTable;
    TypeSystem& typeSystem;
    std::vector<std::string> errors;
    
    // Helper to get current scope as Scope* (not ScopeNode*)
    SymbolHandle get_current_handle();
    
    // Helper to visit block contents without creating a new scope
    void visit_block_contents(BlockStatementNode* node);
    
public:
    DeclarationCollector(SymbolTable& st) 
        : symbolTable(st)
        , typeSystem(st.get_type_system()) {}
    
    // Main entry point
    void collect(CompilationUnitNode* unit);
    
    // Get collected errors
    const std::vector<std::string>& get_errors() const { return errors; }
    
    // === Visitor methods for ALL node types ===
    
    // Root and base nodes
    void visit(CompilationUnitNode* node) override;
    void visit(ErrorNode* node) override;
    void visit(TokenNode* node) override;
    void visit(IdentifierNode* node) override;
    
    // Declarations
    void visit(NamespaceDeclarationNode* node) override;
    void visit(UsingDirectiveNode* node) override;
    void visit(TypeDeclarationNode* node) override;
    void visit(InterfaceDeclarationNode* node) override;
    void visit(EnumDeclarationNode* node) override;
    void visit(EnumCaseNode* node) override;
    void visit(FunctionDeclarationNode* node) override;
    void visit(ParameterNode* node) override;
    void visit(GenericParameterNode* node) override;
    void visit(VariableDeclarationNode* node) override;
    void visit(PropertyDeclarationNode* node) override;
    void visit(PropertyAccessorNode* node) override;
    void visit(ConstructorDeclarationNode* node) override;
    
    // Type names
    void visit(TypeNameNode* node) override;
    void visit(ArrayTypeNameNode* node) override;
    void visit(GenericTypeNameNode* node) override;
    void visit(QualifiedNameNode* node) override;
    
    // Statements
    void visit(BlockStatementNode* node) override;
    void visit(ExpressionStatementNode* node) override;
    void visit(IfStatementNode* node) override;
    void visit(WhileStatementNode* node) override;
    void visit(ForStatementNode* node) override;
    void visit(ForInStatementNode* node) override;
    void visit(ReturnStatementNode* node) override;
    void visit(BreakStatementNode* node) override;
    void visit(ContinueStatementNode* node) override;
    void visit(EmptyStatementNode* node) override;
    
    // Expressions
    void visit(BinaryExpressionNode* node) override;
    void visit(UnaryExpressionNode* node) override;
    void visit(AssignmentExpressionNode* node) override;
    void visit(CallExpressionNode* node) override;
    void visit(MemberAccessExpressionNode* node) override;
    void visit(IdentifierExpressionNode* node) override;
    void visit(LiteralExpressionNode* node) override;
    void visit(ParenthesizedExpressionNode* node) override;
    void visit(NewExpressionNode* node) override;
    void visit(ThisExpressionNode* node) override;
    void visit(CastExpressionNode* node) override;
    void visit(IndexerExpressionNode* node) override;
    void visit(TypeOfExpressionNode* node) override;
    void visit(SizeOfExpressionNode* node) override;
    void visit(ConditionalExpressionNode* node) override;
    void visit(RangeExpressionNode* node) override;
    void visit(FieldKeywordExpressionNode* node) override;
    void visit(ValueKeywordExpressionNode* node) override;
    
    // Match expressions and patterns
    void visit(MatchExpressionNode* node) override;
    void visit(MatchArmNode* node) override;
    void visit(MatchPatternNode* node) override;
    void visit(EnumPatternNode* node) override;
    void visit(RangePatternNode* node) override;
    void visit(ComparisonPatternNode* node) override;
    void visit(WildcardPatternNode* node) override;
    void visit(LiteralPatternNode* node) override;
    
    // Default handlers for abstract base types
    void visit(ExpressionNode* node) override;
    void visit(StatementNode* node) override;
    void visit(DeclarationNode* node) override;
};

} // namespace Myre