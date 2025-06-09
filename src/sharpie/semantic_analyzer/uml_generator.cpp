#include "sharpie/semantic_analyzer/uml_generator.hpp"
#include "sharpie/common/logger.hpp"
#include <fstream>
#include <sstream>
#include <set>

using namespace Mycelium::Scripting::Common;

namespace Mycelium::Scripting::Lang
{

void UmlGenerator::generate(
    const SymbolTable& symbolTable,
    const std::vector<MethodCallInfo>& discoveredMethodCalls,
    const std::string& output_filename)
{
    LOG_INFO("Generating PlantUML class diagram...", "UML_GENERATOR");
    
    std::stringstream plantuml_output;
    
    plantuml_output << "@startuml\n";
    plantuml_output << "!theme toy\n";
    
    // Generate classes
    const auto& classes = symbolTable.get_classes();
    
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
            // Use + for public fields (following current pattern)
            plantuml_output << "  +" << type_name << " " << field_name << "\n";
        }
        
        // Add separator line if we have both fields and methods
        if (!class_symbol.field_registry.empty() && !class_symbol.method_registry.empty()) {
            plantuml_output << "  ..\n";
        }
        
        // Add methods with proper visibility and signatures
        for (const auto& [method_name, method_symbol] : class_symbol.method_registry) {
            if (method_name == "%ctor" || method_name == "%dtor") {
                continue; // Skip internal constructor/destructor names for cleaner display
            }
            
            // Determine visibility (assuming public for now)
            std::string visibility = "+";
            
            // Get return type
            std::string return_type = "void";
            if (method_symbol.return_type) {
                if (auto ident = std::get_if<std::shared_ptr<IdentifierNode>>(&method_symbol.return_type->name_segment)) {
                    return_type = (*ident)->name;
                }
            }
            
            // Build parameter string
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
            
            // Add static modifier if applicable
            std::string static_modifier = method_symbol.is_static ? "{static} " : "";
            
            plantuml_output << "  " << visibility << static_modifier << method_name 
                          << "(" << params.str() << ") : " << return_type << "\n";
        }
        
        plantuml_output << "}\n\n";
    }
    
    // Add method call dependencies using structured call information
    plantuml_output << "' Method call dependencies\n";
    
    // Group calls by type (forward vs normal) and deduplicate class-level relationships
    std::set<std::string> forward_class_relationships;
    std::set<std::string> normal_class_relationships;
    
    for (const auto& call_info : discoveredMethodCalls) {
        if (call_info.caller_class != call_info.callee_class) {
            std::string relationship = call_info.caller_class + " --> " + call_info.callee_class;
            if (call_info.is_forward_call) {
                forward_class_relationships.insert(relationship);
            } else {
                normal_class_relationships.insert(relationship);
            }
        }
    }
    
    // Add forward declaration calls (dashed arrows)
    for (const auto& relationship : forward_class_relationships) {
        size_t arrow_pos = relationship.find(" --> ");
        if (arrow_pos != std::string::npos) {
            std::string caller_class = relationship.substr(0, arrow_pos);
            std::string callee_class = relationship.substr(arrow_pos + 5);
            plantuml_output << caller_class << " ..> " << callee_class << "\n";
        }
    }
    
    // Add normal method calls (solid arrows)
    for (const auto& relationship : normal_class_relationships) {
        size_t arrow_pos = relationship.find(" --> ");
        if (arrow_pos != std::string::npos) {
            std::string caller_class = relationship.substr(0, arrow_pos);
            std::string callee_class = relationship.substr(arrow_pos + 5);
            // Only show normal calls that aren't already shown as forward calls
            if (forward_class_relationships.find(relationship) == forward_class_relationships.end()) {
                plantuml_output << caller_class << " --> " << callee_class << "\n";
            }
        }
    }
    
    plantuml_output << "\n@enduml\n";
    
    // Save to .puml file
    std::string diagram_content = plantuml_output.str();
    std::ofstream file(output_filename);
    if (file.is_open()) {
        file << diagram_content;
        file.close();
        LOG_INFO("PlantUML class diagram saved to: " + output_filename, "UML_GENERATOR");
    } else {
        LOG_WARN("Could not save PlantUML diagram to file '" + output_filename + "', outputting to log:", "UML_GENERATOR");
        LOG_INFO("PlantUML Diagram:\n" + diagram_content, "UML_GENERATOR");
    }
}

} // namespace Mycelium::Scripting::Lang