#pragma once

#include "../script_ast.hpp"
#include "../compiler/class_type_info.hpp"
#include "../compiler/scope_manager.hpp"
#include "symbol_table.hpp"
#include <string>
#include <memory>
#include <map>
#include <vector>
#include <optional>
#include <set>

namespace Mycelium::Scripting::Lang
{

/**
 * Semantic error information
 */
struct SemanticError {
    std::string message;
    SourceLocation location;
    enum class Severity { Error, Warning, Info } severity;
    
    SemanticError(const std::string& msg, SourceLocation loc, Severity sev = Severity::Error)
        : message(msg), location(loc), severity(sev) {}
};

/**
 * Results of semantic analysis
 */
class SemanticAnalysisResult {
public:
    std::vector<SemanticError> errors;
    std::vector<SemanticError> warnings;
    bool has_errors() const { return !errors.empty(); }
    bool has_warnings() const { return !warnings.empty(); }
    
    void add_error(const std::string& message, SourceLocation location) {
        errors.emplace_back(message, location, SemanticError::Severity::Error);
    }
    
    void add_warning(const std::string& message, SourceLocation location) {
        warnings.emplace_back(message, location, SemanticError::Severity::Warning);
    }
    
    void merge(const SemanticAnalysisResult& other) {
        errors.insert(errors.end(), other.errors.begin(), other.errors.end());
        warnings.insert(warnings.end(), other.warnings.begin(), other.warnings.end());
    }
};

/**
 * Pure semantic analyzer - no IR generation, only semantic validation
 */
class SemanticAnalyzer {
private:
    std::unique_ptr<SymbolTable> symbol_table;
    SemanticAnalysisResult result;
    
    // Enhanced analysis context for detailed scope tracking
    std::string current_class_name;
    std::string current_method_name;
    std::string current_namespace_name;
    bool in_static_method = false;
    bool in_constructor = false;
    bool in_instance_method = false;
    std::vector<std::string> loop_stack; // Track nested loops for break/continue validation
    std::vector<std::string> scope_stack; // Track all scope names for detailed logging
    int current_scope_depth = 0;
    
    // Primitive type registry reference
    PrimitiveStructRegistry primitive_registry;
    
    // Method call dependency analysis results - part of SemanticIR
    struct MethodCallInfo {
        std::string caller_class;
        std::string caller_method;
        std::string callee_class;
        std::string callee_method;
        bool is_forward_call;  // true if this is a forward declaration call
        SourceLocation call_location;
        
        std::string get_caller_qualified_name() const {
            return caller_class + "." + caller_method;
        }
        
        std::string get_callee_qualified_name() const {
            return callee_class + "." + callee_method;
        }
    };
    
    std::vector<MethodCallInfo> discovered_method_calls;
    std::set<std::string> discovered_forward_calls; // Legacy - for compatibility
    
public:
    SemanticAnalyzer();
    ~SemanticAnalyzer();
    
    // Main analysis entry point
    SemanticAnalysisResult analyze(std::shared_ptr<CompilationUnitNode> ast_root);
    
    // Access to symbol table for ScriptCompiler
    const SymbolTable& get_symbol_table() const { return *symbol_table; }
    SymbolTable& get_symbol_table() { return *symbol_table; }
    
private:
    // Enhanced multi-pass declaration processing
    void collect_class_declarations(std::shared_ptr<CompilationUnitNode> node);
    void collect_external_declarations(std::shared_ptr<CompilationUnitNode> node);
    void collect_method_signatures(std::shared_ptr<CompilationUnitNode> node);
    void resolve_forward_references();
    
