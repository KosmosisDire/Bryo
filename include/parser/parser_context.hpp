#pragma once

#include "common/token.hpp"
#include <string>
#include <vector>
#include <unordered_set>
#include <memory>

namespace Mycelium::Scripting::Parser {

// Diagnostic levels
enum class DiagnosticLevel {
    Error,
    Warning,
    Info,
    Hint
};

// Language features that can be enabled/disabled
enum class LanguageFeature {
    MatchExpressions,
    Properties,
    Constructors,
    Generics,
    Inheritance,
    Interfaces,
    OperatorOverloading,
    AsyncAwait,
    Nullable,
};

// Parsing context types for different language constructs
enum class ParsingContext {
    Global,              // Top-level declarations
    TypeDeclaration,     // Inside a type declaration
    InterfaceDeclaration,// Inside an interface declaration
    EnumDeclaration,     // Inside an enum declaration
    FunctionDeclaration, // Inside a function declaration
    PropertyDeclaration, // Inside a property declaration
    BlockStatement,      // Inside a block statement
    ExpressionContext,   // Parsing expressions
    ParameterList,       // Function parameter list
    ArgumentList,        // Function argument list
    TypeParameters,      // Generic type parameters
    WhenExpression,     // Inside a when expression
    IfStatement,         // Inside an if statement
    WhileLoop,           // Inside a while loop
    ForLoop,             // Inside a for loop
};

// Parser diagnostic with rich information
struct ParserDiagnostic {
    DiagnosticLevel level;
    std::string message;
    SourceLocation location;
    SourceRange range;
    std::vector<SourceRange> related_locations;
    std::vector<std::string> suggestions;
    std::string error_code;
    
    ParserDiagnostic(DiagnosticLevel lvl, std::string msg, SourceLocation loc)
        : level(lvl), message(std::move(msg)), location(loc), range(loc, 1) {}
        
    ParserDiagnostic(DiagnosticLevel lvl, std::string msg, SourceRange rng)
        : level(lvl), message(std::move(msg)), location(rng.start), range(rng) {}
    
    void add_suggestion(const std::string& suggestion) {
        suggestions.push_back(suggestion);
    }
    
    void add_related_location(SourceRange range) {
        related_locations.push_back(range);
    }
    
    void set_error_code(const std::string& code) {
        error_code = code;
    }
};

// Interface for receiving parser diagnostics
class ParserDiagnosticSink {
public:
    virtual ~ParserDiagnosticSink() = default;
    virtual void report_diagnostic(const ParserDiagnostic& diagnostic) = 0;
};

// Parser context manages state and diagnostics during parsing
class ParserContext {
public:
    // Constructor
    explicit ParserContext(std::string_view source_text,
                          ParserDiagnosticSink* diagnostic_sink = nullptr);
    
    // Destructor
    ~ParserContext() = default;
    
    // Non-copyable but movable
    ParserContext(const ParserContext&) = delete;
    ParserContext& operator=(const ParserContext&) = delete;
    ParserContext(ParserContext&&) = default;
    ParserContext& operator=(ParserContext&&) = default;
    
    // Error and diagnostic management
    void report_error(const std::string& message, SourceLocation location);
    void report_error(const std::string& message, SourceRange range);
    void report_warning(const std::string& message, SourceLocation location);
    void report_warning(const std::string& message, SourceRange range);
    void report_info(const std::string& message, SourceLocation location);
    void report_hint(const std::string& message, SourceLocation location);
    
    // Advanced diagnostic reporting
    ParserDiagnostic& report_diagnostic(DiagnosticLevel level, 
                                       const std::string& message, 
                                       SourceRange range);
    
    // Query diagnostic state
    bool has_errors() const;
    bool has_warnings() const;
    size_t error_count() const;
    size_t warning_count() const;
    const std::vector<ParserDiagnostic>& diagnostics() const { return diagnostics_; }
    
    // Clear diagnostics
    void clear_diagnostics();
    
