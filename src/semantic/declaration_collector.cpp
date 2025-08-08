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
    if (!node->body && symbolTable.currentNamespace) {
        errors.push_back("File-scoped namespace cannot be declared inside another namespace");
        return;
    }

    auto ns_symbol = symbolTable.enter_namespace(ns_name);

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
    // For now, just skip
}

void DeclarationCollector::visit(TypeDeclarationNode* node) {
    if (!node->name) return;
    
    std::string type_name(node->name->name);
    std::string full_name = symbolTable.build_qualified_name(type_name);
    
    // Create type definition
    auto type_def = std::make_shared<TypeDefinition>(type_name, full_name);
    
    // Apply modifiers
    for (int i = 0; i < node->modifiers.size; ++i) {
        switch (node->modifiers[i]) {
            case ModifierKind::Static:
                type_def->modifiers |= SymbolModifiers::Static;
                break;
            case ModifierKind::Abstract:
                type_def->modifiers |= SymbolModifiers::Abstract;
                break;
            case ModifierKind::Ref:
                type_def->modifiers |= SymbolModifiers::Ref;
                break;
            default:
                break;
        }
    }
    
    // Register type definition
    typeSystem.register_type_definition(full_name, type_def);
    
    // Enter type scope - this creates the symbol and defines it
    auto type_symbol = symbolTable.enter_type(type_name);
    
    // Apply modifiers to the symbol
    type_symbol->modifiers = type_def->modifiers;
    type_symbol->access = AccessLevel::Public;
    
    // Create type info and store it in the symbol
    TypeInfo type_info;
    type_info.definition = type_def;
    type_info.body_scope = symbolTable.get_current_scope();
    type_symbol->data = type_info;
    
    // Visit members
    for (int i = 0; i < node->members.size; ++i) {
        auto m = node->members[i];
        auto memberType = m->node_type_name();
        if (m) {
            m->accept(this);
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
        if (auto* param = node_cast<ParameterNode>(node->parameters[i])) {
            if (param->type && param->type->name->identifiers.size > 0) {
                TypePtr param_type = symbolTable.resolve_type_name(param->type->get_full_name(), symbolTable.get_current_scope());
                if (param_type) {
                    param_types.push_back(param_type);
                }
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
        return_type = symbolTable.resolve_type_name(node->returnType->get_full_name(), symbolTable.get_current_scope());
    }
    
    // Enter function scope - this creates the symbol and defines it
    auto func_symbol = symbolTable.enter_function(func_name, return_type, param_types);
    
    // Apply modifiers
    for (int i = 0; i < node->modifiers.size; ++i) {
        switch (node->modifiers[i]) {
            case ModifierKind::Static:
                func_symbol->modifiers |= SymbolModifiers::Static;
                break;
            case ModifierKind::Virtual:
                func_symbol->modifiers |= SymbolModifiers::Virtual;
                break;
            case ModifierKind::Override:
                func_symbol->modifiers |= SymbolModifiers::Override;
                break;
            case ModifierKind::Abstract:
                func_symbol->modifiers |= SymbolModifiers::Abstract;
                break;
            default:
                break;
        }
    }
    func_symbol->access = AccessLevel::Public;
    
    // Add parameters to function scope
    for (int i = 0; i < node->parameters.size; ++i)
    {
        TypePtr param_type = param_types[i];
        if (auto* param = node_cast<ParameterNode>(node->parameters[i]))
        {
            if (param->name) {
                std::string param_name(param->name->name);
                auto param_symbol = symbolTable.make_parameter(param_name, param_type, AccessLevel::Private);
                
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
    TypePtr var_type = typeSystem.get_unresolved_type();  // This calls get_unresolved_type() once if node->type is null
    if (node->type && node->type->name->identifiers.size > 0) {
        var_type = symbolTable.resolve_type_name(node->type->get_full_name(), symbolTable.get_current_scope());
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
            
            auto var_symbol = symbolTable.make_var(var_name, var_type, AccessLevel::Private);
            
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



} // namespace Myre