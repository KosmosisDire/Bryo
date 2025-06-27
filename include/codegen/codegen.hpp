#pragma once

#include "ast/ast.hpp"
#include "semantic/symbol_table.hpp"
#include "codegen/ir_builder.hpp"
#include <memory>
#include <unordered_map>

namespace codegen {

class CommandProcessor;

class CodeGenerator : public Mycelium::Scripting::Lang::StructuralVisitor {
private:
    semantic::SymbolTable& symbol_table_;
    std::unique_ptr<IRBuilder> ir_builder_;
    std::unordered_map<std::string, ValueRef> local_vars_;
    ValueRef current_value_;  // Result of last expression

public:
    CodeGenerator(semantic::SymbolTable& table);
    ~CodeGenerator() = default;

    // Basic visitor overrides for initial implementation
    void visit(Mycelium::Scripting::Lang::CompilationUnitNode* node) override;
    void visit(Mycelium::Scripting::Lang::LiteralExpressionNode* node) override;
    void visit(Mycelium::Scripting::Lang::BinaryExpressionNode* node) override;
    void visit(Mycelium::Scripting::Lang::LocalVariableDeclarationNode* node) override;
    void visit(Mycelium::Scripting::Lang::IdentifierExpressionNode* node) override;
    void visit(Mycelium::Scripting::Lang::ReturnStatementNode* node) override;
    void visit(Mycelium::Scripting::Lang::FunctionDeclarationNode* node) override;
    void visit(Mycelium::Scripting::Lang::BlockStatementNode* node) override;

    // Generate code from AST
    void generate_code(Mycelium::Scripting::Lang::CompilationUnitNode* root);
};

} // namespace codegen