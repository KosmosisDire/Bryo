#pragma once

#include "../ast/ast_location.hpp"
#include "symbol_table.hpp"
#include <string>
#include <vector>
#include <map>

namespace Mycelium::Scripting::Lang
{

// ============================================================================
// Semantic IR Enumerations
// ============================================================================

enum class SymbolKind {
    Class,
    Function,
    Variable,
    Field,
    Parameter,
    Namespace
};

enum class UsageKind {
    Read,
    Write,
    Call,
    Instantiation,
    TypeReference,
    Inheritance
};

// ============================================================================
// Core Semantic IR Structures
// ============================================================================

/**
 * @struct SymbolUsage
 * @brief Represents a single reference (usage) of a declared symbol.
 */
struct SymbolUsage {
    // What is being used?
    std::string qualified_symbol_id; // e.g., "MyClass.myMethod", "MyClass.myField"

    // Where is it being used?
    SourceLocation location;
    UsageKind kind;

    // What is the context of the usage?
    std::string context_scope_path;
    std::string context_class_name;
    std::string context_function_name;
};

/**
 * @struct SemanticError
 * @brief Represents a single diagnostic message (error or warning).
 */
struct SemanticError {
    std::string message;
    SourceLocation location;
    enum class Severity { Error, Warning, Info } severity;
};

/**
 * @class SemanticIR
 * @brief The complete, unified result of semantic analysis.
 *
 * This class serves as a rich, queryable model of the code's meaning,
 * containing all declarations, their usages, and any diagnostics.
 */
class SemanticIR {
public:
    // All declarations found in the code.
    SymbolTable symbol_table;

    // A graph connecting symbol declarations to all of their usages.
    // Key: The qualified ID of a symbol (e.g., "MyClass.myMethod").
    // Value: A vector of all places that symbol is used.
    std::map<std::string, std::vector<SymbolUsage>> usage_graph;

    // Diagnostics generated during analysis.
    std::vector<SemanticError> errors;
    std::vector<SemanticError> warnings;

    // --- Public Methods ---

    bool has_errors() const { return !errors.empty(); }
    bool has_warnings() const { return !warnings.empty(); }

    void add_error(const std::string& message, SourceLocation location) {
        errors.push_back({message, location, SemanticError::Severity::Error});
    }

    void add_warning(const std::string& message, SourceLocation location) {
        warnings.push_back({message, location, SemanticError::Severity::Warning});
    }

    void add_usage(const SymbolUsage& usage) {
        usage_graph[usage.qualified_symbol_id].push_back(usage);
    }
};

} // namespace Mycelium::Scripting::Lang