#pragma once

#include <string>
#include <vector>

namespace Mycelium::Scripting::Lang
{

/**
 * @class SemanticContext
 * @brief Manages the transient state during semantic analysis.
 *
 * This class holds information about the current position and context within the
 * AST, such as the current class, method, and scope hierarchy. This allows the
 * main SemanticAnalyzer to be more stateless.
 */
class SemanticContext {
public:
    // Current location in the code structure
    std::string currentClassName;
    std::string currentMethodName;
    std::string currentNamespaceName;

    // Flags for the current method's context
    bool inStaticMethod = false;
    bool inConstructor = false;
    bool inInstanceMethod = false;

    // Stacks for tracking nested structures
    std::vector<std::string> loopStack;      // For validating break/continue
    std::vector<std::string> scopeStack;     // For tracking scope names and paths
    int currentScopeDepth = 0;

    /**
     * @brief Gets the full path of the current scope.
     * @return A string representing the scope, e.g., "Namespace.ClassName.MethodName.block_1".
     */
    std::string getFullScopePath() const {
        if (scopeStack.empty()) {
            return "global";
        }
        std::string path;
        for (size_t i = 0; i < scopeStack.size(); ++i) {
            if (i > 0) path += ".";
            path += scopeStack[i];
        }
        return path;
    }

    /**
     * @brief Resets the context to a clean state for a new analysis run.
     */
    void reset() {
        currentClassName.clear();
        currentMethodName.clear();
        currentNamespaceName.clear();
        inStaticMethod = false;
        inConstructor = false;
        inInstanceMethod = false;
        loopStack.clear();
        scopeStack.clear();
        currentScopeDepth = 0;
    }
};

} // namespace Mycelium::Scripting::Lang
