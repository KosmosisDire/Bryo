#include "sharpie/semantic_analyzer/semantic_analyzer.hpp"
#include "sharpie/common/logger.hpp"
#include <fstream>
#include <sstream>



namespace Mycelium::Scripting::Lang
{

    // ============================================================================
    // Enhanced Logging Methods
    // ============================================================================

    void SemanticAnalyzer::log_semantic_ir_summary()
    {
        LOG_INFO("=== SEMANTIC IR SUMMARY ===", "SEMANTIC");

        // Log classes
        const auto &classes = ir->symbol_table.get_classes();
        LOG_INFO("Classes registered: " + std::to_string(classes.size()), "COMPILER");
        for (const auto &[name, class_symbol] : classes)
        {
            LOG_INFO("  Class: " + name +
                         " (fields: " + std::to_string(class_symbol.field_registry.size()) +
                         ", methods: " + std::to_string(class_symbol.method_registry.size()) +
                         ", constructors: " + std::to_string(class_symbol.constructors.size()) + ")",
                     "COMPILER");

            for (const auto &[field_name, field_symbol] : class_symbol.field_registry)
            {
                LOG_INFO("    Field: " + field_name + " (scope: " + field_symbol.owning_scope + ")", "COMPILER");
            }

            // Log methods with enhanced information
            for (const auto &[method_name, method_symbol] : class_symbol.method_registry)
            {
                std::string method_info = "    Method: " + method_name +
                                          " (static: " + (method_symbol.is_static ? "yes" : "no") +
                                          ", params: " + std::to_string(method_symbol.parameter_names.size()) +
                                          ", defined: " + (method_symbol.is_defined ? "yes" : "no") + ")";
                LOG_INFO(method_info, "SEMANTIC");
            }
        }

        LOG_INFO("=== END SEMANTIC IR SUMMARY ===", "SEMANTIC");
    }

    void SemanticAnalyzer::log_forward_declarations()
    {
        LOG_INFO("=== FORWARD DECLARATION ANALYSIS ===", "SEMANTIC");

        auto forward_methods = ir->symbol_table.get_forward_declared_methods();
        auto forward_classes = ir->symbol_table.get_forward_declared_classes();

        if (!forward_methods.empty())
        {
            LOG_WARN("Unresolved forward declared methods:", "SEMANTIC");
            for (const auto *method : forward_methods)
            {
                LOG_WARN("  " + method->qualified_name + " (in class: " + method->containing_class + ")", "SEMANTIC");
            }
        }

        if (!forward_classes.empty())
        {
            LOG_WARN("Unresolved forward declared classes:", "SEMANTIC");
            for (const auto *class_symbol : forward_classes)
            {
                LOG_WARN("  " + class_symbol->name, "SEMANTIC");
            }
        }

        LOG_INFO("=== END FORWARD DECLARATION ANALYSIS ===", "SEMANTIC");
    }

    void SemanticAnalyzer::log_class_registry()
    {
        LOG_INFO("=== DETAILED CLASS REGISTRY ===", "SEMANTIC");

        const auto &classes = ir->symbol_table.get_classes();
        for (const auto &[name, class_symbol] : classes)
        {
            LOG_INFO("Class: " + name, "SEMANTIC");
            LOG_INFO("  Defined: " + std::string(class_symbol.is_defined ? "yes" : "no"), "SEMANTIC");
            LOG_INFO("  Forward declared: " + std::string(class_symbol.is_forward_declared ? "yes" : "no"), "SEMANTIC");
            LOG_INFO("  Base class: " + (class_symbol.base_class.empty() ? "none" : class_symbol.base_class), "SEMANTIC");

            if (!class_symbol.field_registry.empty())
            {
                LOG_INFO("  Fields:", "SEMANTIC");
                for (const auto &[field_name, field_symbol] : class_symbol.field_registry)
                {
                    LOG_INFO("    " + field_name + " (used: " + (field_symbol.is_used ? "yes" : "no") + ")", "SEMANTIC");
                }
            }

            if (!class_symbol.method_registry.empty())
            {
                LOG_INFO("  Methods:", "SEMANTIC");
                for (const auto &[method_name, method_symbol] : class_symbol.method_registry)
                {
                    std::string method_details = "    " + method_name +
                                                 " (constructor: " + (method_symbol.is_constructor ? "yes" : "no") +
                                                 ", destructor: " + (method_symbol.is_destructor ? "yes" : "no") +
                                                 ", external: " + (method_symbol.is_external ? "yes" : "no") + ")";
                    LOG_INFO(method_details, "SEMANTIC");
                }
            }
        }

        LOG_INFO("=== END DETAILED CLASS REGISTRY ===", "SEMANTIC");
    }

    void SemanticAnalyzer::log_method_registry()
    {
        LOG_INFO("=== GLOBAL METHOD REGISTRY ===", "SEMANTIC");

        // Count methods by type
        int constructor_count = 0;
        int destructor_count = 0;
        int external_count = 0;
        int static_count = 0;
        int instance_count = 0;

        const auto &classes = ir->symbol_table.get_classes();
        for (const auto &[class_name, class_symbol] : classes)
        {
            for (const auto &[method_name, method_symbol] : class_symbol.method_registry)
            {
                if (method_symbol.is_constructor)
                    constructor_count++;
                if (method_symbol.is_destructor)
                    destructor_count++;
                if (method_symbol.is_external)
                    external_count++;
                if (method_symbol.is_static)
                    static_count++;
                else
                    instance_count++;

                // Log detailed method information
                std::string method_info = "Method: " + method_symbol.qualified_name +
                                          " (params: " + std::to_string(method_symbol.parameter_names.size()) +
                                          ", defined: " + (method_symbol.is_defined ? "yes" : "no") + ")";
                LOG_INFO("  " + method_info, "SEMANTIC");
            }
        }

        LOG_INFO("Method summary - Constructors: " + std::to_string(constructor_count) +
                     ", Destructors: " + std::to_string(destructor_count) +
                     ", External: " + std::to_string(external_count) +
                     ", Static: " + std::to_string(static_count) +
                     ", Instance: " + std::to_string(instance_count),
                 "SEMANTIC");

        LOG_INFO("=== END GLOBAL METHOD REGISTRY ===", "SEMANTIC");
    }

    void SemanticAnalyzer::log_scope_information()
    {
        LOG_INFO("=== SCOPE ANALYSIS ===", "SEMANTIC");

        auto available_vars = ir->symbol_table.get_available_variables_in_scope();
        LOG_INFO("Variables in current scope: " + std::to_string(available_vars.size()), "SEMANTIC");
        for (const std::string &var_name : available_vars)
        {
            auto *var_symbol = ir->symbol_table.find_variable(var_name);
            if (var_symbol)
            {
                std::string var_info = "  " + var_name +
                                       " (parameter: " + (var_symbol->is_parameter ? "yes" : "no") +
                                       ", field: " + (var_symbol->is_field ? "yes" : "no") +
                                       ", used: " + (var_symbol->is_used ? "yes" : "no") +
                                       ", assigned: " + (var_symbol->is_definitely_assigned ? "yes" : "no") + ")";
                LOG_INFO(var_info, "SEMANTIC");
            }
        }

        LOG_INFO("Current scope: " + context->getFullScopePath(), "SEMANTIC");
        LOG_INFO("Scope depth: " + std::to_string(context->currentScopeDepth), "SEMANTIC");
        LOG_INFO("=== END SCOPE ANALYSIS ===", "SEMANTIC");
    }

} // namespace Mycelium::Scripting::Language