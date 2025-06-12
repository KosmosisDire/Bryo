#include "sharpie/semantic_analyzer/semantic_analyzer.hpp"
#include "sharpie/common/logger.hpp"
#include <algorithm>

using namespace Mycelium::Scripting::Common; // For Logger macros

namespace Mycelium::Scripting::Lang
{

    // ============================================================================
    // Pass 1: Class and External Declaration Collection
    // ============================================================================

    void SemanticAnalyzer::collect_class_declarations(std::shared_ptr<CompilationUnitNode> node)
    {
        if (!node)
            return;

        LOG_INFO("Collecting class declarations across all namespaces", "SEMANTIC");

        // Process top-level members
        for (const auto &member : node->members)
        {
            if (auto ns_decl = std::dynamic_pointer_cast<NamespaceDeclarationNode>(member))
            {
                // FIX: Handle nested namespaces correctly by saving and restoring context.
                auto old_namespace = context->currentNamespaceName;
                if (!old_namespace.empty())
                {
                    context->currentNamespaceName += "." + ns_decl->name->name;
                }
                else
                {
                    context->currentNamespaceName = ns_decl->name->name;
                }
                LOG_INFO("Entering namespace: " + context->currentNamespaceName, "SEMANTIC");

                // Collect classes within namespace
                for (const auto &ns_member : ns_decl->members)
                {
                    if (auto class_decl = std::dynamic_pointer_cast<ClassDeclarationNode>(ns_member))
                    {
                        collect_class_structure(class_decl);
                    }
                }

                context->currentNamespaceName = old_namespace; // Restore outer namespace context
            }
            else if (auto class_decl = std::dynamic_pointer_cast<ClassDeclarationNode>(member))
            {
                // Global class declaration
                collect_class_structure(class_decl);
            }
        }
    }

    void SemanticAnalyzer::collect_external_declarations(std::shared_ptr<CompilationUnitNode> node)
    {
        if (!node)
            return;

        LOG_INFO("Collecting external method declarations", "SEMANTIC");

        // Process external declarations
        for (const auto &extern_decl : node->externs)
        {
            if (auto external_method = std::dynamic_pointer_cast<ExternalMethodDeclarationNode>(extern_decl))
            {
                analyze_declarations(external_method); // Reuse existing logic
            }
        }
    }

    void SemanticAnalyzer::collect_class_structure(std::shared_ptr<ClassDeclarationNode> node)
    {
        std::string class_name = node->name->name;
        if (!context->currentNamespaceName.empty())
        {
            class_name = context->currentNamespaceName + "." + class_name;
        }

        // Check for duplicate class declaration
        if (ir->symbol_table.find_class(class_name))
        {
            add_error("Class '" + class_name + "' already declared", node->name->location);
            return; // Don't process duplicate
        }

        // Create class symbol
        SymbolTable::ClassSymbol class_symbol;
        class_symbol.name = class_name;
        class_symbol.type_info.name = class_name; // Ensure type_info is populated
        class_symbol.declaration_location = node->location.value_or(SourceLocation{});
        class_symbol.is_defined = true;

        // Process base class inheritance
        if (!node->baseList.empty())
        {
            if (node->baseList.size() > 1)
            {
                add_error("Multiple inheritance not supported", node->location);
            }
            else
            {
                auto base_type = node->baseList[0];
                if (auto base_identifier = std::get_if<std::shared_ptr<IdentifierNode>>(&base_type->name_segment))
                {
                    std::string base_class_name = (*base_identifier)->name;
                    
                    // For now, assume simple names (no namespace resolution)
                    class_symbol.base_class = base_class_name;
                    
                    LOG_DEBUG("Class " + class_name + " inherits from " + base_class_name, "SEMANTIC");
                }
                else
                {
                    add_error("Complex base class names not yet supported", base_type->location);
                }
            }
        }

        // Process fields (basic structure only)
        for (const auto &member : node->members)
        {
            if (auto field_decl = std::dynamic_pointer_cast<FieldDeclarationNode>(member))
            {
                if (!field_decl->type)
                {
                    add_error("Field missing type in class " + class_name, field_decl->location);
                    continue;
                }

                for (const auto &declarator : field_decl->declarators)
                {
                    std::string field_name = declarator->name->name;

                    SymbolTable::VariableSymbol field_symbol;
                    field_symbol.name = field_name;
                    field_symbol.type = field_decl->type.value();
                    field_symbol.declaration_location = declarator->name->location.value_or(SourceLocation{});
                    field_symbol.is_field = true;
                    field_symbol.owning_scope = class_name;

                    class_symbol.field_registry[field_name] = field_symbol;
                }
            }
        }

        // Register the class
        ir->symbol_table.declare_class(class_symbol);

        LOG_INFO("Collected class structure: " + class_name + " with " +
                     std::to_string(class_symbol.field_registry.size()) + " fields",
                 "SEMANTIC");
    }

