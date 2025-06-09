#include "sharpie/semantic_analyzer/semantic_analyzer.hpp"
#include "sharpie/semantic_analyzer/uml_generator.hpp" // Include the new generator
#include "sharpie/common/logger.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>

using namespace Mycelium::Scripting::Common; // For Logger macros

namespace Mycelium::Scripting::Lang
{

// ============================================================================
// SemanticAnalyzer Implementation
// ============================================================================

SemanticAnalyzer::SemanticAnalyzer() 
    : symbolTable(std::make_unique<SymbolTable>()),
      context(std::make_unique<SemanticContext>()) {
    // Initialize primitive registry
    // This will contain built-in types like int, bool, string, etc.
}

SemanticAnalyzer::~SemanticAnalyzer() = default;

SemanticAnalysisResult SemanticAnalyzer::analyze(std::shared_ptr<CompilationUnitNode> ast_root) {
    analysisResult = SemanticAnalysisResult{}; // Reset results
    context->reset(); // Reset context for a fresh analysis run
    discoveredMethodCalls.clear(); // Also clear discovered calls
    discoveredForwardCalls.clear();
    
    if (!ast_root) {
        add_error("Cannot analyze null AST root");
        return analysisResult;
    }
    
    LOG_INFO("Starting enhanced semantic analysis with comprehensive forward declaration resolution", "COMPILER");
    
    // Enhanced multi-pass analysis for comprehensive forward declaration handling:
    
    // Pass 1: Collect all class and external declarations (structure building)
    LOG_INFO("Pass 1: Class and external declaration collection", "COMPILER");
    collect_class_declarations(ast_root);
    collect_external_declarations(ast_root);
    
    // Pass 2: Collect all method/constructor/destructor signatures within classes
    LOG_INFO("Pass 2: Method signature collection (forward declarations)", "COMPILER");
    collect_method_signatures(ast_root);
    
    // Pass 3: Resolve forward references and validate signatures
    LOG_INFO("Pass 3: Forward reference resolution", "COMPILER");
    resolve_forward_references();
    
    // Pass 4: Type checking and semantic validation with full context
    LOG_INFO("Pass 4: Type checking and semantic validation", "COMPILER");
    analyze_semantics(ast_root);
    
    // Pass 5: Reporting and output generation
    LOG_INFO("Pass 5: Generating analysis reports", "COMPILER");
    log_semantic_ir_summary();
    
    // Use the decoupled UML Generator
    UmlGenerator uml_generator;
    uml_generator.generate(*symbolTable, discoveredMethodCalls);

    // Final summary
    LOG_INFO("Enhanced semantic analysis complete. Errors: " + std::to_string(analysisResult.errors.size()) + 
             ", Warnings: " + std::to_string(analysisResult.warnings.size()), "COMPILER");
    
    // Log forward declaration status
    if (symbolTable->has_unresolved_forward_declarations()) {
        LOG_WARN("Unresolved forward declarations detected", "COMPILER");
        log_forward_declarations();
    } else {
        LOG_INFO("All forward declarations resolved successfully", "COMPILER");
    }
    
    return analysisResult;
}

// ============================================================================
// Error Reporting
// ============================================================================

void SemanticAnalyzer::add_error(const std::string& message, SourceLocation location) {
    analysisResult.add_error(message, location);
    LOG_ERROR("Semantic error: " + message + " at " + std::to_string(location.lineStart) + ":" + std::to_string(location.columnStart), "COMPILER");
}

void SemanticAnalyzer::add_warning(const std::string& message, SourceLocation location) {
    analysisResult.add_warning(message, location);
    LOG_WARN("Semantic warning: " + message + " at " + std::to_string(location.lineStart) + ":" + std::to_string(location.columnStart), "COMPILER");
}

void SemanticAnalyzer::add_error(const std::string& message, std::optional<SourceLocation> location) {
    if (location.has_value()) {
        add_error(message, location.value());
    } else {
        analysisResult.add_error(message, SourceLocation{0, 0}); // Default location
        LOG_ERROR("Semantic error: " + message, "COMPILER");
    }
}

void SemanticAnalyzer::add_warning(const std::string& message, std::optional<SourceLocation> location) {
    if (location.has_value()) {
        add_warning(message, location.value());
    } else {
        analysisResult.add_warning(message, SourceLocation{0, 0}); // Default location
        LOG_WARN("Semantic warning: " + message, "COMPILER");
    }
}

// ============================================================================
// Enhanced Scope Tracking Methods
// ============================================================================

void SemanticAnalyzer::push_semantic_scope(const std::string& scope_name) {
    context->scopeStack.push_back(scope_name);
    context->currentScopeDepth++;
    symbolTable->push_scope();
    log_scope_change("ENTER", scope_name);
}

void SemanticAnalyzer::pop_semantic_scope() {
    if (!context->scopeStack.empty()) {
        std::string scope_name = context->scopeStack.back();
        context->scopeStack.pop_back();
        context->currentScopeDepth--;
        symbolTable->pop_scope();
        log_scope_change("EXIT", scope_name);
    }
}

void SemanticAnalyzer::log_scope_change(const std::string& action, const std::string& scope_name) {
    LOG_INFO("SCOPE " + action + ": " + scope_name + " (depth: " + std::to_string(context->currentScopeDepth) + 
             ", full path: " + context->getFullScopePath() + ")", "COMPILER");
}


} // namespace Mycelium::Scripting::Lang