    // Parsing context management
    void push_context(ParsingContext context);
    void pop_context();
    ParsingContext current_context() const;
    bool in_context(ParsingContext context) const;
    
    // Feature flag management
    void enable_feature(LanguageFeature feature);
    void disable_feature(LanguageFeature feature);
    bool is_feature_enabled(LanguageFeature feature) const;
    
    // Source text utilities
    std::string_view get_source_text() const { return source_text_; }
    std::string_view get_source_snippet(SourceRange range) const;
    std::string_view get_line_text(uint32_t line_number) const;
    SourceLocation offset_to_location(size_t offset) const;
    
    // Utility for creating source ranges
    SourceRange make_range(SourceLocation start, SourceLocation end) const;
    SourceRange make_range(SourceLocation location, uint32_t length) const;
    
    // Parser state queries
    bool in_type_context() const;
    bool in_function_context() const;
    bool in_expression_context() const;
    bool in_statement_context() const;
    
    // Error recovery hints
    void suggest_fix(const std::string& suggestion, SourceRange range);
    void suggest_insertion(const std::string& text, SourceLocation location);
    void suggest_replacement(const std::string& text, SourceRange range);
    void suggest_removal(SourceRange range);
    
    // Token-based diagnostics
    void report_unexpected_token(const Token& token, 
                                const std::string& expected = "");
    void report_missing_token(TokenKind expected, SourceLocation location);
    void report_extra_token(const Token& token);

private:
    std::string_view source_text_;
    ParserDiagnosticSink* diagnostic_sink_;
    std::vector<ParserDiagnostic> diagnostics_;
    std::vector<ParsingContext> context_stack_;
    std::unordered_set<LanguageFeature> enabled_features_;
    size_t error_count_;
    size_t warning_count_;
    
    // Helper methods
    void report_diagnostic_internal(ParserDiagnostic diagnostic);
    std::vector<size_t> calculate_line_offsets() const;
    mutable std::vector<size_t> line_offsets_; // Cached line offsets
    mutable bool line_offsets_computed_;
    
    void ensure_line_offsets_computed() const;
};

// Default diagnostic sink that outputs to stderr
class DefaultDiagnosticSink : public ParserDiagnosticSink {
public:
    void report_diagnostic(const ParserDiagnostic& diagnostic) override;
    
private:
    void print_diagnostic(const ParserDiagnostic& diagnostic);
    void print_source_snippet(const ParserDiagnostic& diagnostic, 
                             std::string_view source_text);
    const char* level_to_string(DiagnosticLevel level);
};

// Collecting diagnostic sink for testing
class CollectingDiagnosticSink : public ParserDiagnosticSink {
public:
    void report_diagnostic(const ParserDiagnostic& diagnostic) override {
        diagnostics.push_back(diagnostic);
    }
    
    void clear() { diagnostics.clear(); }
    
    bool has_errors() const {
        for (const auto& diag : diagnostics) {
            if (diag.level == DiagnosticLevel::Error) {
                return true;
            }
        }
        return false;
    }
    
    size_t error_count() const {
        size_t count = 0;
        for (const auto& diag : diagnostics) {
            if (diag.level == DiagnosticLevel::Error) {
                count++;
            }
        }
        return count;
    }
    
    std::vector<ParserDiagnostic> diagnostics;
};

// RAII helper for managing parsing contexts
class ContextGuard {
public:
    ContextGuard(ParserContext& context, ParsingContext new_context)
        : context_(context) {
        context_.push_context(new_context);
    }
    
    ~ContextGuard() {
        context_.pop_context();
    }
    
    ContextGuard(const ContextGuard&) = delete;
    ContextGuard& operator=(const ContextGuard&) = delete;
    
private:
    ParserContext& context_;
};

// Macro for easy context scoping
#define PARSER_CONTEXT_SCOPE(ctx, new_context) \
    ContextGuard _guard(ctx, new_context)

} // namespace Mycelium::Scripting::Parser