    // ============================================================================
    // Pass 2: Method Signature Collection
    // ============================================================================

    void SemanticAnalyzer::collect_method_signatures(std::shared_ptr<CompilationUnitNode> node)
    {
        if (!node)
            return;

        LOG_INFO("Collecting all method signatures to enable forward declarations", "SEMANTIC");

        // Process top-level members
        for (const auto &member : node->members)
        {
            if (auto ns_decl = std::dynamic_pointer_cast<NamespaceDeclarationNode>(member))
            {
                // FIX: Handle nested namespaces correctly
                auto old_namespace = context->currentNamespaceName;
                if (!old_namespace.empty())
                {
                    context->currentNamespaceName += "." + ns_decl->name->name;
                }
                else
                {
                    context->currentNamespaceName = ns_decl->name->name;
                }

                // Collect method signatures within namespace classes
                for (const auto &ns_member : ns_decl->members)
                {
                    if (auto class_decl = std::dynamic_pointer_cast<ClassDeclarationNode>(ns_member))
                    {
                        collect_class_signatures(class_decl);
                    }
                }

                context->currentNamespaceName = old_namespace; // Restore outer namespace context
            }
            else if (auto class_decl = std::dynamic_pointer_cast<ClassDeclarationNode>(member))
            {
                // Global class method signatures
                collect_class_signatures(class_decl);
            }
        }
        
        // After all method signatures are collected, inherit virtual methods from base classes
        inherit_virtual_methods_from_base_classes();
    }

    void SemanticAnalyzer::collect_class_signatures(std::shared_ptr<ClassDeclarationNode> node)
    {
        if (!node)
            return;

        // FIX: Use the full namespace-qualified class name to find the class symbol
        // and to qualify its methods.
        std::string class_name = node->name->name;
        if (!context->currentNamespaceName.empty())
        {
            class_name = context->currentNamespaceName + "." + class_name;
        }
        LOG_DEBUG("Collecting method signatures for class: " + class_name, "SEMANTIC");

        // Collect all method/constructor/destructor signatures in this class
        for (const auto &member : node->members)
        {
            if (auto method_decl = std::dynamic_pointer_cast<MethodDeclarationNode>(member))
            {
                collect_method_signature(method_decl, class_name);
            }
            else if (auto ctor_decl = std::dynamic_pointer_cast<ConstructorDeclarationNode>(member))
            {
                collect_constructor_signature(ctor_decl, class_name);
            }
            else if (auto dtor_decl = std::dynamic_pointer_cast<DestructorDeclarationNode>(member))
            {
                collect_destructor_signature(dtor_decl, class_name);
            }
        }
    }

    void SemanticAnalyzer::collect_method_signature(std::shared_ptr<MethodDeclarationNode> node, const std::string &class_name)
    {
        if (!node->type.has_value())
        {
            add_error("Method missing return type", node->location);
            return;
        }

        SymbolTable::MethodSymbol method_symbol;
        method_symbol.name = node->name->name;
        method_symbol.qualified_name = class_name + "." + node->name->name;
        method_symbol.return_type = node->type.value();
        method_symbol.parameters = node->parameters;
        method_symbol.declaration_location = node->location.value_or(SourceLocation{});
        method_symbol.containing_class = class_name;
        method_symbol.is_defined = node->body.has_value();

        for (const auto &param : node->parameters)
        {
            if (param->name)
                method_symbol.parameter_names.push_back(param->name->name);
            if (param->type)
                method_symbol.parameter_types.push_back(param->type);
        }

        for (const auto &modifier : node->modifiers)
        {
            if (modifier.first == ModifierKind::Static)
            {
                method_symbol.is_static = true;
            }
            else if (modifier.first == ModifierKind::Virtual)
            {
                method_symbol.is_virtual = true;
            }
        }

        // Register in global and class-local registries
        ir->symbol_table.declare_method(method_symbol);
        auto *class_symbol = ir->symbol_table.find_class(class_name);
        if (class_symbol)
        {
            class_symbol->method_registry[node->name->name] = method_symbol;
            
            // Add virtual methods to VTable order
            if (method_symbol.is_virtual)
            {
                class_symbol->virtual_method_order.push_back(method_symbol.qualified_name);
            }
        }

        LOG_DEBUG("Collected method signature: " + method_symbol.qualified_name, "SEMANTIC");
    }

