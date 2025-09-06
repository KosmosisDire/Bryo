#include "semantic/symbol_table_builder.hpp"

namespace Bryo
{

    SymbolTableBuilder::SymbolTableBuilder(SymbolTable &st)
        : symbolTable(st), typeSystem(st.get_type_system()) {}

    SymbolHandle SymbolTableBuilder::get_current_handle()
    {
        auto h = symbolTable.get_current_scope()->handle;
        assert(h.id != 0 && "Current scope must have a valid handle");
        return h;
    }

    void SymbolTableBuilder::annotate_scope(BaseSyntax *node)
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
        for (auto stmt : block->statements)
        {
            if (stmt)
                stmt->accept(this);
        }
    }

    std::string SymbolTableBuilder::build_qualified_name(QualifiedNameSyntax *memberAccess)
    {
        if (!memberAccess)
            return "";

        std::string result;

        // Recursively build from nested member access
        if (auto nestedMember = memberAccess->object->as<QualifiedNameSyntax>())
        {
            result = build_qualified_name(nestedMember) + ".";
        }
        // Or get the base name
        else if (auto name = memberAccess->object->as<NameExpr>())
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

    TypePtr SymbolTableBuilder::create_unresolved_type(BaseExprSyntax *typeExpr)
    {
        if (typeExpr)
        {
            annotate_scope(typeExpr);
            typeExpr->accept(this);
        }

        return typeSystem.get_unresolved_type();
    }

    void SymbolTableBuilder::collect(CompilationUnitSyntax *unit)
    {
        if (unit)
        {
            unit->accept(this);
        }
    }

    void SymbolTableBuilder::visit(BaseSyntax *node)
    {
        annotate_scope(node);
    }

    void SymbolTableBuilder::visit(CompilationUnitSyntax *node)
    {
        symbolTable.enter_namespace("global");
        annotate_scope(node);

        for (auto stmt : node->topLevelStatements)
        {
            if (stmt)
                stmt->accept(this);
        }

        symbolTable.exit_scope();
    }

    void SymbolTableBuilder::visit(NamespaceDeclSyntax *node)
    {
        annotate_scope(node);

        if (!node->name)
            return;

        std::string ns_name;
        // Handle single name or qualified name
        if (auto name = node->name->as<NameExpr>())
        {
            ns_name = name->get_name();
        }
        else if (auto member = node->name->as<QualifiedNameSyntax>())
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

        auto ns_symbol = symbolTable.enter_namespace(ns_name);

        if (node->body)
        {
            for (auto stmt : *node->body)
            {
                if (stmt)
                    stmt->accept(this);
            }
            symbolTable.exit_scope();
        }
        // File-scoped namespaces don't exit scope
    }

    void SymbolTableBuilder::visit(TypeDeclSyntax *node)
    {
        annotate_scope(node);

        if (!node->name)
            return;

        std::string type_name(node->name->text);
        auto type_symbol = symbolTable.enter_type(type_name);

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
        for (auto member : node->members)
        {
            if (member)
                member->accept(this);
        }

        symbolTable.exit_scope();
    }
    
    void SymbolTableBuilder::visit(FunctionDeclSyntax *node)
    {
        annotate_scope(node);

        if (!node->name)
            return;

        std::string func_name(node->name->text);

        // Get return type
        TypePtr return_type = create_unresolved_type(node->returnType);

        // Enter function scope
        auto func_symbol = symbolTable.enter_function(func_name, return_type);
        node->functionSymbol = func_symbol->handle;

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

        // Add parameters to function scope and visit ParameterDeclSyntax nodes
        auto parameters = std::vector<ParameterSymbol *>(node->parameters.size());
        for (size_t i = 0; i < node->parameters.size(); ++i)
        {
            auto param = node->parameters[i];
            if (param && param->param && param->param->name)
            {
                std::string param_name(param->param->name->text);
                auto param_symbol = symbolTable.define_parameter(param_name, create_unresolved_type(param->param->type));
                if (!param_symbol)
                {
                    errors.push_back("Parameter '" + param_name + "' already defined in function scope");
                }

                parameters[i] = param_symbol;

                // Visit the ParameterDeclSyntax node to annotate it with scope
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

    void SymbolTableBuilder::visit(VariableDeclSyntax *node)
    {
        annotate_scope(node);

        if (!node->variable)
            return;

        TypePtr var_type = create_unresolved_type(node->variable->type);

        if (node->variable->name)
        {
            std::string var_name(node->variable->name->text);

            auto var_symbol = symbolTable.define_variable(var_name, var_type);
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

    void SymbolTableBuilder::visit(PropertyDeclSyntax *node)
    {
        annotate_scope(node);

        if (!node->variable || !node->variable->variable || !node->variable->variable->name)
            return;

        std::string name(node->variable->variable->name->text);
        TypePtr type = create_unresolved_type(node->variable->variable->type);

        auto prop_symbol = symbolTable.enter_property(name, type);
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

    void SymbolTableBuilder::visit_property_accessor(PropertyAccessorSyntax *accessor, TypePtr propType)
    {
        if (!accessor)
            return;

        annotate_scope(accessor);

        std::string kind_name = (accessor->kind == PropertyAccessorSyntax::Kind::Get) ? "get" : "set";
        auto accessor_scope = symbolTable.enter_block(kind_name + "-accessor");

        // For setters, add 'value' parameter
        if (accessor->kind == PropertyAccessorSyntax::Kind::Set)
        {
            auto value_symbol = symbolTable.define_parameter("value", propType);
            if (!value_symbol)
            {
                errors.push_back("Could not create 'value' parameter for setter");
            }
        }

        // Visit accessor body
        if (auto expr = std::get_if<BaseExprSyntax *>(&accessor->body))
        {
            (*expr)->accept(this);
        }
        else if (auto block = std::get_if<Block *>(&accessor->body))
        {
            visit_block_contents(*block);
        }

        symbolTable.exit_scope();
    }

    void SymbolTableBuilder::visit(EnumCaseDeclSyntax *node)
    {
        annotate_scope(node);

        if (!node->name)
            return;

        std::string case_name(node->name->text);

        // Build associated types
        std::vector<TypePtr> params;
        for (auto param : node->associatedData)
        {
            if (param && param->param)
            {
                params.push_back(create_unresolved_type(param->param->type));
            }
        }

        auto case_symbol = symbolTable.define_enum_case(case_name, params);
        if (!case_symbol)
        {
            errors.push_back("Enum case '" + case_name + "' already defined");
        }

        // Manually visit name and associated data (already handled by create_unresolved_type above)
        if (node->name)
        {
            node->name->accept(this);
        }
        for (auto param : node->associatedData)
        {
            if (param)
                param->accept(this);
        }
    }

    void SymbolTableBuilder::visit(Block *node)
    {
        annotate_scope(node);

        auto block_scope = symbolTable.enter_block("block");

        for (auto stmt : node->statements)
        {
            if (stmt)
                stmt->accept(this);
        }

        symbolTable.exit_scope();
    }

    void SymbolTableBuilder::visit(WhileStmtSyntax *node)
    {
        annotate_scope(node);

        auto while_scope = symbolTable.enter_block("while");

        // Visit condition and body in while scope
        if (node->condition)
            node->condition->accept(this);
        if (node->body)
        {
            if (auto block = node->body->as<Block>())
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

    void SymbolTableBuilder::visit(ForStmtSyntax *node)
    {
        annotate_scope(node);

        auto for_scope = symbolTable.enter_block("for");

        // Visit all parts in for scope
        if (node->initializer)
            node->initializer->accept(this);
        if (node->condition)
            node->condition->accept(this);
        for (auto update : node->updates)
        {
            if (update)
                update->accept(this);
        }
        if (node->body)
        {
            if (auto block = node->body->as<Block>())
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

    void SymbolTableBuilder::visit(IfStmt *node)
    {
        annotate_scope(node);

        auto if_scope = symbolTable.enter_block("if");

        if (node->condition)
            node->condition->accept(this);
        if (node->thenBranch)
        {
            if (auto block = node->thenBranch->as<Block>())
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
            if (auto block = node->elseBranch->as<Block>())
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

    void SymbolTableBuilder::visit(ArrayTypeSyntax *node)
    {
        // Call base visitor to annotate scope
        visit(static_cast<BaseExprSyntax *>(node));

        // Mark element type as a type expression and visit it
        if (node->baseType)
        {
            annotate_scope(node->baseType);
            node->baseType->accept(this);
        }
    }

    void SymbolTableBuilder::visit(GenericTypeExpr *node)
    {
        // Call base visitor to annotate scope
        visit(static_cast<BaseExprSyntax *>(node));

        // Mark base type as a type expression and visit it
        if (node->baseType)
        {
            annotate_scope(node->baseType);
            node->baseType->accept(this);
        }

        // Mark type arguments as type expressions and visit them
        for (auto typeArg : node->typeArguments)
        {
            if (typeArg)
            {
                annotate_scope(typeArg);
                typeArg->accept(this);
            }
        }
    }

    void SymbolTableBuilder::visit(PointerTypeExpr *node)
    {
        // Call base visitor to annotate scope
        visit(static_cast<BaseExprSyntax *>(node));

        // Mark base type as a type expression and visit it
        if (node->baseType)
        {
            annotate_scope(node->baseType);
            node->baseType->accept(this);
        }
    }

    void SymbolTableBuilder::visit(TypeParameterDeclSyntax *node)
    {
        annotate_scope(node);

        // Visit the name
        if (node->name)
        {
            node->name->accept(this);
        }

        // Type parameters don't create symbols during the building phase
        // They are handled later during generic type definition processing
    }

} // namespace Bryo