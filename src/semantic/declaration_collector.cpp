#include "semantic/declaration_collector.hpp"
#include <iostream>

namespace Myre
{

    SymbolHandle DeclarationCollector::get_current_handle()
    {
        auto h = symbolTable.get_current_scope()->handle;
        assert(h.id != 0 && "Current scope must have a valid handle");
        return h;
    }

    void DeclarationCollector::collect(CompilationUnitNode *unit)
    {
        if (unit)
        {
            unit->accept(this);
        }
    }

    void DeclarationCollector::visit(CompilationUnitNode *node)
    {
        symbolTable.enter_namespace("global");
        // Annotate with current scope (global)
        node->containingScope = get_current_handle();

        // Visit all top-level declarations
        for (int i = 0; i < node->statements.size; ++i)
        {
            if (node->statements[i])
            {
                node->statements[i]->accept(this);
            }
        }
        symbolTable.exit_scope();
    }

    void DeclarationCollector::visit(NamespaceDeclarationNode *node)
    {
        // Annotate with current scope BEFORE entering namespace
        node->containingScope = get_current_handle();

        if (!node->name)
            return;

        // Visit the namespace name
        if (node->name)
        {
            node->name->containingScope = node->containingScope;
            node->name->accept(this);
        }

        // Enter namespace
        std::string ns_name(node->name->get_full_name());

        // If there is already a current namespace, do not allow file-scoped namespace
        if (!node->body && symbolTable.get_current_namespace() && symbolTable.get_current_namespace() != symbolTable.get_global_namespace())
        {
            errors.push_back("File-scoped namespace cannot be declared inside another namespace");
            return;
        }

        auto *ns_symbol = symbolTable.enter_namespace(ns_name);

        if (node->body)
        {
            // The body gets the NEW namespace scope
            node->body->containingScope = get_current_handle();
            visit_block_contents(node->body);
            symbolTable.exit_scope();
        }
    }

    void DeclarationCollector::visit(UsingDirectiveNode *node)
    {
        node->containingScope = get_current_handle();

        // Visit the namespace name
        if (node->namespaceName)
        {
            node->namespaceName->containingScope = node->containingScope;
            node->namespaceName->accept(this);
        }
    }

