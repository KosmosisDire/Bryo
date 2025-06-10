#include "sharpie/semantic_analyzer/uml_generator.hpp"
#include "sharpie/common/logger.hpp"
#include <fstream>
#include <sstream>
#include <set>
#include <string>

using namespace Mycelium::Scripting::Common;

namespace Mycelium::Scripting::Lang
{

void UmlGenerator::generate(
    const SemanticIR& ir,
    const std::string& output_filename)
{
    LOG_INFO("Generating PlantUML class diagram from Semantic IR...", "UML_GENERATOR");
    
    std::stringstream plantuml_output;
    
    plantuml_output << "@startuml\n";
    plantuml_output << "!theme toy\n";
    
    // --- FIX: Restore the generation of class content ---
    const auto& classes = ir.symbol_table.get_classes();
    for (const auto& [class_name, class_symbol] : classes) {
        plantuml_output << "class " << class_name << " {\n";
        
        // Add fields with proper visibility and types
        for (const auto& [field_name, field_symbol] : class_symbol.field_registry) {
            std::string type_name = "unknown";
            if (field_symbol.type) {
                if (auto ident = std::get_if<std::shared_ptr<IdentifierNode>>(&field_symbol.type->name_segment)) {
                    type_name = (*ident)->name;
                }
            }
            plantuml_output << "  + " << type_name << " " << field_name << "\n";
        }
        
        // Add separator line if we have both fields and methods
        if (!class_symbol.field_registry.empty() && !class_symbol.method_registry.empty()) {
            plantuml_output << "  ..\n";
        }
        
        // Add methods with proper visibility and signatures
        for (const auto& [method_name, method_symbol] : class_symbol.method_registry) {
            if (method_name == "%ctor" || method_name == "%dtor") {
                continue; // Skip internal names for cleaner display
            }
            
            std::string visibility = "+";
            std::string return_type = "void";
            if (method_symbol.return_type) {
                if (auto ident = std::get_if<std::shared_ptr<IdentifierNode>>(&method_symbol.return_type->name_segment)) {
                    return_type = (*ident)->name;
                }
            }
            
            std::stringstream params;
            for (size_t i = 0; i < method_symbol.parameter_names.size(); ++i) {
                if (i > 0) params << ", ";
                std::string param_type = "unknown";
                if (i < method_symbol.parameter_types.size() && method_symbol.parameter_types[i]) {
                    if (auto ident = std::get_if<std::shared_ptr<IdentifierNode>>(&method_symbol.parameter_types[i]->name_segment)) {
                        param_type = (*ident)->name;
                    }
                }
                params << param_type << " " << method_symbol.parameter_names[i];
            }
            
            std::string static_modifier = method_symbol.is_static ? "{static} " : "";
            
            plantuml_output << "  " << visibility << " " << static_modifier << method_name 
                          << "(" << params.str() << ") : " << return_type << "\n";
        }
        
        plantuml_output << "}\n\n";
    }
    
    // --- Generate dependencies from the usage_graph ---
    plantuml_output << "' Method call dependencies\n";
    
    std::set<std::string> forward_class_relationships;
    std::set<std::string> normal_class_relationships;
    
    for (const auto& [symbol_id, usages] : ir.usage_graph) {
        for (const auto& usage : usages) {
            if (usage.kind == UsageKind::Call) {
                const auto* callee_symbol = ir.symbol_table.find_method(usage.qualified_symbol_id);
                if (!callee_symbol) continue;

                const std::string& caller_class = usage.context_class_name;
                const std::string& callee_class = callee_symbol->containing_class;

                if (caller_class.empty() || callee_class.empty() || caller_class == callee_class) {
                    continue;
                }

                std::string relationship = caller_class + " --> " + callee_class;

                bool is_forward_call = callee_symbol->declaration_location.lineStart > usage.location.lineStart;

                if (is_forward_call) {
                    forward_class_relationships.insert(relationship);
                } else {
                    normal_class_relationships.insert(relationship);
                }
            }
        }
    }
    
    // Render forward relationships
    for (const auto& relationship : forward_class_relationships) {
        plantuml_output << relationship << " : forward\n";
    }
    
    // Render normal relationships
    for (const auto& relationship : normal_class_relationships) {
        if (forward_class_relationships.find(relationship) == forward_class_relationships.end()) {
             plantuml_output << relationship << "\n";
        }
    }
    
    plantuml_output << "\n@enduml\n";
    
    // Save to file
    std::string diagram_content = plantuml_output.str();
    std::ofstream file(output_filename);
    if (file.is_open()) {
        file << diagram_content;
        file.close();
        LOG_INFO("PlantUML class diagram saved to: " + output_filename, "UML_GENERATOR");
    } else {
        LOG_WARN("Could not save PlantUML diagram to file '" + output_filename + "'", "UML_GENERATOR");
    }
}

}