    void SemanticAnalyzer::collect_constructor_signature(std::shared_ptr<ConstructorDeclarationNode> node, const std::string &class_name)
    {
        SymbolTable::MethodSymbol ctor_symbol;
        ctor_symbol.name = "%ctor";
        ctor_symbol.qualified_name = class_name + ".%ctor";
        ctor_symbol.return_type = create_primitive_type("void");
        ctor_symbol.parameters = node->parameters;
        ctor_symbol.declaration_location = node->location.value_or(SourceLocation{});
        ctor_symbol.containing_class = class_name;
        ctor_symbol.is_constructor = true;
        ctor_symbol.is_defined = node->body.has_value();

        for (const auto &param : node->parameters)
        {
            if (param->name)
                ctor_symbol.parameter_names.push_back(param->name->name);
            if (param->type)
                ctor_symbol.parameter_types.push_back(param->type);
        }

        ir->symbol_table.declare_method(ctor_symbol);
        auto *class_symbol = ir->symbol_table.find_class(class_name);
        if (class_symbol)
        {
            class_symbol->constructors.push_back(ctor_symbol.qualified_name);
            class_symbol->method_registry["%ctor"] = ctor_symbol;
        }

        LOG_DEBUG("Collected constructor signature: " + ctor_symbol.qualified_name, "SEMANTIC");
    }

    void SemanticAnalyzer::collect_destructor_signature(std::shared_ptr<DestructorDeclarationNode> node, const std::string &class_name)
    {
        SymbolTable::MethodSymbol dtor_symbol;
        dtor_symbol.name = "%dtor";
        dtor_symbol.qualified_name = class_name + ".%dtor";
        dtor_symbol.return_type = create_primitive_type("void");
        dtor_symbol.declaration_location = node->location.value_or(SourceLocation{});
        dtor_symbol.containing_class = class_name;
        dtor_symbol.is_destructor = true;
        dtor_symbol.is_defined = node->body.has_value();

        ir->symbol_table.declare_method(dtor_symbol);
        auto *class_symbol = ir->symbol_table.find_class(class_name);
        if (class_symbol)
        {
            class_symbol->destructor = dtor_symbol.qualified_name;
            class_symbol->method_registry["%dtor"] = dtor_symbol;
        }

        LOG_DEBUG("Collected destructor signature: " + dtor_symbol.qualified_name, "SEMANTIC");
    }

    // ============================================================================
    // Legacy Declaration Analysis (Maintained for compatibility, to be phased out)
    // ============================================================================

    void SemanticAnalyzer::analyze_declarations(std::shared_ptr<AstNode> node)
    {
        // This is now just a dispatcher for legacy external method analysis
        if (!node)
            return;
        if (auto emd = std::dynamic_pointer_cast<ExternalMethodDeclarationNode>(node))
        {
            analyze_declarations(emd);
        }
    }

    void SemanticAnalyzer::analyze_declarations(std::shared_ptr<ExternalMethodDeclarationNode> node)
    {
        if (!node->type.has_value())
        {
            add_error("External method missing return type", node->location);
            return;
        }

        SymbolTable::MethodSymbol extern_symbol;
        extern_symbol.name = node->name->name;
        extern_symbol.qualified_name = node->name->name; // External methods use simple names
        extern_symbol.return_type = node->type.value();
        extern_symbol.parameters = node->parameters;
        extern_symbol.declaration_location = node->location.value_or(SourceLocation{});
        extern_symbol.is_static = true;
        extern_symbol.is_external = true;
        extern_symbol.is_defined = true;

        for (const auto &param : node->parameters)
        {
            if (param->name)
                extern_symbol.parameter_names.push_back(param->name->name);
            if (param->type)
                extern_symbol.parameter_types.push_back(param->type);
        }

        ir->symbol_table.declare_method(extern_symbol);

        LOG_INFO("Registered external method: " + extern_symbol.qualified_name +
                     " with " + std::to_string(extern_symbol.parameter_names.size()) + " parameters",
                 "SEMANTIC");
    }

