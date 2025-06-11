#include "sharpie/semantic_analyzer/symbol_table.hpp"
#include "sharpie/common/logger.hpp"

using namespace Mycelium::Scripting::Common; // For Logger macros

namespace Mycelium::Scripting::Lang
{

    // ============================================================================
    // SymbolTable Implementation
    // ============================================================================

    void SymbolTable::push_scope()
    {
        variable_scopes.emplace_back();
    }

    void SymbolTable::pop_scope()
    {
        if (!variable_scopes.empty())
        {
            variable_scopes.pop_back();
        }
    }

    void SymbolTable::declare_variable(const VariableSymbol &symbol)
    {
        if (variable_scopes.empty())
        {
            // Create global scope if none exists
            push_scope();
        }
        variable_scopes.back()[symbol.name] = symbol;
    }

    SymbolTable::VariableSymbol *SymbolTable::find_variable(const std::string &name)
    {
        // Search from innermost to outermost scope
        for (auto it = variable_scopes.rbegin(); it != variable_scopes.rend(); ++it)
        {
            auto var_it = it->find(name);
            if (var_it != it->end())
            {
                return &var_it->second;
            }
        }
        return nullptr;
    }

    bool SymbolTable::is_variable_declared_in_current_scope(const std::string &name)
    {
        if (variable_scopes.empty())
        {
            return false;
        }
        auto &current_scope = variable_scopes.back();
        return current_scope.find(name) != current_scope.end();
    }

    void SymbolTable::mark_variable_used(const std::string &name)
    {
        if (auto *var = find_variable(name))
        {
            var->is_used = true;
        }
    }

    void SymbolTable::declare_class(const ClassSymbol &symbol)
    {
        classes[symbol.name] = symbol;
    }

    SymbolTable::ClassSymbol *SymbolTable::find_class(const std::string &name)
    {
        auto it = classes.find(name);
        return (it != classes.end()) ? &it->second : nullptr;
    }

    void SymbolTable::declare_method(const MethodSymbol &symbol)
    {
        methods[symbol.qualified_name] = symbol;
    }

    SymbolTable::MethodSymbol *SymbolTable::find_method(const std::string &qualified_name)
    {
        auto it = methods.find(qualified_name);
        return (it != methods.end()) ? &it->second : nullptr;
    }

    const SymbolTable::MethodSymbol *SymbolTable::find_method(const std::string &qualified_name) const
    {
        auto it = methods.find(qualified_name);
        return (it != methods.end()) ? &it->second : nullptr;
    }

    // ============================================================================
    // Enhanced Semantic Analysis Methods
    // ============================================================================

    void SymbolTable::mark_method_as_forward_declared(const std::string &qualified_name)
    {
        auto *method = find_method(qualified_name);
        if (method)
        {
            method->is_forward_declared = true;
        }
    }

    void SymbolTable::mark_method_as_defined(const std::string &qualified_name)
    {
        auto *method = find_method(qualified_name);
        if (method)
        {
            method->is_defined = true;
            method->is_forward_declared = false; // No longer just forward declared
        }
    }

    void SymbolTable::mark_class_as_forward_declared(const std::string &class_name)
    {
        auto *class_symbol = find_class(class_name);
        if (class_symbol)
        {
            class_symbol->is_forward_declared = true;
        }
    }

    void SymbolTable::mark_class_as_defined(const std::string &class_name)
    {
        auto *class_symbol = find_class(class_name);
        if (class_symbol)
        {
            class_symbol->is_defined = true;
            class_symbol->is_forward_declared = false; // No longer just forward declared
        }
    }

    std::vector<SymbolTable::MethodSymbol *> SymbolTable::get_forward_declared_methods()
    {
        std::vector<MethodSymbol *> forward_declared;
        for (auto &[name, method] : methods)
        {
            if (method.is_forward_declared && !method.is_defined)
            {
                forward_declared.push_back(&method);
            }
        }
        return forward_declared;
    }

    std::vector<SymbolTable::ClassSymbol *> SymbolTable::get_forward_declared_classes()
    {
        std::vector<ClassSymbol *> forward_declared;
        for (auto &[name, class_symbol] : classes)
        {
            if (class_symbol.is_forward_declared && !class_symbol.is_defined)
            {
                forward_declared.push_back(&class_symbol);
            }
        }
        return forward_declared;
    }

    bool SymbolTable::has_unresolved_forward_declarations()
    {
        return !get_forward_declared_methods().empty() || !get_forward_declared_classes().empty();
    }

    SymbolTable::MethodSymbol *SymbolTable::find_method_in_class(const std::string &class_name, const std::string &method_name)
    {
        auto *class_symbol = find_class(class_name);
        if (!class_symbol)
            return nullptr;

        auto it = class_symbol->method_registry.find(method_name);
        return (it != class_symbol->method_registry.end()) ? &it->second : nullptr;
    }

    SymbolTable::VariableSymbol *SymbolTable::find_field_in_class(const std::string &class_name, const std::string &field_name)
    {
        auto *class_symbol = find_class(class_name);
        if (!class_symbol)
            return nullptr;

        auto it = class_symbol->field_registry.find(field_name);
        return (it != class_symbol->field_registry.end()) ? &it->second : nullptr;
    }

    std::vector<SymbolTable::MethodSymbol *> SymbolTable::get_constructors_for_class(const std::string &class_name)
    {
        std::vector<MethodSymbol *> constructors;
        auto *class_symbol = find_class(class_name);
        if (!class_symbol)
            return constructors;

        for (const std::string &ctor_name : class_symbol->constructors)
        {
            auto *method = find_method(ctor_name);
            if (method)
            {
                constructors.push_back(method);
            }
        }
        return constructors;
    }

    SymbolTable::MethodSymbol *SymbolTable::get_destructor_for_class(const std::string &class_name)
    {
        auto *class_symbol = find_class(class_name);
        if (!class_symbol || class_symbol->destructor.empty())
            return nullptr;

        return find_method(class_symbol->destructor);
    }

    std::string SymbolTable::get_current_scope_name()
    {
        // For now, return a simple scope indicator
        // This could be enhanced to track actual scope names
        return "scope_" + std::to_string(variable_scopes.size());
    }

    std::vector<std::string> SymbolTable::get_available_variables_in_scope()
    {
        std::vector<std::string> available_vars;
        for (const auto &scope : variable_scopes)
        {
            for (const auto &[name, symbol] : scope)
            {
                available_vars.push_back(name);
            }
        }
        return available_vars;
    }

} // namespace Mycelium::Scripting::Lang