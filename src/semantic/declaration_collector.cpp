#include "semantic/declaration_collector.hpp"
#include <iostream>

namespace Myre {


void DeclarationCollector::collect(CompilationUnitNode* unit) {
    if (unit) {
        unit->accept(this);
    }
}

void DeclarationCollector::visit(CompilationUnitNode* node) {
    // Visit all top-level declarations
    for (int i = 0; i < node->statements.size; ++i) {
        if (node->statements[i]) {
            node->statements[i]->accept(this);
        }
    }
}

void DeclarationCollector::visit(NamespaceDeclarationNode* node)
{
    if (!node->name) return;

    // Enter namespace
    std::string ns_name(node->name->get_full_name());

    // If there is already a current namespace, do not allow file-scoped namespace
    if (!node->body && symbolTable.get_current_namespace() && 
        symbolTable.get_current_namespace()->name() != "global") {
        errors.push_back("File-scoped namespace cannot be declared inside another namespace");
        return;
    }

    symbolTable.enter_namespace(ns_name);

    // If there is a body, visit it; otherwise, treat as file-scoped namespace
    if (node->body) {
        node->body->accept(this);
        symbolTable.exit_scope();
    } else {
        // File-scoped namespace: keep scope open for rest of file
    }
}

void DeclarationCollector::visit(UsingDirectiveNode* node) {
    // TODO: Handle using directives for namespace imports
}

void DeclarationCollector::visit(TypeDeclarationNode* node) {
    if (!node->name) return;
    
    std::string type_name(node->name->name);
    
    // Enter type scope - this creates the symbol
    auto type_symbol = symbolTable.enter_type(type_name);
    
    // Apply modifiers
    for (int i = 0; i < node->modifiers.size; ++i) {
        switch (node->modifiers[i]) {
            case ModifierKind::Static:
                type_symbol->add_modifier(SymbolModifiers::Static);
                break;
            case ModifierKind::Abstract:
                type_symbol->add_modifier(SymbolModifiers::Abstract);
                break;
            case ModifierKind::Ref:
                type_symbol->add_modifier(SymbolModifiers::Ref);
                break;
            default:
                break;
        }
    }
    
    type_symbol->set_access(AccessLevel::Public);
    
    // Visit members
    for (int i = 0; i < node->members.size; ++i) {
        if (node->members[i]) {
            node->members[i]->accept(this);
        }
    }
    
    // Exit type scope
    symbolTable.exit_scope();
}

void DeclarationCollector::visit(FunctionDeclarationNode* node) {
    if (!node->name) return;
    
    std::string func_name(node->name->name);
    
    // Build parameter types
    std::vector<TypePtr> param_types;
    for (int i = 0; i < node->parameters.size; ++i) {
        if (auto* param = node->parameters[i]->as<ParameterNode>()) {
            if (param->type && param->type->name->identifiers.size > 0) {
                TypePtr param_type = symbolTable.resolve_type_name(param->type->get_full_name());
                param_types.push_back(param_type);
            }
            else
            {
                // If no type specified, use unresolved type
                param_types.push_back(typeSystem.get_unresolved_type());
            }
        }
    }
    
    // Get return type
    TypePtr return_type = nullptr;
    if (node->returnType && node->returnType->name->identifiers.size > 0) {
        return_type = symbolTable.resolve_type_name(node->returnType->get_full_name());
    } else {
        // No return type specified - need to infer from body
        return_type = typeSystem.get_unresolved_type();
        if (std::holds_alternative<UnresolvedType>(return_type->value)) {
            auto& unresolved = std::get<UnresolvedType>(return_type->value);
            unresolved.body = node->body;
            unresolved.defining_scope = symbolTable.get_current_scope();
        }
    }
    
    auto func_symbol = symbolTable.enter_function(func_name, return_type, param_types);
    
    // Apply modifiers
    for (int i = 0; i < node->modifiers.size; ++i) {
        switch (node->modifiers[i]) {
            case ModifierKind::Static:
                func_symbol->add_modifier(SymbolModifiers::Static);
                break;
            case ModifierKind::Virtual:
                func_symbol->add_modifier(SymbolModifiers::Virtual);
                break;
            case ModifierKind::Override:
                func_symbol->add_modifier(SymbolModifiers::Override);
                break;
            case ModifierKind::Abstract:
                func_symbol->add_modifier(SymbolModifiers::Abstract);
                break;
            default:
                break;
        }
    }
    func_symbol->set_access(AccessLevel::Public);
    
    // Add parameters to function scope
    for (int i = 0; i < node->parameters.size; ++i)
    {
        TypePtr param_type = param_types[i];
        if (auto* param = node->parameters[i]->as<ParameterNode>())
        {
            if (param->name) {
                std::string param_name(param->name->name);
                auto param_symbol = symbolTable.define_parameter(param_name, param_type);
                
                if (!param_symbol) {
                    errors.push_back("Parameter '" + param_name + "' already defined in function scope");
                }
            }
            else
            {
                errors.push_back("Parameter without name in function '" + func_name + "'");
            }
        }
    }
    
    // Visit function body
    if (node->body) {
        node->body->accept(this);
    }
    
    // Exit function scope
    symbolTable.exit_scope();
}

void DeclarationCollector::visit(VariableDeclarationNode* node)
{
    TypePtr var_type = typeSystem.get_unresolved_type();
    if (node->type && node->type->name->identifiers.size > 0) {
        var_type = symbolTable.resolve_type_name(node->type->get_full_name());
    }
    if (var_type == nullptr) return;
    
    bool needs_inference = std::holds_alternative<UnresolvedType>(var_type->value);
    if (needs_inference) {
        auto& unresolved = std::get<UnresolvedType>(var_type->value);
        unresolved.initializer = node->initializer;
        unresolved.type_name = node->type;
        unresolved.defining_scope = symbolTable.get_current_scope();
    }
    
    for (int i = 0; i < node->names.size; ++i) {
        if (node->names[i]) {
            std::string var_name(node->names[i]->name);
            
            UnscopedSymbol* var_symbol = nullptr;
            
            // Decide whether to create a field or variable based on current context
            // Fields are only created at type level (not inside functions, properties, or blocks)
            if (symbolTable.get_current_type() && 
                !symbolTable.get_current_function() && 
                !symbolTable.get_current_property() &&
                !symbolTable.get_current_scope()->is_block()) {
                var_symbol = symbolTable.define_field(var_name, var_type);
            } else {
                var_symbol = symbolTable.define_variable(var_name, var_type);
            }
            
            if (!var_symbol) {
                errors.push_back("Variable '" + var_name + "' already defined in current scope");
            }
        }
    }
}

void DeclarationCollector::visit(ForStatementNode* node) {
    // Enter for loop scope - encompasses initializer, condition, incrementors, and body
    symbolTable.enter_block("for");

    // Visit initializer (can be a variable declaration or expression statement)
    if (node->initializer) {
        node->initializer->accept(this);
    }

    // Visit condition expression (for any declarations it might contain)
    if (node->condition) {
        node->condition->accept(this);
    }

    // Visit incrementor expressions
    for (int i = 0; i < node->incrementors.size; ++i) {
        if (node->incrementors[i]) {
            node->incrementors[i]->accept(this);
        }
    }

    // Visit loop body
    if (node->body) {
        node->body->accept(this);
    }

    // Exit for loop scope
    symbolTable.exit_scope();
}

void DeclarationCollector::visit(WhileStatementNode* node) {
    // Enter while loop scope - encompasses condition and body
    symbolTable.enter_block("while");

    // Visit condition expression (for any declarations it might contain)
    if (node->condition) {
        node->condition->accept(this);
    }

    // Visit loop body
    if (node->body) {
        node->body->accept(this);
    }

    // Exit while loop scope
    symbolTable.exit_scope();
}

void DeclarationCollector::visit(ForInStatementNode* node) {
    // Enter for-in loop scope
    symbolTable.enter_block("for-in");

    // Special handling for for-in loop variable - need to infer type from iterable
    if (node->mainVariable && node->iterable && node->mainVariable->is_a<VariableDeclarationNode>()) {
        auto* var_decl = node->mainVariable->as<VariableDeclarationNode>();
        
        // Create unresolved type for the loop variable - just use regular variable declaration
        TypePtr var_type = typeSystem.get_unresolved_type();
        if (var_decl->type && var_decl->type->name->identifiers.size > 0) {
            var_type = symbolTable.resolve_type_name(var_decl->type->get_full_name());
        }
        
        // For var declarations without explicit type, leave as unresolved
        // Set up for type inference from the iterable expression
        bool needs_inference = std::holds_alternative<UnresolvedType>(var_type->value);
        if (needs_inference) {
            auto& unresolved = std::get<UnresolvedType>(var_type->value);
            unresolved.type_name = var_decl->type;
            unresolved.defining_scope = symbolTable.get_current_scope();
            // For now, we don't have proper iteration protocol support
            // TODO: Add iteration protocol support for proper for-in loop variable type inference
        }
        
        // Define the loop variable with this special unresolved type
        for (int i = 0; i < var_decl->names.size; ++i) {
            if (var_decl->names[i]) {
                std::string var_name(var_decl->names[i]->name);
                
                UnscopedSymbol* var_symbol = symbolTable.define_variable(var_name, var_type);
                if (!var_symbol) {
                    errors.push_back("For-in loop variable '" + var_name + "' already defined in current scope");
                }
            }
        }
    } else {
        // Fallback to normal variable declaration handling
        if (node->mainVariable) {
            node->mainVariable->accept(this);
        }
    }

    // Visit iterable expression (for any other declarations it might contain)
    if (node->iterable) {
        node->iterable->accept(this);
    }

    // Visit statements in block
    if (node->body) {
        node->body->accept(this);
    }

    // Exit for-in loop scope
    symbolTable.exit_scope();
}

void DeclarationCollector::visit(IfStatementNode* node) {
    // Enter if statement scope
    symbolTable.enter_block("if");

    // Visit condition expression
    if (node->condition) {
        node->condition->accept(this);
    }

    // Visit then block
    if (node->thenStatement) {
        node->thenStatement->accept(this);
    }

    // Visit else block
    if (node->elseStatement) {
        node->elseStatement->accept(this);
    }

    // Exit if statement scope
    symbolTable.exit_scope();
}

void DeclarationCollector::visit(BlockStatementNode* node) {
    // Visit statements in block
    for (int i = 0; i < node->statements.size; ++i) {
        if (node->statements[i]) {
            node->statements[i]->accept(this);
        }
    }
}

void DeclarationCollector::visit(EnumDeclarationNode* node) {
    if (!node->name) return;
    
    std::string enum_name(node->name->name);
    
    auto* enum_symbol = symbolTable.enter_enum(enum_name);
    
    // Apply modifiers if any
    for (int i = 0; i < node->modifiers.size; ++i) {
        switch (node->modifiers[i]) {
            case ModifierKind::Static:
                enum_symbol->add_modifier(SymbolModifiers::Static);
                break;
            case ModifierKind::Abstract:
                enum_symbol->add_modifier(SymbolModifiers::Abstract);
                break;
            default:
                break;
        }
    }
    
    enum_symbol->set_access(AccessLevel::Public);
    
    // Visit enum cases
    for (int i = 0; i < node->cases.size; ++i) {
        if (node->cases[i] && node->cases[i]->name) {
            std::string case_name(node->cases[i]->name->name);
            
            // Build associated types for tagged enum cases
            std::vector<TypePtr> params;
            for (int j = 0; j < node->cases[i]->associatedData.size; ++j) {
                if (auto* param = node->cases[i]->associatedData[j]) {
                    if (param->type) {
                        auto type = symbolTable.resolve_type_name(param->type->get_full_name());
                        params.push_back(type);
                    }
                }
            }
            
            auto* case_symbol = symbolTable.define_enum_case(case_name, params);
            
            if (!case_symbol) {
                errors.push_back("Enum case '" + case_name + "' already defined in enum '" + enum_name + "'");
            }
        }
    }
    
    // Visit methods
    for (int i = 0; i < node->methods.size; ++i) {
        if (node->methods[i]) {
            node->methods[i]->accept(this);
        }
    }
    
    // Exit enum scope
    symbolTable.exit_scope();
}

void DeclarationCollector::visit(PropertyDeclarationNode* node) {
    if (!node->name) return;
    
    std::string prop_name(node->name->name);
    
    // Determine property type
    TypePtr prop_type = nullptr;
    if (node->type && node->type->name->identifiers.size > 0) {
        prop_type = symbolTable.resolve_type_name(node->type->get_full_name());
    } else {
        // No explicit type - need to infer (type resolver will handle this)
        prop_type = typeSystem.get_unresolved_type();
        if (std::holds_alternative<UnresolvedType>(prop_type->value)) {
            auto& unresolved = std::get<UnresolvedType>(prop_type->value);
            unresolved.type_name = node->type;
            // We'll set defining_scope after we create and enter the property scope
            
            // Set up type inference sources for type resolver
            if (node->getterExpression) {
                // Arrow property: var prop => expr
                unresolved.initializer = node->getterExpression;
            } else if (node->initializer && node->accessors.size == 0) {
                // Simple property with initializer: var prop = value
                unresolved.initializer = node->initializer;
            } else if (node->accessors.size > 0) {
                // Find getter accessor and set up for type inference
                // We'll set the body and specific accessor scope after we create the accessor scopes
                for (int i = 0; i < node->accessors.size; ++i) {
                    if (node->accessors[i] && node->accessors[i]->accessorKeyword) {
                        std::string accessor_kind(node->accessors[i]->accessorKeyword->text);
                        if (accessor_kind == "get") {
                            if (node->accessors[i]->expression) {
                                // Arrow-style getter: get => expression
                                unresolved.initializer = node->accessors[i]->expression;
                            } else if (node->accessors[i]->body) {
                                // Block-style getter: get { return expression; }
                                unresolved.body = node->accessors[i]->body;
                            }
                            break;
                        }
                    }
                }
            }
        }
    }
    
    // Create property symbol with its own scope (all properties are scoped)
    auto* prop_symbol = symbolTable.enter_property(prop_name, prop_type);
    
    if (!prop_symbol) {
        errors.push_back("Property '" + prop_name + "' already defined in current scope");
        return;
    }
    
    // Now that we're in the property scope, set the defining_scope for type inference
    if (std::holds_alternative<UnresolvedType>(prop_type->value)) {
        auto& unresolved = std::get<UnresolvedType>(prop_type->value);
        unresolved.defining_scope = prop_symbol; // Property scope for type resolution
    }
    
    // Apply modifiers
    for (int i = 0; i < node->modifiers.size; ++i) {
        switch (node->modifiers[i]) {
            case ModifierKind::Static:
                prop_symbol->add_modifier(SymbolModifiers::Static);
                break;
            case ModifierKind::Virtual:
                prop_symbol->add_modifier(SymbolModifiers::Virtual);
                break;
            case ModifierKind::Override:
                prop_symbol->add_modifier(SymbolModifiers::Override);
                break;
            default:
                break;
        }
    }
    prop_symbol->set_access(AccessLevel::Public);
    
    // Create backing field if property has initializer
    if (node->initializer) {
        TypePtr field_type = typeSystem.get_unresolved_type();
        if (std::holds_alternative<UnresolvedType>(field_type->value)) {
            auto& field_unresolved = std::get<UnresolvedType>(field_type->value);
            field_unresolved.initializer = node->initializer;
            field_unresolved.defining_scope = prop_symbol; // Field is in property scope
        }
        
        std::string field_name = "field";
        auto* field_symbol = symbolTable.define_field(field_name, field_type);
        if (!field_symbol) {
            errors.push_back("Could not create backing field for property '" + prop_name + "'");
        }
    }
    
    // Process accessors - each accessor gets its own direct scope within property
    for (int i = 0; i < node->accessors.size; ++i) {
        if (node->accessors[i] && node->accessors[i]->accessorKeyword) {
            std::string accessor_kind(node->accessors[i]->accessorKeyword->text);
            
            // Create accessor as a direct child scope (not anonymous block)
            // For now, use enter_block but with a meaningful name that represents the accessor
            auto* accessor_scope = symbolTable.enter_block(accessor_kind + "-accessor");
            
            // If this is the getter with a body, update the UnresolvedType to use this specific accessor scope
            if (accessor_kind == "get" && node->accessors[i]->body && 
                std::holds_alternative<UnresolvedType>(prop_type->value)) {
                auto& unresolved = std::get<UnresolvedType>(prop_type->value);
                if (unresolved.body == node->accessors[i]->body) {
                    unresolved.defining_scope = accessor_scope; // Use accessor scope, not property scope
                }
            }
            
            if (accessor_kind == "set") {
                // Add 'value' parameter for setter
                TypePtr value_type = prop_type; // Setter value should match property type
                auto* value_symbol = symbolTable.define_parameter("value", value_type);
                if (!value_symbol) {
                    errors.push_back("Could not create 'value' parameter for setter of property '" + prop_name + "'");
                }
            }
            
            // Visit accessor body to collect any declarations inside
            if (node->accessors[i]->body) {
                node->accessors[i]->body->accept(this);
            }
            
            // Exit accessor scope
            symbolTable.exit_scope();
        }
    }
    
    // Exit property scope
    symbolTable.exit_scope();
}



} // namespace Myre