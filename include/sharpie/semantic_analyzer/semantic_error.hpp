#pragma once

#include "../ast/ast_location.hpp" // For SourceLocation
#include "dependency_info.hpp"      // For MethodCallInfo
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
    std::vector<MethodCallInfo> method_calls; // Added to store call graph info

    bool has_errors() const { return !errors.empty(); }
    bool has_warnings() const { return !warnings.empty(); }
    
    void add_error(const std::string& message, SourceLocation location) {
        errors.emplace_back(message, location, SemanticError::Severity::Error);
    }
    
    void add_warning(const std::string& message, SourceLocation location) {
        warnings.emplace_back(message, location, SemanticError::Severity::Warning);
    }

    void add_method_call(const MethodCallInfo& call) {
        method_calls.push_back(call);
    }
    
    void merge(const SemanticAnalysisResult& other) {
        errors.insert(errors.end(), other.errors.begin(), other.errors.end());
        warnings.insert(warnings.end(), other.warnings.begin(), other.warnings.end());
        method_calls.insert(method_calls.end(), other.method_calls.begin(), other.method_calls.end());
    }
};

} // namespace Mycelium::Scripting::Lang