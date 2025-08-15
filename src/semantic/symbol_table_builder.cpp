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

    std::string SymbolTableBuilder::path_to_string(List<Identifier *> path)
    {
        std::string result;
        for (size_t i = 0; i < path.size(); ++i)
        {
            if (i > 0)
                result += ".";
            result += std::string(path[i]->text);
        }
        return result;
    }

    TypePtr SymbolTableBuilder::resolve_type(TypeRef *typeRef)
    {
        if (!typeRef)
            return typeSystem.get_unresolved_type();

        if (auto *named = typeRef->as<NamedTypeRef>())
        {
            return symbolTable.resolve_type_name(path_to_string(named->path));
        }
        else if (auto *array = typeRef->as<ArrayTypeRef>())
        {
            auto elemType = resolve_type(array->elementType);
            return typeSystem.get_array_type(elemType);
        }
        // Add more type resolution as needed
        return typeSystem.get_unresolved_type();
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
        DefaultVisitor::visit(node);
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

        if (node->path.empty())
            return;

        std::string ns_name = path_to_string(node->path);

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

        // Build parameter types
        std::vector<TypePtr> param_types;
        for (auto *param : node->parameters)
        {
            if (param && param->param)
            {
                param_types.push_back(resolve_type(param->param->type));
            }
        }

        // Get return type
        TypePtr return_type = resolve_type(node->returnType);

        // Enter function scope
        auto *func_symbol = symbolTable.enter_function(func_name, return_type, param_types);
        node->functionSymbol = func_symbol->handle;

        if (std::holds_alternative<UnresolvedType>(return_type->value))
        {
            auto &unresolved = std::get<UnresolvedType>(return_type->value);
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

        // Add parameters to function scope
        for (size_t i = 0; i < node->parameters.size(); ++i)
        {
            auto *param = node->parameters[i];
            if (param && param->param && param->param->name)
            {
                std::string param_name(param->param->name->text);
                auto *param_symbol = symbolTable.define_parameter(param_name, param_types[i]);
                if (!param_symbol)
                {
                    errors.push_back("Parameter '" + param_name + "' already defined in function scope");
                }
            }
        }

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

        TypePtr var_type = resolve_type(node->variable->type);

        // Handle type inference
        if (std::holds_alternative<UnresolvedType>(var_type->value))
        {
            auto &unresolved = std::get<UnresolvedType>(var_type->value);
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

        // Continue visiting children
        DefaultVisitor::visit(node);
    }

    void SymbolTableBuilder::visit(MemberVariableDecl *node)
    {
        annotate_scope(node);

        if (!node->name)
            return;

        std::string name(node->name->text);
        TypePtr type = resolve_type(node->type);

        // Determine if this is a field or property
        if (node->getter || node->setter)
        {
            // It's a property
            auto *prop_symbol = symbolTable.enter_property(name, type);

            if (!prop_symbol)
            {
                errors.push_back("Property '" + name + "' already defined in current scope");
                // NOTE: A property scope is entered, so we must exit even on error.
                // Depending on implementation, you might want to return here.
                // For now, assume we continue and exit at the end.
            }
            else
            {
                // Apply modifiers
                if (has_flag(node->modifiers, ModifierKindFlags::Static))
                {
                    prop_symbol->add_modifier(SymbolModifiers::Static);
                }
                if (has_flag(node->modifiers, ModifierKindFlags::Virtual))
                {
                    prop_symbol->add_modifier(SymbolModifiers::Virtual);
                }
                if (has_flag(node->modifiers, ModifierKindFlags::Override))
                {
                    prop_symbol->add_modifier(SymbolModifiers::Override);
                }
                prop_symbol->set_access(AccessLevel::Public);
            }

            // Visit accessors
            if (node->getter)
            {
                visit_property_accessor(node->getter, type);
            }
            if (node->setter)
            {
                visit_property_accessor(node->setter, type);
            }

            symbolTable.exit_scope();
        }
        else
        {
            // It's a field
            if (std::holds_alternative<UnresolvedType>(type->value))
            {
                auto &unresolved = std::get<UnresolvedType>(type->value);
                unresolved.initializer = node->initializer;
                unresolved.definingScope = get_current_handle();
            }

            auto *field_symbol = symbolTable.define_field(name, type);

            if (!field_symbol)
            {
                errors.push_back("Field '" + name + "' already defined in current scope");
            }
            else
            {
                // Apply modifiers
                if (has_flag(node->modifiers, ModifierKindFlags::Static))
                {
                    field_symbol->add_modifier(SymbolModifiers::Static);
                }
                field_symbol->set_access(AccessLevel::Public);
            }
        }

        // Continue visiting children (initializer expression, etc.)
        DefaultVisitor::visit(node);
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
                params.push_back(resolve_type(param->param->type));
            }
        }

        auto *case_symbol = symbolTable.define_enum_case(case_name, params);
        if (!case_symbol)
        {
            errors.push_back("Enum case '" + case_name + "' already defined");
        }

        // Continue visiting children
        DefaultVisitor::visit(node);
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

    void SymbolTableBuilder::visit(ForInStmt *node)
    {
        annotate_scope(node);

        auto *forin_scope = symbolTable.enter_block("for-in");

        // Define loop variable
        if (node->iterator && node->iterator->name)
        {
            std::string iter_name(node->iterator->name->text);
            TypePtr iter_type = resolve_type(node->iterator->type);

            auto *iter_symbol = symbolTable.define_variable(iter_name, iter_type);
            if (!iter_symbol)
            {
                errors.push_back("For-in iterator '" + iter_name + "' already defined");
            }
        }

        // Define index variable if present
        if (node->indexVar && node->indexVar->name)
        {
            std::string idx_name(node->indexVar->name->text);
            TypePtr idx_type = resolve_type(node->indexVar->type);

            auto *idx_symbol = symbolTable.define_variable(idx_name, idx_type);
            if (!idx_symbol)
            {
                errors.push_back("For-in index '" + idx_name + "' already defined");
            }
        }

        // Visit iterable and body
        if (node->iterable)
            node->iterable->accept(this);
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