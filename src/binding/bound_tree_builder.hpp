#pragma once

#include "bound_tree.hpp"
#include "ast/ast.hpp"
#include "binding_arena.hpp"

namespace Bryo
{
    class BoundTreeBuilder
    {
    private:
        BindingArena arena_;
        
    public:
        BoundTreeBuilder();
        
        // Main entry point
        BoundCompilationUnit* bind(CompilationUnitSyntax* syntax);
        
    private:
        // === Statement/Declaration binding ===
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

        // === Expression binding ===
        BoundExpression* bind_expression(BaseExprSyntax* syntax);
        BoundLiteralExpression* bind_literal(LiteralExprSyntax* syntax);
        BoundNameExpression* bind_name(BaseNameExprSyntax* syntax);
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
        
        // === Type expression binding ===
        BoundTypeExpression* bind_type_expression(BaseExprSyntax* syntax);
    };
    
} // namespace Bryo