#pragma once

#include "bound_tree.hpp"
#include "semantic/symbol_table.hpp"

namespace Bryo
{

    class SymbolResolutionVisitor : public DefaultBoundVisitor
    {
    public:
        explicit SymbolResolutionVisitor(SymbolTable &symbolTable)
            : symbol_table(symbolTable) {}

        // === Expressions that need symbol resolution ===

        void visit(BoundNameExpression *node) override
        {

            node->symbol = symbol_table.resolve(node->parts);
            DefaultBoundVisitor::visit(node);
        }

        void visit(BoundCallExpression *node) override
        {
            // First visit children to resolve them
            DefaultBoundVisitor::visit(node);

            // Now resolve and ASSIGN the method
            if (auto nameExpr = node->callee->as<BoundNameExpression>())
            {
                std::vector<TypePtr> arg_types;
                for (auto *arg : node->arguments)
                {
                    if (arg && arg->type)
                    {
                        arg_types.push_back(arg->type);
                    }
                }

                std::string funcName = nameExpr->parts.empty() ? "" : nameExpr->parts.back();
                node->method = symbol_table.resolve_function(funcName, arg_types);
            }
            else if (auto memberExpr = node->callee->as<BoundMemberAccessExpression>())
            {
                if (memberExpr->member && memberExpr->member->is<FunctionSymbol>())
                {
                    node->method = memberExpr->member->as<FunctionSymbol>();
                }
            }
        }

        void visit(BoundMemberAccessExpression *node) override
        {
            // Visit object first
            if (node->object)
            {
                node->object->accept(this);
            }

            if (node->object && node->object->type)
            {
                if (auto typeSymbol = find_type_symbol(node->object->type))
                {
                    auto members = typeSymbol->get_member(node->memberName);
                    node->member = members.empty() ? nullptr : members[0];
                }
            }
        }

        void visit(BoundNewExpression *node) override
        {
            DefaultBoundVisitor::visit(node);

            if (node->typeExpression && node->typeExpression->type)
            {
                if (auto typeSymbol = find_type_symbol(node->typeExpression->type))
                {
                    std::vector<TypePtr> arg_types;
                    for (auto *arg : node->arguments)
                    {
                        if (arg && arg->type)
                        {
                            arg_types.push_back(arg->type);
                        }
                    }

                    auto funcs = typeSymbol->get_functions(typeSymbol->name);
                    for (auto *func : funcs)
                    {
                        if (func->is_constructor && matches_signature(func, arg_types))
                        {
                            node->constructor = func;
                            break;
                        }
                    }
                }
            }
        }

        void visit(BoundThisExpression *node) override
        {

            auto current = symbol_table.get_current_scope();
            node->containingType = current ? current->get_enclosing<TypeSymbol>() : nullptr;
            DefaultBoundVisitor::visit(node);
        }

        void visit(BoundTypeExpression *node) override
        {

            if (auto symbol = symbol_table.resolve(node->parts))
            {
                if (auto typeSymbol = symbol->as<TypeSymbol>())
                {
                    node->resolvedTypeReference = typeSymbol->type;
                }
            }
            DefaultBoundVisitor::visit(node);
        }

        void visit(BoundIndexExpression *node) override
        {
            DefaultBoundVisitor::visit(node);

            if (node->object && node->object->type)
            {
                if (auto typeSymbol = find_type_symbol(node->object->type))
                {
                    auto members = typeSymbol->get_member("Item");
                    for (auto *member : members)
                    {
                        if (auto prop = member->as<PropertySymbol>())
                        {
                            node->indexerProperty = prop;
                            break;
                        }
                    }
                }
            }
        }

        void visit(BoundBinaryExpression *node) override
        {
            DefaultBoundVisitor::visit(node);
            // TODO: ASSIGN operatorMethod for user-defined operators
        }

        void visit(BoundUnaryExpression *node) override
        {
            DefaultBoundVisitor::visit(node);
            // TODO: ASSIGN operatorMethod for user-defined operators
        }

        // === Statements ===

        void visit(BoundUsingStatement *node) override
        {

            if (auto symbol = symbol_table.resolve(node->namespaceParts))
            {
                node->targetNamespace = symbol->as<NamespaceSymbol>();
            }
            DefaultBoundVisitor::visit(node);
        }

        // === Declarations - ASSIGN their symbols ===

        void visit(BoundVariableDeclaration *node) override
        {
            node->symbol = symbol_table.resolve_local(node->name);

            // Push scope if needed for initialization
            if (node->symbol)
            {
                symbol_table.push_scope(node->symbol);
            }

            DefaultBoundVisitor::visit(node);

            if (node->symbol)
            {
                symbol_table.pop_scope();
            }
        }

        void visit(BoundFunctionDeclaration *node) override
        {

            node->symbol = symbol_table.resolve(node->name);

            if (node->symbol)
            {
                symbol_table.push_scope(node->symbol);
            }

            DefaultBoundVisitor::visit(node);

            if (node->symbol)
            {
                symbol_table.pop_scope();
            }
        }

        void visit(BoundPropertyDeclaration *node) override
        {

            node->symbol = symbol_table.resolve(node->name);

            if (node->symbol)
            {
                symbol_table.push_scope(node->symbol);
            }

            DefaultBoundVisitor::visit(node);

            if (node->symbol)
            {
                symbol_table.pop_scope();
            }
        }

        void visit(BoundTypeDeclaration *node) override
        {

            node->symbol = symbol_table.resolve(node->name);

            if (node->symbol)
            {
                symbol_table.push_scope(node->symbol);
            }

            DefaultBoundVisitor::visit(node);

            if (node->symbol)
            {
                symbol_table.pop_scope();
            }
        }

        void visit(BoundNamespaceDeclaration *node) override
        {

            node->symbol = symbol_table.resolve(node->name);

            if (node->symbol)
            {
                symbol_table.push_scope(node->symbol);
            }

            DefaultBoundVisitor::visit(node);

            if (node->symbol)
            {
                symbol_table.pop_scope();
            }
        }

        void visit(BoundCompilationUnit *node) override
        {
            // Start from global namespace
            symbol_table.push_scope(symbol_table.get_global_namespace());
            DefaultBoundVisitor::visit(node);
            symbol_table.pop_scope();
        }

    private:
        SymbolTable &symbol_table;

        TypeSymbol *find_type_symbol(TypePtr type)
        {
            if (!type)
                return nullptr;

            if (auto symbol = symbol_table.resolve(type->get_name()))
            {
                return symbol->as<TypeSymbol>();
            }
            return nullptr;
        }

        bool matches_signature(FunctionSymbol* func, const std::vector<TypePtr>& arg_types)
        {
            if (func->parameters.size() != arg_types.size()) return false;
            
            for (size_t i = 0; i < arg_types.size(); ++i) {
                if (func->parameters[i]->type != arg_types[i]) {
                    return false;
                }
            }
            return true;
        }
    };

} // namespace Bryo