    // ============================================================================
    // VTable Inheritance Support
    // ============================================================================

    void SemanticAnalyzer::inherit_virtual_methods_from_base_classes()
    {
        LOG_INFO("Inheriting virtual methods from base classes for VTable construction", "SEMANTIC");
        
        // Get all classes
        auto& classes = ir->symbol_table.get_classes();
        
        // Process classes in dependency order (base classes first)
        std::set<std::string> processed;
        for (auto& class_pair : classes)
        {
            if (processed.find(class_pair.first) == processed.end())
            {
                inherit_virtual_methods_recursive(class_pair.first, processed);
            }
        }
    }

    void SemanticAnalyzer::inherit_virtual_methods_recursive(const std::string& class_name, std::set<std::string>& processed)
    {
        // Skip if already processed
        if (processed.find(class_name) != processed.end())
            return;
            
        auto* class_symbol = ir->symbol_table.find_class(class_name);
        if (!class_symbol)
            return;
            
        // If this class has a base class, process the base class first
        if (!class_symbol->base_class.empty())
        {
            inherit_virtual_methods_recursive(class_symbol->base_class, processed);
            
            // Now inherit virtual methods from base class
            auto* base_class_symbol = ir->symbol_table.find_class(class_symbol->base_class);
            if (base_class_symbol)
            {
                // Start with base class virtual method order
                std::vector<std::string> inherited_virtual_methods = base_class_symbol->virtual_method_order;
                
                // For each virtual method in the current class, check if it overrides a base method
                for (const std::string& current_virtual_method : class_symbol->virtual_method_order)
                {
                    // Extract method name from qualified name (ClassName.MethodName)
                    size_t dot_pos = current_virtual_method.find_last_of('.');
                    if (dot_pos == std::string::npos)
                        continue;
                    std::string method_name = current_virtual_method.substr(dot_pos + 1);
                    
                    // Check if this method overrides a base method
                    bool overrides_base_method = false;
                    for (size_t i = 0; i < inherited_virtual_methods.size(); ++i)
                    {
                        // Extract method name from base class qualified name
                        size_t base_dot_pos = inherited_virtual_methods[i].find_last_of('.');
                        if (base_dot_pos == std::string::npos)
                            continue;
                        std::string base_method_name = inherited_virtual_methods[i].substr(base_dot_pos + 1);
                        
                        if (method_name == base_method_name)
                        {
                            // Override: replace base method with derived method in same slot
                            inherited_virtual_methods[i] = current_virtual_method;
                            overrides_base_method = true;
                            LOG_DEBUG("Method " + current_virtual_method + " overrides " + inherited_virtual_methods[i], "SEMANTIC");
                            break;
                        }
                    }
                    
                    // If not an override, add as new virtual method
                    if (!overrides_base_method)
                    {
                        inherited_virtual_methods.push_back(current_virtual_method);
                        LOG_DEBUG("Method " + current_virtual_method + " added as new virtual method", "SEMANTIC");
                    }
                }
                
                // Update the class's virtual method order with inherited + own methods
                class_symbol->virtual_method_order = inherited_virtual_methods;
                
                LOG_INFO("Class " + class_name + " inherited " + 
                        std::to_string(base_class_symbol->virtual_method_order.size()) + 
                        " virtual methods from " + class_symbol->base_class + 
                        ", total virtual methods: " + std::to_string(class_symbol->virtual_method_order.size()), 
                        "SEMANTIC");
                        
                // Log detailed VTable layout
                LOG_DEBUG("Final VTable layout for " + class_name + ":", "SEMANTIC");
                for (size_t i = 0; i < class_symbol->virtual_method_order.size(); ++i)
                {
                    LOG_DEBUG("  [" + std::to_string(i) + "] " + class_symbol->virtual_method_order[i], "SEMANTIC");
                }
            }
        }
        
        processed.insert(class_name);
    }

} // namespace Mycelium::Scripting::Lang