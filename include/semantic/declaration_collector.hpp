#pragma once

#include "ast/ast.hpp"
#include "symbol_table.hpp"
#include "type_system.hpp"
#include "type_definition.hpp"
#include <string>
#include <vector>

namespace Myre {

// Pass 1: Collect all declarations and build symbol table structure
class DeclarationCollector : public StructuralVisitor {
private:
    SymbolTable& symbolTable;
    TypeSystem& typeSystem;
    std::vector<std::string> errors;

    
public:
    DeclarationCollector(SymbolTable& symbol_table)
        : symbolTable(symbol_table)
        , typeSystem(symbol_table.get_type_system()) {}
    
    const std::vector<std::string>& get_errors() const { return errors; }
    
    // Entry point
    void collect(CompilationUnitNode* unit);
    
    private:
    // Visitor methods for declarations
    void visit(NamespaceDeclarationNode* node) override;
    void visit(UsingDirectiveNode* node) override;
    void visit(TypeDeclarationNode* node) override;
    void visit(FunctionDeclarationNode* node) override;
    void visit(VariableDeclarationNode* node) override;
    void visit(ForInStatementNode* node) override;
    void visit(IfStatementNode* node) override;

    // Pass through for nested visits
    void visit(CompilationUnitNode* node) override;
    void visit(BlockStatementNode* node) override;
};

} // namespace Myre