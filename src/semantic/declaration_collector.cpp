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
    TypePtr return_type = typeSystem.get_primitive("void");
    if (node->returnType && node->returnType->name->identifiers.size > 0) {
        return_type = symbolTable.resolve_type_name(node->returnType->get_full_name());
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
            
            Symbol* var_symbol = nullptr;
            
            // Decide whether to create a field or variable based on current context
            if (symbolTable.get_current_type() && !symbolTable.get_current_function()) {
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

void DeclarationCollector::visit(ForInStatementNode* node) {
    // Enter for-in loop scope
    symbolTable.enter_block("for-in");

    // Visit loop variable declaration
    if (node->mainVariable) {
        node->mainVariable->accept(this);
    }

    // Visit iterable expression
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
            std::vector<TypePtr> associated_types;
            for (int j = 0; j < node->cases[i]->associatedData.size; ++j) {
                if (auto* param = node->cases[i]->associatedData[j]) {
                    if (param->type) {
                        auto type = symbolTable.resolve_type_name(param->type->get_full_name());
                        associated_types.push_back(type);
                    }
                }
            }
            
            auto* case_symbol = symbolTable.define_enum_case(case_name, associated_types);
            
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
    TypePtr prop_type = typeSystem.get_unresolved_type();
    if (node->type && node->type->name->identifiers.size > 0) {
        prop_type = symbolTable.resolve_type_name(node->type->get_full_name());
    }
    
    // Properties are fields with special behavior
    auto* prop_symbol = symbolTable.define_property(prop_name, prop_type);
    
    if (!prop_symbol) {
        errors.push_back("Property '" + prop_name + "' already defined in current scope");
    } else {
        
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
    }
    
    // TODO: Handle getter/setter expressions and blocks
}



} // namespace Myre