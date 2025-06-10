#pragma once

#include "semantic_ir.hpp" // Use the new SemanticIR
#include <string>

namespace Mycelium::Scripting::Lang
{

/**
 * @class UmlGenerator
 * @brief Generates a PlantUML class diagram from a SemanticIR.
 */
class UmlGenerator {
public:
    /**
     * @brief Generates a UML diagram and saves it to a file.
     * @param ir The complete Semantic IR containing all declarations and usages.
     * @param output_filename The path to the output .puml file.
     */
    void generate(
        const SemanticIR& ir,
        const std::string& output_filename = "tests/build/class_diagram.puml"
    );
};

} // namespace Mycelium::Scripting::Lang