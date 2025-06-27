#pragma once

#include "ast/ast.hpp"
#include "semantic/symbol_table.hpp"
#include "codegen/ir_builder.hpp"
#include <memory>
#include <unordered_map>

namespace Mycelium::Scripting::Lang {

class CommandProcessor;


class CodeGenerator : public StructuralVisitor {
private:
    SymbolTable& symbol_table_;
    std::unique_ptr<IRBuilder> ir_builder_;
    std::unordered_map<std::string, ValueRef> local_vars_;
    ValueRef current_value_;  // Result of last expression

public:
    CodeGenerator(SymbolTable& table);
    ~CodeGenerator() = default;

    // Basic visitor overrides for initial implementation
    void visit(CompilationUnitNode* node) override;
    void visit(LiteralExpressionNode* node) override;
    void visit(BinaryExpressionNode* node) override;
    void visit(LocalVariableDeclarationNode* node) override;
    void visit(IdentifierExpressionNode* node) override;
    void visit(ReturnStatementNode* node) override;
    void visit(FunctionDeclarationNode* node) override;
    void visit(BlockStatementNode* node) override;

    // Generate code from AST
    void generate_code(CompilationUnitNode* root);
};

} // namespace Mycelium::Scripting::Lang