#pragma once

#include "symbol_table.hpp"
#include "dependency_info.hpp"
#include <vector>
#include <string>

namespace Mycelium::Scripting::Lang
{

/**
 * @class UmlGenerator
 * @brief Generates a PlantUML class diagram from semantic analysis results.
 */
class UmlGenerator {
public:
    /**
     * @brief Generates a UML diagram and saves it to a file.
     * @param symbolTable The populated symbol table containing class and method info.
     * @param discoveredMethodCalls A vector of discovered method call dependencies.
     * @param output_filename The path to the output .puml file.
     */
    void generate(
        const SymbolTable& symbolTable,
        const std::vector<MethodCallInfo>& discoveredMethodCalls,
        const std::string& output_filename = "tests/build/class_diagram.puml"
    );
};

} // namespace Mycelium::Scripting::Lang