#include "parser/parser_context.hpp"
#include <algorithm>
#include <iostream>
#include <iomanip>

namespace Mycelium::Scripting::Parser {

ParserContext::ParserContext(std::string_view source_text, ParserDiagnosticSink* diagnostic_sink)
    : source_text_(source_text)
    , diagnostic_sink_(diagnostic_sink)
    , error_count_(0)
    , warning_count_(0)
    , line_offsets_computed_(false)
{
    context_stack_.push_back(ParsingContext::Global);
    
    // Enable all features by default for now
    enabled_features_.insert(LanguageFeature::MatchExpressions);
    enabled_features_.insert(LanguageFeature::Properties);
    enabled_features_.insert(LanguageFeature::Constructors);
    enabled_features_.insert(LanguageFeature::Generics);
    enabled_features_.insert(LanguageFeature::Inheritance);
    enabled_features_.insert(LanguageFeature::Interfaces);
}

void ParserContext::report_error(const std::string& message, SourceLocation location) {
    report_diagnostic_internal(ParserDiagnostic(DiagnosticLevel::Error, message, location));
}

void ParserContext::report_error(const std::string& message, SourceRange range) {
    report_diagnostic_internal(ParserDiagnostic(DiagnosticLevel::Error, message, range));
}

void ParserContext::report_warning(const std::string& message, SourceLocation location) {
    report_diagnostic_internal(ParserDiagnostic(DiagnosticLevel::Warning, message, location));
}

void ParserContext::report_warning(const std::string& message, SourceRange range) {
    report_diagnostic_internal(ParserDiagnostic(DiagnosticLevel::Warning, message, range));
}

void ParserContext::report_info(const std::string& message, SourceLocation location) {
    report_diagnostic_internal(ParserDiagnostic(DiagnosticLevel::Info, message, location));
}

void ParserContext::report_hint(const std::string& message, SourceLocation location) {
    report_diagnostic_internal(ParserDiagnostic(DiagnosticLevel::Hint, message, location));
}

ParserDiagnostic& ParserContext::report_diagnostic(DiagnosticLevel level, 
                                                  const std::string& message, 
                                                  SourceRange range) {
    diagnostics_.emplace_back(level, message, range);
    ParserDiagnostic& diagnostic = diagnostics_.back();
    
    if (level == DiagnosticLevel::Error) {
        error_count_++;
    } else if (level == DiagnosticLevel::Warning) {
        warning_count_++;
    }
    
    if (diagnostic_sink_) {
        diagnostic_sink_->report_diagnostic(diagnostic);
    }
    
    return diagnostic;
}

bool ParserContext::has_errors() const {
    return error_count_ > 0;
}

bool ParserContext::has_warnings() const {
    return warning_count_ > 0;
}

size_t ParserContext::error_count() const {
    return error_count_;
}

size_t ParserContext::warning_count() const {
    return warning_count_;
}

void ParserContext::clear_diagnostics() {
    diagnostics_.clear();
    error_count_ = 0;
    warning_count_ = 0;
}

void ParserContext::push_context(ParsingContext context) {
    context_stack_.push_back(context);
}

void ParserContext::pop_context() {
    if (context_stack_.size() > 1) {
        context_stack_.pop_back();
    }
}

ParsingContext ParserContext::current_context() const {
    return context_stack_.back();
}

bool ParserContext::in_context(ParsingContext context) const {
    return std::find(context_stack_.begin(), context_stack_.end(), context) != context_stack_.end();
}

void ParserContext::enable_feature(LanguageFeature feature) {
    enabled_features_.insert(feature);
}

void ParserContext::disable_feature(LanguageFeature feature) {
    enabled_features_.erase(feature);
}

bool ParserContext::is_feature_enabled(LanguageFeature feature) const {
    return enabled_features_.find(feature) != enabled_features_.end();
}

std::string_view ParserContext::get_source_snippet(SourceRange range) const {
    if (range.start.offset >= source_text_.size()) {
        return {};
    }
    
    size_t start = range.start.offset;
    size_t length = std::min(static_cast<size_t>(range.length()), source_text_.size() - start);
    
    return source_text_.substr(start, length);
}

std::string_view ParserContext::get_line_text(uint32_t line_number) const {
    ensure_line_offsets_computed();
    
    if (line_number == 0 || line_number > line_offsets_.size()) {
        return {};
    }
    
    size_t line_start = line_offsets_[line_number - 1];
    size_t line_end = (line_number < line_offsets_.size()) ? 
                      line_offsets_[line_number] : source_text_.size();
    
    // Remove trailing newline if present
    if (line_end > line_start && (source_text_[line_end - 1] == '\n' || source_text_[line_end - 1] == '\r')) {
        line_end--;
        if (line_end > line_start && source_text_[line_end - 1] == '\r') {
            line_end--; // Handle \r\n
        }
    }
    
    return source_text_.substr(line_start, line_end - line_start);
}

SourceLocation ParserContext::offset_to_location(size_t offset) const {
    ensure_line_offsets_computed();
    
    if (offset >= source_text_.size()) {
        if (line_offsets_.empty()) {
            return SourceLocation(offset, 1, 1);
        }
        return SourceLocation(offset, line_offsets_.size(), 1);
    }
    
    // Binary search for the line
    auto it = std::upper_bound(line_offsets_.begin(), line_offsets_.end(), offset);
    
    uint32_t line = std::distance(line_offsets_.begin(), it);
    if (line == 0) line = 1;
    
    size_t line_start = (line > 1) ? line_offsets_[line - 2] : 0;
    uint32_t column = offset - line_start + 1;
    
    return SourceLocation(offset, line, column);
}

SourceRange ParserContext::make_range(SourceLocation start, SourceLocation end) const {
    return SourceRange(start, end);
}

SourceRange ParserContext::make_range(SourceLocation location, uint32_t length) const {
    return SourceRange(location, length);
}

bool ParserContext::in_type_context() const {
    return in_context(ParsingContext::TypeDeclaration) ||
           in_context(ParsingContext::InterfaceDeclaration) ||
           in_context(ParsingContext::EnumDeclaration);
}

bool ParserContext::in_function_context() const {
    return in_context(ParsingContext::FunctionDeclaration);
}

bool ParserContext::in_expression_context() const {
    return in_context(ParsingContext::ExpressionContext);
}

bool ParserContext::in_statement_context() const {
    return in_context(ParsingContext::BlockStatement) ||
           in_context(ParsingContext::IfStatement) ||
           in_context(ParsingContext::WhileLoop) ||
           in_context(ParsingContext::ForLoop);
}

void ParserContext::suggest_fix(const std::string& suggestion, SourceRange range) {
    if (!diagnostics_.empty()) {
        diagnostics_.back().add_suggestion(suggestion);
        diagnostics_.back().add_related_location(range);
    }
}

void ParserContext::suggest_insertion(const std::string& text, SourceLocation location) {
    suggest_fix("Insert '" + text + "'", SourceRange(location, 0));
}

void ParserContext::suggest_replacement(const std::string& text, SourceRange range) {
    suggest_fix("Replace with '" + text + "'", range);
}

void ParserContext::suggest_removal(SourceRange range) {
    suggest_fix("Remove this", range);
}

void ParserContext::report_unexpected_token(const Token& token, const std::string& expected) {
    std::string message = "Unexpected token '" + std::string(to_string(token.kind)) + "'";
    if (!expected.empty()) {
        message += ", expected " + expected;
    }
    
    report_error(message, token.range());
}

void ParserContext::report_missing_token(TokenKind expected, SourceLocation location) {
    std::string message = "Missing '" + std::string(to_string(expected)) + "'";
    report_error(message, location);
}

void ParserContext::report_extra_token(const Token& token) {
    std::string message = "Unexpected '" + std::string(to_string(token.kind)) + "'";
    report_error(message, token.range());
    suggest_removal(token.range());
}

void ParserContext::report_diagnostic_internal(ParserDiagnostic diagnostic) {
    if (diagnostic.level == DiagnosticLevel::Error) {
        error_count_++;
    } else if (diagnostic.level == DiagnosticLevel::Warning) {
        warning_count_++;
    }
    
    diagnostics_.push_back(std::move(diagnostic));
    
    if (diagnostic_sink_) {
        diagnostic_sink_->report_diagnostic(diagnostics_.back());
    }
}

void ParserContext::ensure_line_offsets_computed() const {
    if (line_offsets_computed_) {
        return;
    }
    
    line_offsets_.clear();
    line_offsets_.push_back(0); // First line starts at offset 0
    
    for (size_t i = 0; i < source_text_.size(); ++i) {
        if (source_text_[i] == '\n') {
            line_offsets_.push_back(i + 1);
        }
    }
    
    line_offsets_computed_ = true;
}

// DefaultDiagnosticSink implementation

void DefaultDiagnosticSink::report_diagnostic(const ParserDiagnostic& diagnostic) {
    print_diagnostic(diagnostic);
}

void DefaultDiagnosticSink::print_diagnostic(const ParserDiagnostic& diagnostic) {
    // Print the diagnostic level and message
    std::cerr << level_to_string(diagnostic.level) << ": ";
    std::cerr << diagnostic.message;
    std::cerr << " at " << diagnostic.location.line << ":" << diagnostic.location.column;
    
    if (!diagnostic.error_code.empty()) {
        std::cerr << " [" << diagnostic.error_code << "]";
    }
    
    std::cerr << std::endl;
    
    // Print suggestions if any
    for (const auto& suggestion : diagnostic.suggestions) {
        std::cerr << "  hint: " << suggestion << std::endl;
    }
}

void DefaultDiagnosticSink::print_source_snippet(const ParserDiagnostic& diagnostic, 
                                                std::string_view source_text) {
    // This would be implemented to show source context with highlighting
    // For now, just a placeholder
    (void)diagnostic;
    (void)source_text;
}

const char* DefaultDiagnosticSink::level_to_string(DiagnosticLevel level) {
    switch (level) {
        case DiagnosticLevel::Error: return "error";
        case DiagnosticLevel::Warning: return "warning";
        case DiagnosticLevel::Info: return "info";
        case DiagnosticLevel::Hint: return "hint";
        default: return "unknown";
    }
}

} // namespace Mycelium::Scripting::Parser