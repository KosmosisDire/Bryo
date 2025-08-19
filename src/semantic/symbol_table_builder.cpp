#include "semantic/symbol_table_builder.hpp"

namespace Myre
{

    SymbolTableBuilder::SymbolTableBuilder(SymbolTable &st)
        : symbolTable(st), typeSystem(st.get_type_system()) {}

    SymbolHandle SymbolTableBuilder::get_current_handle()
    {
        auto h = symbolTable.get_current_scope()->handle;
        assert(h.id != 0 && "Current scope must have a valid handle");
        return h;
    }

    void SymbolTableBuilder::annotate_scope(Node *node)
    {
        if (node)
        {
            node->containingScope = get_current_handle();
        }
    }

    void SymbolTableBuilder::visit_block_contents(Block *block)
    {
        if (!block)
            return;
        for (auto *stmt : block->statements)
        {
            if (stmt)
                stmt->accept(this);
        }
    }

    std::string SymbolTableBuilder::build_qualified_name(MemberAccessExpr *memberAccess)
    {
        if (!memberAccess)
            return "";

        std::string result;

        // Recursively build from nested member access
        if (auto *nestedMember = memberAccess->object->as<MemberAccessExpr>())
        {
            result = build_qualified_name(nestedMember) + ".";
        }
        // Or get the base name
        else if (auto *name = memberAccess->object->as<NameExpr>())
        {
            result = name->get_name() + ".";
        }

        // Add the member name
        if (memberAccess->member)
        {
            result += memberAccess->member->text;
        }

        return result;
    }

    TypePtr SymbolTableBuilder::create_unresolved_type(Expression *typeExpr)
    {
        // CRITICAL: Ensure type expression gets its containing scope set
        // This is needed because type expressions may not be visited through normal visitor traversal
        if (typeExpr)
        {
            annotate_scope(typeExpr);
            // Mark this as a type expression to avoid treating it as a value expression later
            typeExpr->isTypeExpression = true;
            // Also visit children of the type expression to ensure they get scope annotation
            typeExpr->accept(this);
        }

        // SymbolTableBuilder should NOT resolve any types - just create unresolved types
        // All type resolution happens later in TypeResolver
        auto unresolved = typeSystem.get_unresolved_type();
        if (typeExpr && std::get_if<UnresolvedType>(&unresolved->value))
        {
            auto& unresolvedVar = unresolved->as<UnresolvedType>();
            unresolvedVar.typeName = typeExpr;
            unresolvedVar.definingScope = get_current_handle();
        }
        return unresolved;
    }

    void SymbolTableBuilder::collect(CompilationUnit *unit)
    {
        if (unit)
        {
            unit->accept(this);
        }
    }

    void SymbolTableBuilder::visit(Node *node)
    {
        annotate_scope(node);
    }

    void SymbolTableBuilder::visit(CompilationUnit *node)
    {
        symbolTable.enter_namespace("global");
        annotate_scope(node);

        for (auto *stmt : node->topLevelStatements)
        {
            if (stmt)
                stmt->accept(this);
        }

        symbolTable.exit_scope();
    }

