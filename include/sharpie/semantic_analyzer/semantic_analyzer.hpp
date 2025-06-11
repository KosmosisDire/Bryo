#pragma once

#include "../script_ast.hpp"
#include "../compiler/class_type_info.hpp"
#include "../compiler/scope_manager.hpp"
#include "semantic_ir.hpp" // Use the new SemanticIR
#include "semantic_context.hpp"
#include <string>
#include <memory>
#include <map>
#include <vector>
#include <optional>
#include <set>

namespace Mycelium::Scripting::Lang
{

    /**
     * Pure semantic analyzer - no IR generation, only semantic validation
     */
    class SemanticAnalyzer
    {
    private:
        std::unique_ptr<SemanticIR> ir; // The analyzer now builds a SemanticIR
        std::unique_ptr<SemanticContext> context;
        PrimitiveStructRegistry primitiveRegistry;

    public:
        SemanticAnalyzer();
        ~SemanticAnalyzer();

        // Main analysis entry point now returns a complete SemanticIR
        std::unique_ptr<SemanticIR> analyze(std::shared_ptr<CompilationUnitNode> ast_root);

    private:
        // Helper to record a symbol usage from any analysis pass
        void record_usage(
            const std::string &symbol_id,
            UsageKind kind,
            std::optional<SourceLocation> location);

        // Pass 1 & 2: Declaration collection (in declaration_pass.cpp)
        void collect_class_declarations(std::shared_ptr<CompilationUnitNode> node);
        void collect_external_declarations(std::shared_ptr<CompilationUnitNode> node);
        void collect_method_signatures(std::shared_ptr<CompilationUnitNode> node);
        void collect_class_signatures(std::shared_ptr<ClassDeclarationNode> node);
        void collect_method_signature(std::shared_ptr<MethodDeclarationNode> node, const std::string &class_name);
        void collect_constructor_signature(std::shared_ptr<ConstructorDeclarationNode> node, const std::string &class_name);
        void collect_destructor_signature(std::shared_ptr<DestructorDeclarationNode> node, const std::string &class_name);
        void collect_class_structure(std::shared_ptr<ClassDeclarationNode> node);

        // Legacy declaration analysis (in declaration_pass.cpp)
        void analyze_declarations(std::shared_ptr<AstNode> node);
        void analyze_declarations(std::shared_ptr<CompilationUnitNode> node);
        void analyze_declarations(std::shared_ptr<NamespaceDeclarationNode> node);
        void analyze_declarations(std::shared_ptr<ClassDeclarationNode> node);
        void analyze_declarations(std::shared_ptr<ExternalMethodDeclarationNode> node);

        // Pass 3: Type Checking and Usage Collection (in usage_collection_pass.cpp)
        void collect_usages_and_type_check(std::shared_ptr<AstNode> node);
        void collect_usages_and_type_check(std::shared_ptr<CompilationUnitNode> node);
        void collect_usages_and_type_check(std::shared_ptr<NamespaceDeclarationNode> node);
        void collect_usages_and_type_check(std::shared_ptr<ClassDeclarationNode> node);
        void collect_usages_and_type_check(std::shared_ptr<MethodDeclarationNode> node, const std::string &class_name);
        void collect_usages_and_type_check(std::shared_ptr<ConstructorDeclarationNode> node, const std::string &class_name);
        void collect_usages_and_type_check(std::shared_ptr<DestructorDeclarationNode> node, const std::string &class_name);

        // Statement analysis (in usage_collection_pass.cpp)
        void analyze_statement(std::shared_ptr<StatementNode> node);
        void analyze_statement(std::shared_ptr<BlockStatementNode> node);
        void analyze_statement(std::shared_ptr<LocalVariableDeclarationStatementNode> node);
        void analyze_statement(std::shared_ptr<ExpressionStatementNode> node);
        void analyze_statement(std::shared_ptr<IfStatementNode> node);
        void analyze_statement(std::shared_ptr<WhileStatementNode> node);
        void analyze_statement(std::shared_ptr<ForStatementNode> node);
        void analyze_statement(std::shared_ptr<ReturnStatementNode> node);
        void analyze_statement(std::shared_ptr<BreakStatementNode> node);
        void analyze_statement(std::shared_ptr<ContinueStatementNode> node);

