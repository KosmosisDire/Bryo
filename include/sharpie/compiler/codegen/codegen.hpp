#pragma once

#include "codegen_context.hpp"
#include "codegen_util.hpp"
#include "sharpie/script_ast.hpp"
#include <llvm/IR/Verifier.h>

#include <memory>

namespace llvm {
    class Value;
    class Function;
}

namespace Mycelium::Scripting::Lang::CodeGen {

/**
 * @class CodeGenerator
 * @brief Orchestrates the translation of an AST into LLVM IR.
 *
 * This class is instantiated once per compilation run. It holds the
 * CodeGenContext and contains all the methods for generating code for
 * different AST nodes. Its methods are organized by the type of AST node
 * they handle (declarations, statements, expressions).
 */
class CodeGenerator {
public:
    // The result of an expression code generation, including the LLVM value
    // and associated type information.
    struct ExpressionCGResult {
        llvm::Value* value = nullptr;
        const SymbolTable::ClassSymbol* classInfo = nullptr;
        llvm::Value* header_ptr = nullptr;
        PrimitiveStructInfo* primitive_info = nullptr;
        std::string resolved_path;
        bool is_static_type = false;

        ExpressionCGResult(llvm::Value* v = nullptr, const SymbolTable::ClassSymbol* ci = nullptr, llvm::Value* hp = nullptr)
            : value(v), classInfo(ci), header_ptr(hp) {}
    };

    // Constructor takes a reference to the context, which is owned by ScriptCompiler.
    explicit CodeGenerator(CodeGenContext& context);

    // The main entry point to start code generation for the entire compilation unit.
    void generate(std::shared_ptr<CompilationUnitNode> ast_root);

private:
    // --- Declaration Code Generation ---
    void cg_compilation_unit(std::shared_ptr<CompilationUnitNode> node);
    void cg_namespace_declaration(std::shared_ptr<NamespaceDeclarationNode> node, const std::string& parent_namespace);
    void cg_external_method_declaration(std::shared_ptr<ExternalMethodDeclarationNode> node);
    
    // Multi-pass class compilation methods
    void cg_declare_class_structure_and_signatures(std::shared_ptr<ClassDeclarationNode> node, const std::string& fq_class_name);
    void cg_compile_all_method_bodies(std::shared_ptr<ClassDeclarationNode> node, const std::string& fq_class_name);
    void cg_populate_vtable_for_class(const std::string& fq_class_name);
    void cg_generate_vtable_for_class(SymbolTable::ClassSymbol& class_symbol, const SymbolTable::ClassSymbol* class_symbol_const, const std::string& fq_class_name);
    
    // Method/Constructor/Destructor body compilation
    void cg_compile_method_body(std::shared_ptr<MethodDeclarationNode> node, const std::string& class_name);
    void cg_compile_constructor_body(std::shared_ptr<ConstructorDeclarationNode> node, const std::string& class_name);
    void cg_compile_destructor_body(std::shared_ptr<DestructorDeclarationNode> node, const std::string& class_name);
    
    // Method/Constructor/Destructor signature declaration
    llvm::Function* cg_declare_method_signature(std::shared_ptr<MethodDeclarationNode> node, const std::string& class_name);
    llvm::Function* cg_declare_constructor_signature(std::shared_ptr<ConstructorDeclarationNode> node, const std::string& class_name);
    llvm::Function* cg_declare_destructor_signature(std::shared_ptr<DestructorDeclarationNode> node, const std::string& class_name);

    // --- Statement Code Generation ---
    llvm::Value* cg_statement(std::shared_ptr<StatementNode> node);
    llvm::Value* cg_block_statement(std::shared_ptr<BlockStatementNode> node);
    llvm::Value* cg_local_variable_declaration_statement(std::shared_ptr<LocalVariableDeclarationStatementNode> node);
    llvm::Value* cg_expression_statement(std::shared_ptr<ExpressionStatementNode> node);
    llvm::Value* cg_if_statement(std::shared_ptr<IfStatementNode> node);
    llvm::Value* cg_while_statement(std::shared_ptr<WhileStatementNode> node);
    llvm::Value* cg_for_statement(std::shared_ptr<ForStatementNode> node);
    llvm::Value* cg_return_statement(std::shared_ptr<ReturnStatementNode> node);
    llvm::Value* cg_break_statement(std::shared_ptr<BreakStatementNode> node);
    llvm::Value* cg_continue_statement(std::shared_ptr<ContinueStatementNode> node);

    // --- Expression Code Generation ---
    ExpressionCGResult cg_expression(std::shared_ptr<ExpressionNode> node);
    ExpressionCGResult cg_literal_expression(std::shared_ptr<LiteralExpressionNode> node);
    ExpressionCGResult cg_identifier_expression(std::shared_ptr<IdentifierExpressionNode> node);
    ExpressionCGResult cg_binary_expression(std::shared_ptr<BinaryExpressionNode> node);
    ExpressionCGResult cg_assignment_expression(std::shared_ptr<AssignmentExpressionNode> node);
    ExpressionCGResult cg_unary_expression(std::shared_ptr<UnaryExpressionNode> node);
    ExpressionCGResult cg_method_call_expression(std::shared_ptr<MethodCallExpressionNode> node);
    ExpressionCGResult cg_object_creation_expression(std::shared_ptr<ObjectCreationExpressionNode> node);
    ExpressionCGResult cg_this_expression(std::shared_ptr<ThisExpressionNode> node);
    ExpressionCGResult cg_cast_expression(std::shared_ptr<CastExpressionNode> node);
    ExpressionCGResult cg_member_access_expression(std::shared_ptr<MemberAccessExpressionNode> node);
    ExpressionCGResult cg_parenthesized_expression(std::shared_ptr<ParenthesizedExpressionNode> node);
    ExpressionCGResult cg_primitive_method_call(std::shared_ptr<MethodCallExpressionNode> node, PrimitiveStructInfo* primitive_info, llvm::Value* instance_ptr);
    
    // The context is stored as a member reference.
    CodeGenContext& ctx;
};

} // namespace Mycelium::Scripting::Lang::CodeGen