    void SymbolTableBuilder::visit(NamespaceDecl *node)
    {
        annotate_scope(node);

        if (!node->name)
            return;

        std::string ns_name;
        // Handle single name or qualified name
        if (auto *name = node->name->as<NameExpr>())
        {
            ns_name = name->get_name();
        }
        else if (auto *member = node->name->as<MemberAccessExpr>())
        {
            ns_name = build_qualified_name(member);
        }
        else
        {
            return; // Invalid namespace name
        }

        // Check for file-scoped namespace restrictions
        if (node->isFileScoped && symbolTable.get_current_namespace() &&
            symbolTable.get_current_namespace() != symbolTable.get_global_namespace())
        {
            errors.push_back("File-scoped namespace cannot be declared inside another namespace");
            return;
        }

        auto *ns_symbol = symbolTable.enter_namespace(ns_name);

        if (node->body)
        {
            for (auto *stmt : *node->body)
            {
                if (stmt)
                    stmt->accept(this);
            }
            symbolTable.exit_scope();
        }
        // File-scoped namespaces don't exit scope
    }
    void SymbolTableBuilder::visit(TypeDecl *node)
    {
        annotate_scope(node);

        if (!node->name)
            return;

        std::string type_name(node->name->text);
        auto *type_symbol = symbolTable.enter_type(type_name);

        // Apply modifiers
        if (has_flag(node->modifiers, ModifierKindFlags::Static))
        {
            type_symbol->add_modifier(SymbolModifiers::Static);
        }
        if (has_flag(node->modifiers, ModifierKindFlags::Abstract))
        {
            type_symbol->add_modifier(SymbolModifiers::Abstract);
        }

        type_symbol->set_access(AccessLevel::Public);

        // Visit members
        for (auto *member : node->members)
        {
            if (member)
                member->accept(this);
        }

        symbolTable.exit_scope();
    }
    void SymbolTableBuilder::visit(FunctionDecl *node)
    {
        annotate_scope(node);

        if (!node->name)
            return;

        std::string func_name(node->name->text);

        // Get return type
        TypePtr return_type = create_unresolved_type(node->returnType);

        // Enter function scope
        auto *func_symbol = symbolTable.enter_function(func_name, return_type);
        node->functionSymbol = func_symbol->handle;

        if (return_type->is<UnresolvedType>())
        {
            auto &unresolved = return_type->as<UnresolvedType>();
            unresolved.body = node->body;                   // Set the function body for analysis
            unresolved.definingScope = func_symbol->handle; // Set the scope to the function itself
        }

        // Apply modifiers
        if (has_flag(node->modifiers, ModifierKindFlags::Static))
        {
            func_symbol->add_modifier(SymbolModifiers::Static);
        }
        if (has_flag(node->modifiers, ModifierKindFlags::Virtual))
        {
            func_symbol->add_modifier(SymbolModifiers::Virtual);
        }
        if (has_flag(node->modifiers, ModifierKindFlags::Override))
        {
            func_symbol->add_modifier(SymbolModifiers::Override);
        }
        if (has_flag(node->modifiers, ModifierKindFlags::Abstract))
        {
            func_symbol->add_modifier(SymbolModifiers::Abstract);
        }

        func_symbol->set_access(AccessLevel::Public);

        // Add parameters to function scope and visit ParameterDecl nodes
        auto parameters = std::vector<SymbolHandle>(node->parameters.size());
        for (size_t i = 0; i < node->parameters.size(); ++i)
        {
            auto *param = node->parameters[i];
            if (param && param->param && param->param->name)
            {
                std::string param_name(param->param->name->text);
                auto *param_symbol = symbolTable.define_parameter(param_name, create_unresolved_type(param->param->type));
                if (!param_symbol)
                {
                    errors.push_back("Parameter '" + param_name + "' already defined in function scope");
                }

                parameters[i] = param_symbol->handle;

                // Visit the ParameterDecl node to annotate it with scope
                param->accept(this);
            }
        }
        func_symbol->set_parameters(std::move(parameters));

        // Visit function body
        if (node->body)
        {
            visit_block_contents(node->body);
        }

        symbolTable.exit_scope();
    }

    void SymbolTableBuilder::visit(VariableDecl *node)
    {
        annotate_scope(node);

        if (!node->variable)
            return;

        TypePtr var_type = create_unresolved_type(node->variable->type);

        // Handle type inference
        if (var_type->is<UnresolvedType>())
        {
            auto &unresolved = var_type->as<UnresolvedType>();
            unresolved.initializer = node->initializer;
            unresolved.definingScope = get_current_handle();
        }

        if (node->variable->name)
        {
            std::string var_name(node->variable->name->text);

            auto *var_symbol = symbolTable.define_variable(var_name, var_type);
            if (!var_symbol)
            {
                errors.push_back("Variable '" + var_name + "' already defined in current scope");
            }
        }

        // Manually visit initializer (variable->name and variable->type are handled by create_unresolved_type)
        if (node->initializer)
        {
            node->initializer->accept(this);
        }
    }

    void SymbolTableBuilder::visit(PropertyDecl *node)
    {
        annotate_scope(node);

        if (!node->variable || !node->variable->variable || !node->variable->variable->name)
            return;

        std::string name(node->variable->variable->name->text);
        TypePtr type = create_unresolved_type(node->variable->variable->type);

        if (type->is<UnresolvedType>())
        {
            auto &unresolved = type->as<UnresolvedType>();
            unresolved.initializer = node->variable->initializer;
        }

        auto *prop_symbol = symbolTable.enter_property(name, type);
        if (!prop_symbol)
        {
            errors.push_back("Property '" + name + "' already defined in current scope");
            return;
        }

        // Apply modifiers
        if (has_flag(node->modifiers, ModifierKindFlags::Static))
        {
            prop_symbol->add_modifier(SymbolModifiers::Static);
        }
        prop_symbol->set_access(AccessLevel::Public);

        // Visit accessors if they exist
        if (node->getter || node->setter)
        {
            visit_property_accessor(node->getter, type);
            visit_property_accessor(node->setter, type);
        }

        symbolTable.exit_scope();
    }

