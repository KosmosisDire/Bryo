#pragma once

#include "../ast/ast_location.hpp" // For SourceLocation
#include <string>
#include <vector>

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

} // namespace Mycelium::Scripting::Lang