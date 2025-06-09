#include "sharpie/semantic_analyzer/semantic_analyzer.hpp"
#include "sharpie/common/logger.hpp"
#include <fstream>
#include <sstream>

using namespace Mycelium::Scripting::Common; // For Logger macros

namespace Mycelium::Scripting::Lang
{

// ============================================================================
// Enhanced Logging Methods
// ============================================================================

void SemanticAnalyzer::log_semantic_ir_summary() {
    LOG_INFO("=== SEMANTIC IR SUMMARY ===", "COMPILER");
    
    // Log classes
    const auto& classes = symbolTable->get_classes();
    LOG_INFO("Classes registered: " + std::to_string(classes.size()), "COMPILER");
    for (const auto& [name, class_symbol] : classes) {
        LOG_INFO("  Class: " + name + 
                " (fields: " + std::to_string(class_symbol.field_names.size()) + 
                ", methods: " + std::to_string(class_symbol.method_registry.size()) + 
                ", constructors: " + std::to_string(class_symbol.constructors.size()) + ")", "COMPILER");
        
        // Log fields with enhanced information
        for (const auto& [field_name, field_symbol] : class_symbol.field_registry) {
            LOG_INFO("    Field: " + field_name + " (scope: " + field_symbol.owning_scope + ")", "COMPILER");
        }
        
        // Log methods with enhanced information
        for (const auto& [method_name, method_symbol] : class_symbol.method_registry) {
            std::string method_info = "    Method: " + method_name + 
                                    " (static: " + (method_symbol.is_static ? "yes" : "no") + 
                                    ", params: " + std::to_string(method_symbol.parameter_names.size()) + 
                                    ", defined: " + (method_symbol.is_defined ? "yes" : "no") + ")";
            LOG_INFO(method_info, "COMPILER");
        }
    }
    
    // Log dependency analysis results instead of old forward declaration logic
    LOG_INFO("Forward declared method calls found: " + std::to_string(discoveredForwardCalls.size()), "COMPILER");
    if (!discoveredForwardCalls.empty()) {
        LOG_INFO("Forward call dependencies:", "COMPILER");
        for (const auto& call : discoveredForwardCalls) {
            LOG_INFO("  " + call, "COMPILER");
        }
    }
    
    LOG_INFO("=== END SEMANTIC IR SUMMARY ===", "COMPILER");
}

void SemanticAnalyzer::log_forward_declarations() {
    LOG_INFO("=== FORWARD DECLARATION ANALYSIS ===", "COMPILER");
    
    auto forward_methods = symbolTable->get_forward_declared_methods();
    auto forward_classes = symbolTable->get_forward_declared_classes();
    
    if (!forward_methods.empty()) {
        LOG_WARN("Unresolved forward declared methods:", "COMPILER");
        for (const auto* method : forward_methods) {
            LOG_WARN("  " + method->qualified_name + " (in class: " + method->containing_class + ")", "COMPILER");
        }
    }
    
    if (!forward_classes.empty()) {
        LOG_WARN("Unresolved forward declared classes:", "COMPILER");
        for (const auto* class_symbol : forward_classes) {
            LOG_WARN("  " + class_symbol->name, "COMPILER");
        }
    }
    
    LOG_INFO("=== END FORWARD DECLARATION ANALYSIS ===", "COMPILER");
}

void SemanticAnalyzer::log_class_registry() {
    LOG_INFO("=== DETAILED CLASS REGISTRY ===", "COMPILER");
    
    const auto& classes = symbolTable->get_classes();
    for (const auto& [name, class_symbol] : classes) {
        LOG_INFO("Class: " + name, "COMPILER");
        LOG_INFO("  Defined: " + std::string(class_symbol.is_defined ? "yes" : "no"), "COMPILER");
        LOG_INFO("  Forward declared: " + std::string(class_symbol.is_forward_declared ? "yes" : "no"), "COMPILER");
        LOG_INFO("  Base class: " + (class_symbol.base_class.empty() ? "none" : class_symbol.base_class), "COMPILER");
        
        if (!class_symbol.field_registry.empty()) {
            LOG_INFO("  Fields:", "COMPILER");
            for (const auto& [field_name, field_symbol] : class_symbol.field_registry) {
                LOG_INFO("    " + field_name + " (used: " + (field_symbol.is_used ? "yes" : "no") + ")", "COMPILER");
            }
        }
        
        if (!class_symbol.method_registry.empty()) {
            LOG_INFO("  Methods:", "COMPILER");
            for (const auto& [method_name, method_symbol] : class_symbol.method_registry) {
                std::string method_details = "    " + method_name + 
                                           " (constructor: " + (method_symbol.is_constructor ? "yes" : "no") +
                                           ", destructor: " + (method_symbol.is_destructor ? "yes" : "no") +
                                           ", external: " + (method_symbol.is_external ? "yes" : "no") + ")";
                LOG_INFO(method_details, "COMPILER");
            }
        }
    }
    
    LOG_INFO("=== END DETAILED CLASS REGISTRY ===", "COMPILER");
}

void SemanticAnalyzer::log_method_registry() {
    LOG_INFO("=== GLOBAL METHOD REGISTRY ===", "COMPILER");
    
    // Count methods by type
    int constructor_count = 0;
    int destructor_count = 0;
    int external_count = 0;
    int static_count = 0;
    int instance_count = 0;
    
    const auto& classes = symbolTable->get_classes();
    for (const auto& [class_name, class_symbol] : classes) {
        for (const auto& [method_name, method_symbol] : class_symbol.method_registry) {
            if (method_symbol.is_constructor) constructor_count++;
            if (method_symbol.is_destructor) destructor_count++;
            if (method_symbol.is_external) external_count++;
            if (method_symbol.is_static) static_count++;
            else instance_count++;
            
            // Log detailed method information
            std::string method_info = "Method: " + method_symbol.qualified_name +
                                    " (params: " + std::to_string(method_symbol.parameter_names.size()) + 
                                    ", defined: " + (method_symbol.is_defined ? "yes" : "no") + ")";
            LOG_INFO("  " + method_info, "COMPILER");
        }
    }
    
    LOG_INFO("Method summary - Constructors: " + std::to_string(constructor_count) +
             ", Destructors: " + std::to_string(destructor_count) +
             ", External: " + std::to_string(external_count) +
             ", Static: " + std::to_string(static_count) +
             ", Instance: " + std::to_string(instance_count), "COMPILER");
    
    LOG_INFO("=== END GLOBAL METHOD REGISTRY ===", "COMPILER");
}

void SemanticAnalyzer::log_scope_information() {
    LOG_INFO("=== SCOPE ANALYSIS ===", "COMPILER");
    
    auto available_vars = symbolTable->get_available_variables_in_scope();
    LOG_INFO("Variables in current scope: " + std::to_string(available_vars.size()), "COMPILER");
    for (const std::string& var_name : available_vars) {
        auto* var_symbol = symbolTable->find_variable(var_name);
        if (var_symbol) {
            std::string var_info = "  " + var_name + 
                                 " (parameter: " + (var_symbol->is_parameter ? "yes" : "no") +
                                 ", field: " + (var_symbol->is_field ? "yes" : "no") +
                                 ", used: " + (var_symbol->is_used ? "yes" : "no") +
                                 ", assigned: " + (var_symbol->is_definitely_assigned ? "yes" : "no") + ")";
            LOG_INFO(var_info, "COMPILER");
        }
    }
    
    LOG_INFO("Current scope: " + context->getFullScopePath(), "COMPILER");
    LOG_INFO("Scope depth: " + std::to_string(context->currentScopeDepth), "COMPILER");
    LOG_INFO("=== END SCOPE ANALYSIS ===", "COMPILER");
}


} // namespace Mycelium::Scripting::Language