    void SymbolTableBuilder::visit_property_accessor(PropertyAccessor *accessor, TypePtr propType)
    {
        if (!accessor)
            return;

        annotate_scope(accessor);

        std::string kind_name = (accessor->kind == PropertyAccessor::Kind::Get) ? "get" : "set";
        auto *accessor_scope = symbolTable.enter_block(kind_name + "-accessor");

        // For setters, add 'value' parameter
        if (accessor->kind == PropertyAccessor::Kind::Set)
        {
            auto *value_symbol = symbolTable.define_parameter("value", propType);
            if (!value_symbol)
            {
                errors.push_back("Could not create 'value' parameter for setter");
            }
        }

        // Visit accessor body
        if (auto *expr = std::get_if<Expression *>(&accessor->body))
        {
            (*expr)->accept(this);
        }
        else if (auto *block = std::get_if<Block *>(&accessor->body))
        {
            visit_block_contents(*block);
        }

        symbolTable.exit_scope();
    }

    void SymbolTableBuilder::visit(EnumCaseDecl *node)
    {
        annotate_scope(node);

        if (!node->name)
            return;

        std::string case_name(node->name->text);

        // Build associated types
        std::vector<TypePtr> params;
        for (auto *param : node->associatedData)
        {
            if (param && param->param)
            {
                params.push_back(create_unresolved_type(param->param->type));
            }
        }

        auto *case_symbol = symbolTable.define_enum_case(case_name, params);
        if (!case_symbol)
        {
            errors.push_back("Enum case '" + case_name + "' already defined");
        }

        // Manually visit name and associated data (already handled by create_unresolved_type above)
        if (node->name)
        {
            node->name->accept(this);
        }
        for (auto *param : node->associatedData)
        {
            if (param)
                param->accept(this);
        }
    }

    void SymbolTableBuilder::visit(Block *node)
    {
        annotate_scope(node);

        auto *block_scope = symbolTable.enter_block("block");

        for (auto *stmt : node->statements)
        {
            if (stmt)
                stmt->accept(this);
        }

        symbolTable.exit_scope();
    }

    void SymbolTableBuilder::visit(WhileStmt *node)
    {
        annotate_scope(node);

        auto *while_scope = symbolTable.enter_block("while");

        // Visit condition and body in while scope
        if (node->condition)
            node->condition->accept(this);
        if (node->body)
        {
            if (auto *block = node->body->as<Block>())
            {
                visit_block_contents(block);
            }
            else
            {
                node->body->accept(this);
            }
        }

        symbolTable.exit_scope();
    }

    void SymbolTableBuilder::visit(ForStmt *node)
    {
        annotate_scope(node);

        auto *for_scope = symbolTable.enter_block("for");

        // Visit all parts in for scope
        if (node->initializer)
            node->initializer->accept(this);
        if (node->condition)
            node->condition->accept(this);
        for (auto *update : node->updates)
        {
            if (update)
                update->accept(this);
        }
        if (node->body)
        {
            if (auto *block = node->body->as<Block>())
            {
                visit_block_contents(block);
            }
            else
            {
                node->body->accept(this);
            }
        }

        symbolTable.exit_scope();
    }

    void SymbolTableBuilder::visit(IfExpr *node)
    {
        annotate_scope(node);

        auto *if_scope = symbolTable.enter_block("if");

        if (node->condition)
            node->condition->accept(this);
        if (node->thenBranch)
        {
            if (auto *block = node->thenBranch->as<Block>())
            {
                visit_block_contents(block);
            }
            else
            {
                node->thenBranch->accept(this);
            }
        }
        if (node->elseBranch)
        {
            if (auto *block = node->elseBranch->as<Block>())
            {
                visit_block_contents(block);
            }
            else
            {
                node->elseBranch->accept(this);
            }
        }

        symbolTable.exit_scope();
    }

} // namespace Myre