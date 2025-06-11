#include "sharpie/semantic_analyzer/semantic_analyzer.hpp"
#include "sharpie/semantic_analyzer/uml_generator.hpp"
#include "sharpie/common/logger.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>

using namespace Mycelium::Scripting::Common;

namespace Mycelium::Scripting::Lang
{

    // ============================================================================
    // SemanticAnalyzer Implementation
    // ============================================================================

    SemanticAnalyzer::SemanticAnalyzer()
        : context(std::make_unique<SemanticContext>())
    {
        // Constructor is now simpler as the IR is created per-analysis.
    }

    SemanticAnalyzer::~SemanticAnalyzer() = default;

    std::unique_ptr<SemanticIR> SemanticAnalyzer::analyze(std::shared_ptr<CompilationUnitNode> ast_root)
    {
        ir = std::make_unique<SemanticIR>(); // Create a fresh Semantic IR for this run
        context->reset();                    // Reset context for a fresh analysis run

        if (!ast_root)
        {
            add_error("Cannot analyze null AST root");
            return std::move(ir);
        }

        LOG_INFO("Starting semantic analysis, building Semantic IR", "SEMANTIC");

        // Pass 1: Collect all class and external declarations (populates ir->symbol_table)
        LOG_INFO("Pass 1: Declaration Collection", "SEMANTIC");
        collect_class_declarations(ast_root);
        collect_external_declarations(ast_root);

        // Pass 2: Collect all method/constructor/destructor signatures within classes (populates ir->symbol_table)
        LOG_INFO("Pass 2: Method Signature Collection", "SEMANTIC");
        collect_method_signatures(ast_root);

        // Pass 3: Type checking and usage collection (populates ir->usage_graph and ir->errors)
        LOG_INFO("Pass 3: Type Checking and Usage Collection", "SEMANTIC");
        collect_usages_and_type_check(ast_root);

        // Pass 4: Finalize results and generate reports
        LOG_INFO("Pass 4: Finalizing Semantic IR and generating reports", "SEMANTIC");

        // Generate UML from the completed IR
        UmlGenerator uml_generator;
        uml_generator.generate(*ir);

        // Final summary
        LOG_INFO("Semantic analysis complete. Errors: " + std::to_string(ir->errors.size()) +
                     ", Warnings: " + std::to_string(ir->warnings.size()),
                 "SEMANTIC");

        // Log detailed summary from the IR if needed
        log_semantic_ir_summary();

        if (ir->symbol_table.has_unresolved_forward_declarations())
        {
            LOG_WARN("Unresolved forward declarations detected", "SEMANTIC");
            log_forward_declarations();
        }
        else
        {
            LOG_INFO("All forward declarations resolved successfully", "SEMANTIC");
        }

        return std::move(ir);
    }

    // ============================================================================
    // Usage Recording
    // ============================================================================

    void SemanticAnalyzer::record_usage(const std::string &symbol_id, UsageKind kind, std::optional<SourceLocation> location)
    {
        if (symbol_id.empty() || !location.has_value())
        {
            return;
        }

        SymbolUsage usage;
        usage.qualified_symbol_id = symbol_id;
        usage.kind = kind;
        usage.location = location.value();
        usage.context_scope_path = context->getFullScopePath();
        usage.context_class_name = context->currentClassName;
        usage.context_function_name = context->currentMethodName;

        ir->add_usage(usage);

        // Example of new, detailed logging for usage tracking.
        std::string usage_kind_str;
        switch (kind)
        {
        case UsageKind::Read:
            usage_kind_str = "Read";
            break;
        case UsageKind::Write:
            usage_kind_str = "Write";
            break;
        case UsageKind::Call:
            usage_kind_str = "Call";
            break;
        case UsageKind::Instantiation:
            usage_kind_str = "Instantiation";
            break;
        case UsageKind::TypeReference:
            usage_kind_str = "TypeReference";
            break;
        case UsageKind::Inheritance:
            usage_kind_str = "Inheritance";
            break;
        }

        LOG_DEBUG("Recorded usage: '" + symbol_id + "' (Kind: " + usage_kind_str +
                      ") in scope '" + usage.context_scope_path + "'",
                  "SEMANTIC_IR");
    }

    // ============================================================================
    // Error Reporting
    // ============================================================================

    void SemanticAnalyzer::add_error(const std::string &message, std::optional<SourceLocation> location)
    {
        SourceLocation loc = location.value_or(SourceLocation{0, 0});
        ir->add_error(message, loc);
        LOG_ERROR("Semantic error: " + message + " at " + std::to_string(loc.lineStart) + ":" + std::to_string(loc.columnStart), "SEMANTIC");
    }

    void SemanticAnalyzer::add_warning(const std::string &message, std::optional<SourceLocation> location)
    {
        SourceLocation loc = location.value_or(SourceLocation{0, 0});
        ir->add_warning(message, loc);
        LOG_WARN("Semantic warning: " + message + " at " + std::to_string(loc.lineStart) + ":" + std::to_string(loc.columnStart), "SEMANTIC");
    }

    // ============================================================================
    // Enhanced Scope Tracking Methods
    // ============================================================================

    void SemanticAnalyzer::push_semantic_scope(const std::string &scope_name)
    {
        context->scopeStack.push_back(scope_name);
        context->currentScopeDepth++;
        ir->symbol_table.push_scope();
        log_scope_change("ENTER", scope_name);
    }

    void SemanticAnalyzer::pop_semantic_scope()
    {
        if (!context->scopeStack.empty())
        {
            std::string scope_name = context->scopeStack.back();
            context->scopeStack.pop_back();
            context->currentScopeDepth--;
            ir->symbol_table.pop_scope();
            log_scope_change("EXIT", scope_name);
        }
    }

    void SemanticAnalyzer::log_scope_change(const std::string &action, const std::string &scope_name)
    {
        LOG_INFO("SCOPE " + action + ": " + scope_name + " (depth: " + std::to_string(context->currentScopeDepth) +
                     ", full path: " + context->getFullScopePath() + ")",
                 "SEMANTIC");
    }

} // namespace Mycelium::Scripting::Lang