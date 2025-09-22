#pragma once

#include "bound_tree.hpp"
#include "ast/ast.hpp"
#include "binding_arena.hpp"
#include "semantic/symbol_table.hpp"

namespace Bryo
{
    class BoundTreeBuilder
    {
    private:
        BindingArena arena_;
        SymbolTable& symbol_table_;
        
        #pragma region Symbol Resolution Helpers
        
        // Simple symbol lookup
        Symbol* resolve_symbol(const std::vector<std::string>& parts)
        {
            return symbol_table_.resolve(parts);
        }
        
        // Function overload resolution
        FunctionSymbol* resolve_function(const std::string& name, BoundCallExpression* call)
        {
            std::vector<TypePtr> arg_types;
            for (auto* arg : call->arguments)
            {
                if (arg && arg->type)
                {
                    arg_types.push_back(arg->type);
                }
            }
            return symbol_table_.resolve_function(name, arg_types);
        }
        
        // Member resolution on a type
        Symbol* resolve_member(TypePtr type, const std::string& member_name)
        {
            if (!type) return nullptr;
            
            if (auto symbol = symbol_table_.resolve(type->get_name()))
            {
                if (auto type_symbol = symbol->as<TypeSymbol>())
                {
                    auto members = type_symbol->get_member(member_name);
                    return members.empty() ? nullptr : members[0];
                }
            }
            return nullptr;
        }
        
        // Constructor resolution
        FunctionSymbol* resolve_constructor(TypePtr type, const std::vector<BoundExpression*>& arguments)
        {
            if (!type) return nullptr;
            
            if (auto symbol = symbol_table_.resolve(type->get_name()))
            {
                if (auto type_symbol = symbol->as<TypeSymbol>())
                {
                    std::vector<TypePtr> arg_types;
                    for (auto* arg : arguments)
                    {
                        if (arg && arg->type)
                        {
                            arg_types.push_back(arg->type);
                        }
                    }
                    
                    auto funcs = type_symbol->get_functions(type_symbol->name);
                    for (auto* func : funcs)
                    {
                        if (func->is_constructor && matches_signature(func, arg_types))
                        {
                            return func;
                        }
                    }
                }
            }
            return nullptr;
        }
        
        // Get current containing type for 'this' expressions
        TypeSymbol* get_containing_type()
        {
            auto current = symbol_table_.get_current_scope();
            return current ? current->get_enclosing<TypeSymbol>() : nullptr;
        }
        
        bool matches_signature(FunctionSymbol* func, const std::vector<TypePtr>& arg_types)
        {
            if (func->parameters.size() != arg_types.size()) return false;
            
            for (size_t i = 0; i < arg_types.size(); ++i)
            {
                if (func->parameters[i]->type != arg_types[i])
                {
                    return false;
                }
            }
            return true;
        }
        
        #pragma region Scope Management Helpers
        
        class ScopeGuard
        {
            SymbolTable& table_;
            bool pushed_;
        public:
            ScopeGuard(SymbolTable& table, Symbol* symbol) 
                : table_(table), pushed_(symbol != nullptr)
            {
                if (pushed_)
                {
                    table_.push_scope(symbol);
                }
            }
            ~ScopeGuard()
            {
                if (pushed_)
                {
                    table_.pop_scope();
                }
            }
        };
        
    public:
        BoundTreeBuilder(SymbolTable& symbol_table);
        
        // Main entry point
        BoundCompilationUnit* bind(CompilationUnitSyntax* syntax);
        
    private:

        #pragma region Statements

        BoundStatement* bind_statement(BaseStmtSyntax* syntax);
        BoundBlockStatement* bind_block(BlockSyntax* syntax);
        BoundVariableDeclaration* bind_variable_declaration(VariableDeclSyntax* syntax);
        BoundFunctionDeclaration* bind_function_declaration(FunctionDeclSyntax* syntax);
        BoundTypeDeclaration* bind_type_declaration(TypeDeclSyntax* syntax);
        BoundNamespaceDeclaration* bind_namespace_declaration(NamespaceDeclSyntax* syntax);
        BoundPropertyDeclaration* bind_property_declaration(PropertyDeclSyntax* syntax);
        BoundIfStatement* bind_if_statement(IfStmtSyntax* syntax);
        BoundWhileStatement* bind_while_statement(WhileStmtSyntax* syntax);
        BoundForStatement* bind_for_statement(ForStmtSyntax* syntax);
        BoundReturnStatement* bind_return_statement(ReturnStmtSyntax* syntax);
        BoundBreakStatement* bind_break_statement(BreakStmtSyntax* syntax);
        BoundContinueStatement* bind_continue_statement(ContinueStmtSyntax* syntax);
        BoundExpressionStatement* bind_expression_statement(ExpressionStmtSyntax* syntax);
        BoundUsingStatement* bind_using_statement(UsingDirectiveSyntax* syntax);

        #pragma region Expressions

        BoundExpression* bind_expression(BaseExprSyntax* syntax);
        BoundLiteralExpression* bind_literal(LiteralExprSyntax* syntax);
        BoundExpression* bind_name(BaseNameExprSyntax* syntax);
        BoundBinaryExpression* bind_binary_expression(BinaryExprSyntax* syntax);
        BoundUnaryExpression* bind_unary_expression(UnaryExprSyntax* syntax);
        BoundAssignmentExpression* bind_assignment_expression(AssignmentExprSyntax* syntax);
        BoundCallExpression* bind_call_expression(CallExprSyntax* syntax);
        BoundMemberAccessExpression* bind_member_access(MemberAccessExprSyntax* syntax);
        BoundIndexExpression* bind_index_expression(IndexerExprSyntax* syntax);
        BoundConditionalExpression* bind_conditional_expression(ConditionalExprSyntax* syntax);
        BoundCastExpression* bind_cast_expression(CastExprSyntax* syntax);
        BoundNewExpression* bind_new_expression(NewExprSyntax* syntax);
        BoundThisExpression* bind_this_expression(ThisExprSyntax* syntax);
        BoundArrayCreationExpression* bind_array_creation(ArrayLiteralExprSyntax* syntax);
        BoundTypeOfExpression* bind_typeof_expression(TypeOfExprSyntax* syntax);
        BoundSizeOfExpression* bind_sizeof_expression(SizeOfExprSyntax* syntax);
        BoundParenthesizedExpression* bind_parenthesized_expression(ParenthesizedExprSyntax* syntax);
        
        #pragma region Types

        BoundTypeExpression* bind_type_expression(BaseExprSyntax* syntax);
    };
    
} // namespace Bryo