        // Expression analysis (in usage_collection_pass.cpp)
        struct ExpressionTypeInfo
        {
            std::shared_ptr<TypeNameNode> type;
            const ClassTypeInfo *class_info = nullptr;
            bool is_lvalue = false;
            std::string namespace_path; // Used to track namespace resolution

            ExpressionTypeInfo(std::shared_ptr<TypeNameNode> t = nullptr) : type(t) {}
            ExpressionTypeInfo(std::shared_ptr<TypeNameNode> t, const ClassTypeInfo *ci) : type(t), class_info(ci) {}

            ExpressionTypeInfo(std::shared_ptr<TypeNameNode> t, const ClassTypeInfo *ci, bool lval)
                : type(t), class_info(ci), is_lvalue(lval) {}
        };

        ExpressionTypeInfo analyze_expression(std::shared_ptr<ExpressionNode> node);
        ExpressionTypeInfo analyze_expression(std::shared_ptr<LiteralExpressionNode> node);
        ExpressionTypeInfo analyze_expression(std::shared_ptr<IdentifierExpressionNode> node);
        ExpressionTypeInfo analyze_expression(std::shared_ptr<BinaryExpressionNode> node);
        ExpressionTypeInfo analyze_expression(std::shared_ptr<AssignmentExpressionNode> node);
        ExpressionTypeInfo analyze_expression(std::shared_ptr<UnaryExpressionNode> node);
        ExpressionTypeInfo analyze_expression(std::shared_ptr<MethodCallExpressionNode> node);
        ExpressionTypeInfo analyze_expression(std::shared_ptr<ObjectCreationExpressionNode> node);
        ExpressionTypeInfo analyze_expression(std::shared_ptr<ThisExpressionNode> node);
        ExpressionTypeInfo analyze_expression(std::shared_ptr<CastExpressionNode> node);
        ExpressionTypeInfo analyze_expression(std::shared_ptr<MemberAccessExpressionNode> node);
        ExpressionTypeInfo analyze_expression(std::shared_ptr<ParenthesizedExpressionNode> node);

        // Type checking utilities (in usage_collection_pass.cpp)
        bool are_types_compatible(std::shared_ptr<TypeNameNode> left, std::shared_ptr<TypeNameNode> right);
        bool is_primitive_type(const std::string &type_name);
        bool is_numeric_type(std::shared_ptr<TypeNameNode> type);
        bool is_string_type(std::shared_ptr<TypeNameNode> type);
        bool is_bool_type(std::shared_ptr<TypeNameNode> type);
        std::shared_ptr<TypeNameNode> create_primitive_type(const std::string &type_name);
        std::shared_ptr<TypeNameNode> promote_numeric_types(std::shared_ptr<TypeNameNode> left, std::shared_ptr<TypeNameNode> right);

        // Error reporting (in semantic_analyzer.cpp)
        void add_error(const std::string &message, std::optional<SourceLocation> location = std::nullopt);
        void add_warning(const std::string &message, std::optional<SourceLocation> location = std::nullopt);

        // Logging (in semantic_logging.cpp)
        void log_semantic_ir_summary();
        void log_forward_declarations();
        void log_class_registry();
        void log_method_registry();
        void log_scope_information();

        // Scope tracking (in semantic_analyzer.cpp)
        void push_semantic_scope(const std::string &scope_name);
        void pop_semantic_scope();
        void log_scope_change(const std::string &action, const std::string &scope_name);
    };

} // namespace Mycelium::Scripting::Lang