    // Forward declaration specific methods
    void collect_class_signatures(std::shared_ptr<ClassDeclarationNode> node);
    void collect_method_signature(std::shared_ptr<MethodDeclarationNode> node, const std::string& class_name);
    void collect_constructor_signature(std::shared_ptr<ConstructorDeclarationNode> node, const std::string& class_name);
    void collect_destructor_signature(std::shared_ptr<DestructorDeclarationNode> node, const std::string& class_name);
    void collect_class_structure(std::shared_ptr<ClassDeclarationNode> node);
    void analyze_class_method_dependencies(const std::string& class_name, 
                                           std::map<std::string, std::vector<std::string>>& dependency_graph,
                                           std::set<std::string>& forward_declared_calls);
    void validate_forward_declared_calls(const std::set<std::string>& forward_declared_calls);
    
    // Legacy declaration processing visitors (being migrated)
    void analyze_declarations(std::shared_ptr<AstNode> node);
    void analyze_declarations(std::shared_ptr<CompilationUnitNode> node);
    void analyze_declarations(std::shared_ptr<NamespaceDeclarationNode> node);
    void analyze_declarations(std::shared_ptr<ClassDeclarationNode> node);
    void analyze_declarations(std::shared_ptr<MethodDeclarationNode> node, const std::string& class_name);
    void analyze_declarations(std::shared_ptr<ConstructorDeclarationNode> node, const std::string& class_name);
    void analyze_declarations(std::shared_ptr<DestructorDeclarationNode> node, const std::string& class_name);
    void analyze_declarations(std::shared_ptr<ExternalMethodDeclarationNode> node);
    
    // Type checking and validation visitors
    void analyze_semantics(std::shared_ptr<AstNode> node);
    void analyze_semantics(std::shared_ptr<CompilationUnitNode> node);
    void analyze_semantics(std::shared_ptr<NamespaceDeclarationNode> node);
    void analyze_semantics(std::shared_ptr<ClassDeclarationNode> node);
    void analyze_semantics(std::shared_ptr<MethodDeclarationNode> node, const std::string& class_name);
    void analyze_semantics(std::shared_ptr<ConstructorDeclarationNode> node, const std::string& class_name);
    void analyze_semantics(std::shared_ptr<DestructorDeclarationNode> node, const std::string& class_name);
    
    // Statement analysis
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
    
    // Expression analysis
    struct ExpressionTypeInfo {
        std::shared_ptr<TypeNameNode> type;
        const ClassTypeInfo* class_info = nullptr;
        bool is_lvalue = false;
        
        ExpressionTypeInfo(std::shared_ptr<TypeNameNode> t = nullptr) : type(t) {}
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
    
    // Type checking utilities
    bool are_types_compatible(std::shared_ptr<TypeNameNode> left, std::shared_ptr<TypeNameNode> right);
    bool is_primitive_type(const std::string& type_name);
    bool is_numeric_type(std::shared_ptr<TypeNameNode> type);
    bool is_string_type(std::shared_ptr<TypeNameNode> type);
    bool is_bool_type(std::shared_ptr<TypeNameNode> type);
    std::shared_ptr<TypeNameNode> create_primitive_type(const std::string& type_name);
    std::shared_ptr<TypeNameNode> promote_numeric_types(std::shared_ptr<TypeNameNode> left, std::shared_ptr<TypeNameNode> right);
    
    // Error reporting
    void add_error(const std::string& message, SourceLocation location);
    void add_warning(const std::string& message, SourceLocation location);
    void add_error(const std::string& message, std::optional<SourceLocation> location = std::nullopt);
    void add_warning(const std::string& message, std::optional<SourceLocation> location = std::nullopt);
    
    // Enhanced logging for semantic information
    void log_semantic_ir_summary();
    void log_forward_declarations();
    void log_class_registry();
    void log_method_registry();
    void log_scope_information();
    
    // Enhanced scope tracking
    void push_semantic_scope(const std::string& scope_name);
    void pop_semantic_scope();
    std::string get_full_scope_path();
    void log_scope_change(const std::string& action, const std::string& scope_name);
    
    // UML diagram generation
    void generate_uml_diagram_output();
};

} // namespace Mycelium::Scripting::Lang