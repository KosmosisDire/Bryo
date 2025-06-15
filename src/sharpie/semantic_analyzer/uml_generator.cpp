#include "sharpie/semantic_analyzer/uml_generator.hpp"
#include "sharpie/common/logger.hpp"
#include <fstream>
#include <sstream>
#include <set>
#include <string>
#include <vector>



namespace Mycelium::Scripting::Lang
{

// Helper to create a UML-safe alias by replacing all '.' with '_'
std::string create_uml_alias(const std::string& qualified_name) {
    std::string alias = qualified_name;
    size_t pos = alias.find('.');
    while (pos != std::string::npos) {
        alias.replace(pos, 1, "_");
        pos = alias.find('.', pos + 1); // Find the next occurrence
    }
    return alias;
}

// Struct to hold relationship data before rendering
struct UmlRelationship {
    std::string caller_alias;
    std::string callee_alias;
    bool is_forward;

    // For use in std::set to uniquely identify a relationship type
    bool operator<(const UmlRelationship& other) const {
        if (caller_alias != other.caller_alias) return caller_alias < other.caller_alias;
        if (callee_alias != other.callee_alias) return callee_alias < other.callee_alias;
        return is_forward < other.is_forward;
    }
};

void UmlGenerator::generate(
    const SemanticIR& ir,
    const std::string& output_filename)
{
    LOG_INFO("Generating PlantUML class diagram from Semantic IR...", "UML_GENERATOR");
    
    std::stringstream plantuml_output;
    
    plantuml_output << "@startuml\n";
    plantuml_output << "!theme toy\n";
    
    const auto& classes = ir.symbol_table.get_classes();
    for (const auto& [class_name, class_symbol] : classes) {
        std::string alias = create_uml_alias(class_name);
        plantuml_output << "class \"" << class_name << "\" as " << alias << " {\n";
        
        // Add fields
        for (const auto& [field_name, field_symbol] : class_symbol.field_registry) {
            std::string type_name = "unknown";
            if (field_symbol.type) {
                if (auto ident = std::get_if<std::shared_ptr<IdentifierNode>>(&field_symbol.type->name_segment)) {
                    type_name = (*ident)->name;
                }
            }
            plantuml_output << "  + " << type_name << " " << field_name << "\n";
        }
        
        if (!class_symbol.field_registry.empty() && !class_symbol.method_registry.empty()) {
            plantuml_output << "  ..\n";
        }
        
        // Add methods
        for (const auto& [method_name, method_symbol] : class_symbol.method_registry) {
            if (method_name == "%ctor" || method_name == "%dtor") continue;
            
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
    
    // --- FIX: Use a data structure that allows for multiple relationship types ---
    plantuml_output << "' Method call dependencies\n";
    
    // Use a set of a struct to store unique relationship *types*
    // (e.g., one A-->B and one A..>B is allowed, but not two A-->B)
    std::set<UmlRelationship> unique_relationships;
    
    for (const auto& [symbol_id, usages] : ir.usage_graph) {
        for (const auto& usage : usages) {
            if (usage.kind == UsageKind::Call) {
                const auto* callee_symbol = ir.symbol_table.find_method(usage.qualified_symbol_id);
                if (!callee_symbol) continue;

                const std::string& caller_class = usage.context_class_name;
                const std::string& callee_class = callee_symbol->containing_class;
                
                if (caller_class.empty() || callee_class.empty()) continue;

                UmlRelationship rel;
                rel.caller_alias = create_uml_alias(caller_class);
                rel.callee_alias = create_uml_alias(callee_class);
                rel.is_forward = callee_symbol->declaration_location.lineStart > usage.location.lineStart;

                unique_relationships.insert(rel);
                LOG_DEBUG("Registering relationship: " + caller_class + " -> " + callee_class + (rel.is_forward ? " (forward)" : " (normal)"), "UML_GENERATOR");
            }
        }
    }
    
    // Render the unique relationships
    for (const auto& rel : unique_relationships) {
        std::string arrow = rel.is_forward ? "..>" : "-->";
        plantuml_output << rel.caller_alias << " " << arrow << " " << rel.callee_alias << "\n";
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