#pragma once

#include "ast/ast.hpp"
#include "symbol_table.hpp"
#include <vector>
#include <string>

namespace Myre {

// Forward declarations
class SymbolTable;
class TypeSystem;

class SymbolTableBuilder : public DefaultVisitor {
private:
    SymbolTable& symbolTable;
    TypeSystem& typeSystem;
    std::vector<std::string> errors;
    
    // === Helper Methods ===
    
    // Get current scope handle
    SymbolHandle get_current_handle();
    
    // Annotate node with current scope
    void annotate_scope(Node* node);
    
    // Visit block contents without creating a new scope
    void visit_block_contents(Block* block);
    
    // Convert identifier path to string
    std::string path_to_string(List<Identifier*> path);
    
    // Build type from TypeRef
    TypePtr resolve_type(TypeRef* typeRef);
    
    // Helper for property accessors
    void visit_property_accessor(PropertyAccessor* accessor, TypePtr propType);
    
public:
    // Constructor
    explicit SymbolTableBuilder(SymbolTable& st);
    
    // Main entry point
    void collect(CompilationUnit* unit);
    
    // Get collected errors
    const std::vector<std::string>& get_errors() const { return errors; }
    bool has_errors() const { return !errors.empty(); }
    
    // === Visitor Overrides ===
    
    // Base node - annotates all nodes with scope
    void visit(Node* node) override;
    
    // Root
    void visit(CompilationUnit* node) override;
    
    // === Declarations - These create symbols ===
    void visit(NamespaceDecl* node) override;
    void visit(TypeDecl* node) override;
    void visit(FunctionDecl* node) override;
    void visit(VariableDecl* node) override;
    void visit(MemberVariableDecl* node) override;
    void visit(EnumCaseDecl* node) override;
    
    // === Statements with scopes ===
    void visit(Block* node) override;
    void visit(WhileStmt* node) override;
    void visit(ForStmt* node) override;
    void visit(ForInStmt* node) override;
    void visit(IfExpr* node) override;
    
    // Note: Other visit methods from DefaultVisitor are inherited
    // and will automatically traverse children while annotating scopes
};

} // namespace Myre