    void DeclarationCollector::visit(TypeDeclarationNode *node)
    {
        // Annotate with current scope BEFORE entering type
        node->containingScope = get_current_handle();

        if (!node->name)
            return;

        // Visit the type name identifier
        if (node->name)
        {
            node->name->containingScope = node->containingScope;
            node->name->accept(this);
        }

        std::string type_name(node->name->name);

        // Enter type scope - this creates the symbol
        auto *type_symbol = symbolTable.enter_type(type_name);

        // Apply modifiers
        for (int i = 0; i < node->modifiers.size; ++i)
        {
            switch (node->modifiers[i])
            {
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

        // Visit members - they will get the type's scope
        for (int i = 0; i < node->members.size; ++i)
        {
            if (node->members[i])
            {
                node->members[i]->accept(this);
            }
        }

        // Exit type scope
        symbolTable.exit_scope();
    }

    void DeclarationCollector::visit(InterfaceDeclarationNode *node)
    {
        // Annotate with current scope
        node->containingScope = get_current_handle();

        if (!node->name)
            return;

        // Visit the interface name
        if (node->name)
        {
            node->name->containingScope = node->containingScope;
            node->name->accept(this);
        }

        // TODO: Implement interface symbol creation
        // For now, just visit members with current scope
        for (int i = 0; i < node->members.size; ++i)
        {
            if (node->members[i])
            {
                node->members[i]->accept(this);
            }
        }
    }

    void DeclarationCollector::visit(FunctionDeclarationNode *node)
    {
        // Annotate with current scope BEFORE entering function
        node->containingScope = get_current_handle();

        if (!node->name)
            return;

        // Visit function name
        if (node->name)
        {
            node->name->containingScope = node->containingScope;
            node->name->accept(this);
        }

        std::string func_name(node->name->name);

        // Build parameter types (but also need to visit the type nodes!)
        std::vector<TypePtr> param_types;
        for (int i = 0; i < node->parameters.size; ++i)
        {
            if (auto *param = node->parameters[i]->as<ParameterNode>())
            {
                if (param->type && param->type->name->identifiers.size > 0)
                {
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
        if (node->returnType && node->returnType->name->identifiers.size > 0)
        {
            return_type = symbolTable.resolve_type_name(node->returnType->get_full_name());
        }
        else
        {
            // No return type specified - need to infer from body
            return_type = typeSystem.get_unresolved_type();
            if (std::holds_alternative<UnresolvedType>(return_type->value))
            {
                auto &unresolved = std::get<UnresolvedType>(return_type->value);
                unresolved.body = node->body;
                unresolved.definingScope = get_current_handle();
            }
        }

        // Visit return type node BEFORE entering function scope
        if (node->returnType)
        {
            node->returnType->containingScope = node->containingScope;
            node->returnType->accept(this);
        }

        auto *func_symbol = symbolTable.enter_function(func_name, return_type, param_types);

        // Apply modifiers
        for (int i = 0; i < node->modifiers.size; ++i)
        {
            switch (node->modifiers[i])
            {
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

        // Visit and add parameters to function scope
        for (int i = 0; i < node->parameters.size; ++i)
        {
            if (auto *param = node->parameters[i]->as<ParameterNode>())
            {
                // Annotate parameter with function scope
                param->containingScope = get_current_handle();

                TypePtr param_type = param_types[i];
                if (param->name)
                {
                    std::string param_name(param->name->name);
                    auto *param_symbol = symbolTable.define_parameter(param_name, param_type);

                    if (!param_symbol)
                    {
                        errors.push_back("Parameter '" + param_name + "' already defined in function scope");
                    }
                }
                else
                {
                    errors.push_back("Parameter without name in function '" + func_name + "'");
                }
            }
            // Visit the parameter node to handle its children
            node->parameters[i]->accept(this);
        }

        // Visit function body - it gets the function scope
        if (node->body)
        {
            node->body->containingScope = get_current_handle();
            visit_block_contents(node->body);
        }

        // Exit function scope
        symbolTable.exit_scope();
    }

    void DeclarationCollector::visit(ParameterNode *node)
    {
        // Node itself already has containingScope set by parent

        // Visit parameter name
        if (node->name)
        {
            node->name->containingScope = node->containingScope;
            node->name->accept(this);
        }

        // Visit parameter type
        if (node->type)
        {
            node->type->containingScope = node->containingScope;
            node->type->accept(this);
        }

        // Visit default value
        if (node->defaultValue)
        {
            node->defaultValue->containingScope = node->containingScope;
            node->defaultValue->accept(this);
        }
    }

    void DeclarationCollector::visit(VariableDeclarationNode *node)
    {
        // Annotate with current scope
        node->containingScope = get_current_handle();

        // Visit the type node
        if (node->type)
        {
            node->type->containingScope = node->containingScope;
            node->type->accept(this);
        }

        TypePtr var_type = typeSystem.get_unresolved_type();
        if (node->type && node->type->name->identifiers.size > 0)
        {
            var_type = symbolTable.resolve_type_name(node->type->get_full_name());
        }
        if (var_type == nullptr)
            return;

        bool needs_inference = std::holds_alternative<UnresolvedType>(var_type->value);
        if (needs_inference)
        {
            auto &unresolved = std::get<UnresolvedType>(var_type->value);
            unresolved.initializer = node->initializer;
            unresolved.typeName = node->type;
            unresolved.definingScope = get_current_handle();
        }

        // Visit initializer
        if (node->initializer)
        {
            node->initializer->containingScope = node->containingScope;
            node->initializer->accept(this);
        }

        // Visit variable names
        for (int i = 0; i < node->names.size; ++i)
        {
            if (node->names[i])
            {
                node->names[i]->containingScope = node->containingScope;
                node->names[i]->accept(this);

                std::string var_name(node->names[i]->name);

                UnscopedSymbol *var_symbol = nullptr;

                // Decide whether to create a field or variable based on current context
                if (symbolTable.get_current_type() &&
                    !symbolTable.get_current_function() &&
                    !symbolTable.get_current_property() &&
                    !symbolTable.get_current_scope()->is<BlockScope>())
                {
                    var_symbol = symbolTable.define_field(var_name, var_type);
                }
                else
                {
                    var_symbol = symbolTable.define_variable(var_name, var_type);
                }

                if (!var_symbol)
                {
                    errors.push_back("Variable '" + var_name + "' already defined in current scope");
                }
            }
        }
    }

    void DeclarationCollector::visit(ConstructorDeclarationNode *node)
    {
        // Annotate with current scope
        node->containingScope = get_current_handle();

        // TODO: Implement constructor symbol creation
        // For now, just visit parameters and body

        for (int i = 0; i < node->parameters.size; ++i)
        {
            if (node->parameters[i])
            {
                node->parameters[i]->containingScope = node->containingScope;
                node->parameters[i]->accept(this);
            }
        }

        if (node->body)
        {
            node->body->containingScope = node->containingScope;
            node->body->accept(this);
        }
    }

    // === Type Name Visitors ===

    void DeclarationCollector::visit(TypeNameNode *node)
    {
        // Already has containingScope set by parent

        // Visit the qualified name
        if (node->name)
        {
            node->name->containingScope = node->containingScope;
            node->name->accept(this);
        }
    }

    void DeclarationCollector::visit(ArrayTypeNameNode *node)
    {
        // Already has containingScope set by parent

        // Visit element type
        if (node->elementType)
        {
            node->elementType->containingScope = node->containingScope;
            node->elementType->accept(this);
        }
    }

    void DeclarationCollector::visit(GenericTypeNameNode *node)
    {
        // Already has containingScope set by parent

        // Visit base type
        if (node->baseType)
        {
            node->baseType->containingScope = node->containingScope;
            node->baseType->accept(this);
        }

        // Visit type arguments
        for (int i = 0; i < node->arguments.size; ++i)
        {
            if (node->arguments[i])
            {
                node->arguments[i]->containingScope = node->containingScope;
                node->arguments[i]->accept(this);
            }
        }
    }

    void DeclarationCollector::visit(QualifiedNameNode *node)
    {
        // Already has containingScope set by parent

        // Visit each identifier in the qualified name
        for (int i = 0; i < node->identifiers.size; ++i)
        {
            if (node->identifiers[i])
            {
                node->identifiers[i]->containingScope = node->containingScope;
                node->identifiers[i]->accept(this);
            }
        }
    }

    // === Statement Visitors ===

    void DeclarationCollector::visit(EmptyStatementNode *node)
    {
        node->containingScope = get_current_handle();
    }

    void DeclarationCollector::visit(ForStatementNode *node)
    {
        // Annotate with current scope BEFORE entering for scope
        node->containingScope = get_current_handle();

        // Enter for loop scope
        auto *for_scope = symbolTable.enter_block("for");

        // Visit initializer - gets the for scope
        if (node->initializer)
        {
            node->initializer->containingScope = get_current_handle();
            node->initializer->accept(this);
        }

        // Visit condition - gets the for scope
        if (node->condition)
        {
            node->condition->containingScope = get_current_handle();
            node->condition->accept(this);
        }

        // Visit incrementors - get the for scope
        for (int i = 0; i < node->incrementors.size; ++i)
        {
            if (node->incrementors[i])
            {
                node->incrementors[i]->containingScope = get_current_handle();
                node->incrementors[i]->accept(this);
            }
        }

        // Visit loop body
        if (node->body)
        {
            if (node->body->is_a<BlockStatementNode>())
            {
                node->body->containingScope = get_current_handle();
                visit_block_contents(node->body->as<BlockStatementNode>());
            }
            else
            {
                node->body->containingScope = get_current_handle();
                node->body->accept(this);
            }
        }

        // Exit for loop scope
        symbolTable.exit_scope();
    }

    void DeclarationCollector::visit(WhileStatementNode *node)
    {
        // Annotate with current scope BEFORE entering while scope
        node->containingScope = get_current_handle();

        // Enter while loop scope
        auto *while_scope = symbolTable.enter_block("while");

        // Visit condition
        if (node->condition)
        {
            node->condition->containingScope = get_current_handle();
            node->condition->accept(this);
        }

        // Visit loop body
        if (node->body)
        {
            if (node->body->is_a<BlockStatementNode>())
            {
                node->body->containingScope = get_current_handle();
                visit_block_contents(node->body->as<BlockStatementNode>());
            }
            else
            {
                node->body->containingScope = get_current_handle();
                node->body->accept(this);
            }
        }

        // Exit while loop scope
        symbolTable.exit_scope();
    }

    void DeclarationCollector::visit(ForInStatementNode *node)
    {
        // Annotate with current scope BEFORE entering for-in scope
        node->containingScope = get_current_handle();

        // Enter for-in loop scope
        auto *forin_scope = symbolTable.enter_block("for-in");

        // Handle loop variable
        if (node->mainVariable && node->iterable && node->mainVariable->is_a<VariableDeclarationNode>())
        {
            auto *var_decl = node->mainVariable->as<VariableDeclarationNode>();
            var_decl->containingScope = get_current_handle();

            TypePtr var_type = typeSystem.get_unresolved_type();
            if (var_decl->type && var_decl->type->name->identifiers.size > 0)
            {
                var_type = symbolTable.resolve_type_name(var_decl->type->get_full_name());
            }

            bool needs_inference = std::holds_alternative<UnresolvedType>(var_type->value);
            if (needs_inference)
            {
                auto &unresolved = std::get<UnresolvedType>(var_type->value);
                unresolved.typeName = var_decl->type;
                unresolved.definingScope = get_current_handle();
            }

            for (int i = 0; i < var_decl->names.size; ++i)
            {
                if (var_decl->names[i])
                {
                    std::string var_name(var_decl->names[i]->name);

                    UnscopedSymbol *var_symbol = symbolTable.define_variable(var_name, var_type);
                    if (!var_symbol)
                    {
                        errors.push_back("For-in loop variable '" + var_name + "' already defined in current scope");
                    }
                }
            }
        }
        else if (node->mainVariable)
        {
            node->mainVariable->containingScope = get_current_handle();
            node->mainVariable->accept(this);
        }

        // Visit iterable
        if (node->iterable)
        {
            node->iterable->containingScope = get_current_handle();
            node->iterable->accept(this);
        }

        // Visit index variable if present
        if (node->indexVariable)
        {
            node->indexVariable->containingScope = get_current_handle();
            node->indexVariable->accept(this);
        }

        // Visit body
        if (node->body)
        {
            if (node->body->is_a<BlockStatementNode>())
            {
                node->body->containingScope = get_current_handle();
                visit_block_contents(node->body->as<BlockStatementNode>());
            }
            else
            {
                node->body->containingScope = get_current_handle();
                node->body->accept(this);
            }
        }

        // Exit for-in loop scope
        symbolTable.exit_scope();
    }

    void DeclarationCollector::visit(IfStatementNode *node)
    {
        // Annotate with current scope BEFORE entering if scope
        node->containingScope = get_current_handle();

        // Enter if statement scope
        auto *if_scope = symbolTable.enter_block("if");

        // Visit condition
        if (node->condition)
        {
            node->condition->containingScope = get_current_handle();
            node->condition->accept(this);
        }

        // Visit then block
        if (node->thenStatement)
        {
            if (node->thenStatement->is_a<BlockStatementNode>())
            {
                node->thenStatement->containingScope = get_current_handle();
                visit_block_contents(node->thenStatement->as<BlockStatementNode>());
            }
            else
            {
                node->thenStatement->containingScope = get_current_handle();
                node->thenStatement->accept(this);
            }
        }

        // Visit else block
        if (node->elseStatement)
        {
            if (node->elseStatement->is_a<BlockStatementNode>())
            {
                node->elseStatement->containingScope = get_current_handle();
                visit_block_contents(node->elseStatement->as<BlockStatementNode>());
            }
            else
            {
                node->elseStatement->containingScope = get_current_handle();
                node->elseStatement->accept(this);
            }
        }

        // Exit if statement scope
        symbolTable.exit_scope();
    }

    void DeclarationCollector::visit(BlockStatementNode *node)
    {
        // Annotate with current scope BEFORE entering block scope
        node->containingScope = get_current_handle();

        // Create scope for anonymous blocks
        auto *block_scope = symbolTable.enter_block("block");

        // Visit statements in block - they get the new block scope
        for (int i = 0; i < node->statements.size; ++i)
        {
            if (node->statements[i])
            {
                // First set the scope, then visit
                node->statements[i]->containingScope = get_current_handle();
                node->statements[i]->accept(this);
            }
        }

        // Exit block scope
        symbolTable.exit_scope();
    }

    void DeclarationCollector::visit_block_contents(BlockStatementNode *node)
    {
        // Helper method to visit block contents without creating a scope
        // The block already has its containingScope set by the caller
        if (!node)
            return;

        // Current scope should already be set correctly by caller
        SymbolHandle current = get_current_handle();

        for (int i = 0; i < node->statements.size; ++i)
        {
            if (node->statements[i])
            {
                // Annotate each statement with current scope
                node->statements[i]->containingScope = current;
                node->statements[i]->accept(this);
            }
        }
    }

    void DeclarationCollector::visit(EnumDeclarationNode *node)
    {
        // Annotate with current scope BEFORE entering enum
        node->containingScope = get_current_handle();

        if (!node->name)
            return;

        // Visit enum name
        if (node->name)
        {
            node->name->containingScope = node->containingScope;
            node->name->accept(this);
        }

        std::string enum_name(node->name->name);

        auto *enum_symbol = symbolTable.enter_enum(enum_name);

        // Apply modifiers
        for (int i = 0; i < node->modifiers.size; ++i)
        {
            switch (node->modifiers[i])
            {
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

        // Visit enum cases - they get the enum scope
        for (int i = 0; i < node->cases.size; ++i)
        {
            if (node->cases[i])
            {
                node->cases[i]->containingScope = get_current_handle();
                // Visit the case node (it will handle its children)
                node->cases[i]->accept(this);

                if (node->cases[i]->name)
                {
                    std::string case_name(node->cases[i]->name->name);

                    // Build associated types for tagged enum cases
                    std::vector<TypePtr> params;
                    for (int j = 0; j < node->cases[i]->associatedData.size; ++j)
                    {
                        if (auto *param = node->cases[i]->associatedData[j])
                        {
                            if (param->type)
                            {
                                auto type = symbolTable.resolve_type_name(param->type->get_full_name());
                                params.push_back(type);
                            }
                        }
                    }

                    auto *case_symbol = symbolTable.define_enum_case(case_name, params);

                    if (!case_symbol)
                    {
                        errors.push_back("Enum case '" + case_name + "' already defined in enum '" + enum_name + "'");
                    }
                }
            }
        }

        // Visit methods - they get the enum scope initially
        for (int i = 0; i < node->methods.size; ++i)
        {
            if (node->methods[i])
            {
                node->methods[i]->accept(this);
            }
        }

        // Exit enum scope
        symbolTable.exit_scope();
    }

    void DeclarationCollector::visit(EnumCaseNode *node)
    {
        // Already has containingScope set by parent

        // Visit case name
        if (node->name)
        {
            node->name->containingScope = node->containingScope;
            node->name->accept(this);
        }

        // Visit associated data parameters
        for (int i = 0; i < node->associatedData.size; ++i)
        {
            if (node->associatedData[i])
            {
                node->associatedData[i]->containingScope = node->containingScope;
                node->associatedData[i]->accept(this);
            }
        }
    }

    void DeclarationCollector::visit(PropertyDeclarationNode *node)
    {
        // Annotate with current scope BEFORE entering property
        node->containingScope = get_current_handle();

        if (!node->name)
            return;

        // Visit property name
        if (node->name)
        {
            node->name->containingScope = node->containingScope;
            node->name->accept(this);
        }

        // Visit property type
        if (node->type)
        {
            node->type->containingScope = node->containingScope;
            node->type->accept(this);
        }

        std::string prop_name(node->name->name);

        // Determine property type
        TypePtr prop_type = nullptr;
        if (node->type && node->type->name->identifiers.size > 0)
        {
            prop_type = symbolTable.resolve_type_name(node->type->get_full_name());
        }
        else
        {
            // No explicit type - need to infer
            prop_type = typeSystem.get_unresolved_type();
            if (std::holds_alternative<UnresolvedType>(prop_type->value))
            {
                auto &unresolved = std::get<UnresolvedType>(prop_type->value);
                unresolved.typeName = node->type;

                // Set up type inference sources
                if (node->getterExpression)
                {
                    unresolved.initializer = node->getterExpression;
                }
                else if (node->initializer && node->accessors.size == 0)
                {
                    unresolved.initializer = node->initializer;
                }
                else if (node->accessors.size > 0)
                {
                    for (int i = 0; i < node->accessors.size; ++i)
                    {
                        if (node->accessors[i] && node->accessors[i]->accessorKeyword)
                        {
                            std::string accessor_kind(node->accessors[i]->accessorKeyword->text);
                            if (accessor_kind == "get")
                            {
                                if (node->accessors[i]->expression)
                                {
                                    unresolved.initializer = node->accessors[i]->expression;
                                }
                                else if (node->accessors[i]->body)
                                {
                                    unresolved.body = node->accessors[i]->body;
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }

        // Create property symbol with its own scope
        auto *prop_symbol = symbolTable.enter_property(prop_name, prop_type);

        if (!prop_symbol)
        {
            errors.push_back("Property '" + prop_name + "' already defined in current scope");
            return;
        }

        // Now set definingScope for type inference
        if (std::holds_alternative<UnresolvedType>(prop_type->value))
        {
            auto &unresolved = std::get<UnresolvedType>(prop_type->value);
            unresolved.definingScope = get_current_handle();
        }

        // Apply modifiers
        for (int i = 0; i < node->modifiers.size; ++i)
        {
            switch (node->modifiers[i])
            {
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

        // Visit getter expression if present (arrow syntax)
        if (node->getterExpression)
        {
            node->getterExpression->containingScope = get_current_handle();
            node->getterExpression->accept(this);
        }

        // Create backing field if property has initializer
        if (node->initializer)
        {
            node->initializer->containingScope = get_current_handle();
            node->initializer->accept(this);

            TypePtr field_type = typeSystem.get_unresolved_type();
            if (std::holds_alternative<UnresolvedType>(field_type->value))
            {
                auto &field_unresolved = std::get<UnresolvedType>(field_type->value);
                field_unresolved.initializer = node->initializer;
                field_unresolved.definingScope = get_current_handle();
            }

            std::string field_name = "field";
            auto *field_symbol = symbolTable.define_field(field_name, field_type);
            if (!field_symbol)
            {
                errors.push_back("Could not create backing field for property '" + prop_name + "'");
            }
        }

        // Process accessors
        for (int i = 0; i < node->accessors.size; ++i)
        {
            if (node->accessors[i])
            {
                // Visit the accessor node itself
                node->accessors[i]->accept(this);
            }
        }

        // Exit property scope
        symbolTable.exit_scope();
    }

    void DeclarationCollector::visit(PropertyAccessorNode *node)
    {
        // Already has containingScope set by parent (property)
        node->containingScope = get_current_handle();

        if (node->accessorKeyword)
        {
            std::string accessor_kind(node->accessorKeyword->text);

            auto *accessor_scope = symbolTable.enter_block(accessor_kind + "-accessor");

            if (accessor_kind == "set")
            {
                // Add 'value' parameter for setter
                // Type should match property type - get from parent property
                TypePtr value_type = typeSystem.get_unresolved_type(); // TODO: Get from property
                auto *value_symbol = symbolTable.define_parameter("value", value_type);
                if (!value_symbol)
                {
                    errors.push_back("Could not create 'value' parameter for setter");
                }
            }

            if (node->body)
            {
                node->body->containingScope = get_current_handle();
                visit_block_contents(node->body);
            }

            if (node->expression)
            {
                node->expression->containingScope = get_current_handle();
                node->expression->accept(this);
            }

            // Exit accessor scope
            symbolTable.exit_scope();
        }
    }

    // === Expression Visitors ===

    void DeclarationCollector::visit(ExpressionStatementNode *node)
    {
        node->containingScope = get_current_handle();
        if (node->expression)
        {
            node->expression->containingScope = node->containingScope;
            node->expression->accept(this);
        }
    }

    void DeclarationCollector::visit(ReturnStatementNode *node)
    {
        node->containingScope = get_current_handle();
        if (node->expression)
        {
            node->expression->containingScope = node->containingScope;
            node->expression->accept(this);
        }
    }

    void DeclarationCollector::visit(BreakStatementNode *node)
    {
        node->containingScope = get_current_handle();
    }

    void DeclarationCollector::visit(ContinueStatementNode *node)
    {
        node->containingScope = get_current_handle();
    }

    void DeclarationCollector::visit(BinaryExpressionNode *node)
    {
        node->containingScope = get_current_handle();
        if (node->left)
        {
            node->left->containingScope = node->containingScope;
            node->left->accept(this);
        }
        if (node->right)
        {
            node->right->containingScope = node->containingScope;
            node->right->accept(this);
        }
    }

    void DeclarationCollector::visit(UnaryExpressionNode *node)
    {
        node->containingScope = get_current_handle();
        if (node->operand)
        {
            node->operand->containingScope = node->containingScope;
            node->operand->accept(this);
        }
    }

    void DeclarationCollector::visit(CallExpressionNode *node)
    {
        node->containingScope = get_current_handle();
        if (node->target)
        {
            node->target->containingScope = node->containingScope;
            node->target->accept(this);
        }
        for (int i = 0; i < node->arguments.size; ++i)
        {
            if (node->arguments[i])
            {
                node->arguments[i]->containingScope = node->containingScope;
                node->arguments[i]->accept(this);
            }
        }
    }

    void DeclarationCollector::visit(MemberAccessExpressionNode *node)
    {
        node->containingScope = get_current_handle();
        if (node->target)
        {
            node->target->containingScope = node->containingScope;
            node->target->accept(this);
        }
        if (node->member)
        {
            node->member->containingScope = node->containingScope;
            node->member->accept(this);
        }
    }

    void DeclarationCollector::visit(AssignmentExpressionNode *node)
    {
        node->containingScope = get_current_handle();
        if (node->target)
        {
            node->target->containingScope = node->containingScope;
            node->target->accept(this);
        }
        if (node->source)
        {
            node->source->containingScope = node->containingScope;
            node->source->accept(this);
        }
    }

    void DeclarationCollector::visit(IdentifierExpressionNode *node)
    {
        node->containingScope = get_current_handle();
        if (node->identifier)
        {
            node->identifier->containingScope = node->containingScope;
            node->identifier->accept(this);
        }
    }

    void DeclarationCollector::visit(LiteralExpressionNode *node)
    {
        node->containingScope = get_current_handle();
        if (node->token)
        {
            node->token->containingScope = node->containingScope;
        }
    }

    void DeclarationCollector::visit(ParenthesizedExpressionNode *node)
    {
        node->containingScope = get_current_handle();
        if (node->expression)
        {
            node->expression->containingScope = node->containingScope;
            node->expression->accept(this);
        }
    }

    void DeclarationCollector::visit(NewExpressionNode *node)
    {
        node->containingScope = get_current_handle();
        if (node->type)
        {
            node->type->containingScope = node->containingScope;
            node->type->accept(this);
        }
        if (node->constructorCall)
        {
            node->constructorCall->containingScope = node->containingScope;
            node->constructorCall->accept(this);
        }
    }

    void DeclarationCollector::visit(ThisExpressionNode *node)
    {
        node->containingScope = get_current_handle();
    }

    void DeclarationCollector::visit(CastExpressionNode *node)
    {
        node->containingScope = get_current_handle();
        if (node->targetType)
        {
            node->targetType->containingScope = node->containingScope;
            node->targetType->accept(this);
        }
        if (node->expression)
        {
            node->expression->containingScope = node->containingScope;
            node->expression->accept(this);
        }
    }

    void DeclarationCollector::visit(IndexerExpressionNode *node)
    {
        node->containingScope = get_current_handle();
        if (node->target)
        {
            node->target->containingScope = node->containingScope;
            node->target->accept(this);
        }
        if (node->index)
        {
            node->index->containingScope = node->containingScope;
            node->index->accept(this);
        }
    }

    void DeclarationCollector::visit(TypeOfExpressionNode *node)
    {
        node->containingScope = get_current_handle();
        if (node->type)
        {
            node->type->containingScope = node->containingScope;
            node->type->accept(this);
        }
    }

    void DeclarationCollector::visit(SizeOfExpressionNode *node)
    {
        node->containingScope = get_current_handle();
        if (node->type)
        {
            node->type->containingScope = node->containingScope;
            node->type->accept(this);
        }
    }

    void DeclarationCollector::visit(ConditionalExpressionNode *node)
    {
        node->containingScope = get_current_handle();
        if (node->condition)
        {
            node->condition->containingScope = node->containingScope;
            node->condition->accept(this);
        }
        if (node->whenTrue)
        {
            node->whenTrue->containingScope = node->containingScope;
            node->whenTrue->accept(this);
        }
        if (node->whenFalse)
        {
            node->whenFalse->containingScope = node->containingScope;
            node->whenFalse->accept(this);
        }
    }

    void DeclarationCollector::visit(RangeExpressionNode *node)
    {
        node->containingScope = get_current_handle();
        if (node->start)
        {
            node->start->containingScope = node->containingScope;
            node->start->accept(this);
        }
        if (node->end)
        {
            node->end->containingScope = node->containingScope;
            node->end->accept(this);
        }
        if (node->stepExpression)
        {
            node->stepExpression->containingScope = node->containingScope;
            node->stepExpression->accept(this);
        }
    }

    void DeclarationCollector::visit(FieldKeywordExpressionNode *node)
    {
        node->containingScope = get_current_handle();
    }

    void DeclarationCollector::visit(ValueKeywordExpressionNode *node)
    {
        node->containingScope = get_current_handle();
    }

    // === Match Expression and Pattern Visitors ===

    void DeclarationCollector::visit(MatchExpressionNode *node)
    {
        node->containingScope = get_current_handle();

        if (node->expression)
        {
            node->expression->containingScope = node->containingScope;
            node->expression->accept(this);
        }

        for (int i = 0; i < node->arms.size; ++i)
        {
            if (node->arms[i])
            {
                node->arms[i]->containingScope = node->containingScope;
                node->arms[i]->accept(this);
            }
        }
    }

    void DeclarationCollector::visit(MatchArmNode *node)
    {
        // Already has containingScope set by parent

        if (node->pattern)
        {
            node->pattern->containingScope = node->containingScope;
            node->pattern->accept(this);
        }

        if (node->result)
        {
            node->result->containingScope = node->containingScope;
            node->result->accept(this);
        }
    }

    void DeclarationCollector::visit(MatchPatternNode *node)
    {
        // Base pattern node - just set scope
        // Derived pattern types will override this
    }

    void DeclarationCollector::visit(EnumPatternNode *node)
    {
        // Already has containingScope set by parent

        if (node->enumCase)
        {
            node->enumCase->containingScope = node->containingScope;
            node->enumCase->accept(this);
        }
    }

    void DeclarationCollector::visit(RangePatternNode *node)
    {
        // Already has containingScope set by parent

        if (node->start)
        {
            node->start->containingScope = node->containingScope;
            node->start->accept(this);
        }

        if (node->end)
        {
            node->end->containingScope = node->containingScope;
            node->end->accept(this);
        }
    }

    void DeclarationCollector::visit(ComparisonPatternNode *node)
    {
        // Already has containingScope set by parent

        if (node->value)
        {
            node->value->containingScope = node->containingScope;
            node->value->accept(this);
        }
    }

    void DeclarationCollector::visit(WildcardPatternNode *node)
    {
        // Already has containingScope set by parent
        // No children to visit
    }

    void DeclarationCollector::visit(LiteralPatternNode *node)
    {
        // Already has containingScope set by parent

        if (node->literal)
        {
            node->literal->containingScope = node->containingScope;
            node->literal->accept(this);
        }
    }

    // === Generic Parameter ===

    void DeclarationCollector::visit(GenericParameterNode *node)
    {
        node->containingScope = get_current_handle();

        if (node->name)
        {
            node->name->containingScope = node->containingScope;
            node->name->accept(this);
        }
    }

    // === Default Visitors ===

    void DeclarationCollector::visit(ExpressionNode *node)
    {
        node->containingScope = get_current_handle();
    }

    void DeclarationCollector::visit(StatementNode *node)
    {
        node->containingScope = get_current_handle();
    }

    void DeclarationCollector::visit(DeclarationNode *node)
    {
        node->containingScope = get_current_handle();
    }

    void DeclarationCollector::visit(ErrorNode *node)
    {
        node->containingScope = get_current_handle();
    }

    void DeclarationCollector::visit(TokenNode *node)
    {
        node->containingScope = get_current_handle();
    }

    void DeclarationCollector::visit(IdentifierNode *node)
    {
        node->containingScope = get_current_handle();
    }

} // namespace Myre