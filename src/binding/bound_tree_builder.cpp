#include "bound_tree_builder.hpp"

namespace Bryo
{
    BoundTreeBuilder::BoundTreeBuilder() : arena_() {}
    
    BoundCompilationUnit* BoundTreeBuilder::bind(CompilationUnitSyntax* syntax)
    {
        auto unit = arena_.make<BoundCompilationUnit>();
        unit->location = syntax->location;
        
        for (auto stmt : syntax->topLevelStatements)
        {
            if (stmt)
            {
                if (auto bound = bind_statement(stmt))
                {
                    unit->statements.push_back(bound);
                }
            }
        }
        
        return unit;
    }
    
    // === Statement/Declaration binding ===
    
    BoundStatement* BoundTreeBuilder::bind_statement(BaseStmtSyntax* syntax)
    {
        if (!syntax) return nullptr;
        
        // Declarations
        if (auto func_decl = syntax->as<FunctionDeclSyntax>())
            return bind_function_declaration(func_decl);
        if (auto type_decl = syntax->as<TypeDeclSyntax>())
            return bind_type_declaration(type_decl);
        if (auto var_decl = syntax->as<VariableDeclSyntax>())
            return bind_variable_declaration(var_decl);
        if (auto namespace_decl = syntax->as<NamespaceDeclSyntax>())
            return bind_namespace_declaration(namespace_decl);
        if (auto prop_decl = syntax->as<PropertyDeclSyntax>())
            return bind_property_declaration(prop_decl);
        if (auto using_decl = syntax->as<UsingDirectiveSyntax>())
            return bind_using_statement(using_decl);
            
        // Statements
        if (auto block = syntax->as<BlockSyntax>())
            return bind_block(block);
        if (auto if_stmt = syntax->as<IfStmtSyntax>())
            return bind_if_statement(if_stmt);
        if (auto while_stmt = syntax->as<WhileStmtSyntax>())
            return bind_while_statement(while_stmt);
        if (auto for_stmt = syntax->as<ForStmtSyntax>())
            return bind_for_statement(for_stmt);
        if (auto return_stmt = syntax->as<ReturnStmtSyntax>())
            return bind_return_statement(return_stmt);
        if (auto break_stmt = syntax->as<BreakStmtSyntax>())
            return bind_break_statement(break_stmt);
        if (auto continue_stmt = syntax->as<ContinueStmtSyntax>())
            return bind_continue_statement(continue_stmt);
        if (auto expr_stmt = syntax->as<ExpressionStmtSyntax>())
            return bind_expression_statement(expr_stmt);
            
        return nullptr;
    }
    
    BoundBlockStatement* BoundTreeBuilder::bind_block(BlockSyntax* syntax)
    {
        auto bound = arena_.make<BoundBlockStatement>();
        bound->location = syntax->location;
        
        for (auto stmt : syntax->statements)
        {
            if (auto bound_stmt = bind_statement(stmt))
            {
                bound->statements.push_back(bound_stmt);
            }
        }
        
        return bound;
    }
    
    BoundVariableDeclaration* BoundTreeBuilder::bind_variable_declaration(VariableDeclSyntax* syntax)
    {
        auto bound = arena_.make<BoundVariableDeclaration>();
        bound->location = syntax->location;
        bound->modifiers = syntax->modifiers;
        
        if (syntax->variable && syntax->variable->name)
        {
            bound->name = syntax->variable->name->get_name();
        }
        
        if (syntax->variable && syntax->variable->type)
        {
            bound->typeExpression = bind_type_expression(syntax->variable->type);
        }
        
        if (syntax->initializer)
        {
            bound->initializer = bind_expression(syntax->initializer);
        }
        
        // Determine variable kind based on context (will be refined in semantic pass)
        bound->isField = has_flag(bound->modifiers, ModifierKindFlags::Static) ||
                        has_flag(bound->modifiers, ModifierKindFlags::Private) ||
                        has_flag(bound->modifiers, ModifierKindFlags::Public);
        bound->isLocal = !bound->isField && !bound->isParameter;
        
        return bound;
    }
    
    BoundFunctionDeclaration* BoundTreeBuilder::bind_function_declaration(FunctionDeclSyntax* syntax)
    {
        auto bound = arena_.make<BoundFunctionDeclaration>();
        bound->location = syntax->location;
        bound->modifiers = syntax->modifiers;
        
        if (syntax->name)
        {
            bound->name = syntax->name->get_name();
        }
        
        if (syntax->returnType)
        {
            bound->returnTypeExpression = bind_type_expression(syntax->returnType);
        }
        
        // Bind parameters
        for (auto param : syntax->parameters)
        {
            if (auto param_syntax = param->as<ParameterDeclSyntax>())
            {
                auto bound_param = arena_.make<BoundVariableDeclaration>();
                bound_param->location = param_syntax->location;
                bound_param->name = param_syntax->param->name ? param_syntax->param->name->get_name() : "";
                bound_param->typeExpression = param_syntax->param->type ? bind_type_expression(param_syntax->param->type) : nullptr;
                bound_param->isParameter = true;
                bound->parameters.push_back(bound_param);
            }
        }
        
        if (syntax->body)
        {
            bound->body = bind_block(syntax->body);
        }
        
        return bound;
    }
    
    BoundTypeDeclaration* BoundTreeBuilder::bind_type_declaration(TypeDeclSyntax* syntax)
    {
        auto bound = arena_.make<BoundTypeDeclaration>();
        bound->location = syntax->location;
        bound->modifiers = syntax->modifiers;
        
        if (syntax->name)
        {
            bound->name = syntax->name->get_name();
        }

        // TODO: Handle base types and interfaces
        // if (syntax->baseTypes && !syntax->baseTypes->empty())
        // {
        //     // Just take the first base type for now
        //     bound->baseTypeExpression = bind_type_expression(syntax->baseTypes->at(0));
        // }
        
        // Bind members
        for (auto member : syntax->members)
        {
            if (auto bound_member = bind_statement(member))
            {
                bound->members.push_back(bound_member);
            }
        }
        
        return bound;
    }
    
    BoundNamespaceDeclaration* BoundTreeBuilder::bind_namespace_declaration(NamespaceDeclSyntax* syntax)
    {
        auto bound = arena_.make<BoundNamespaceDeclaration>();
        bound->location = syntax->location;
        
        if (syntax->name)
        {
            bound->name = syntax->name->get_name();
        }
        
        if (syntax->body.has_value())
        {
            for (auto member : syntax->body.value())
            {
                if (auto bound_member = bind_statement(member))
                {
                    bound->members.push_back(bound_member);
                }
            }
        }
        
        return bound;
    }
    
    BoundPropertyDeclaration* BoundTreeBuilder::bind_property_declaration(PropertyDeclSyntax* syntax)
    {
        auto bound = arena_.make<BoundPropertyDeclaration>();
        bound->location = syntax->location;
        bound->modifiers = syntax->modifiers;
        
        if (syntax->variable && syntax->variable->variable)
        {
            if (syntax->variable->variable->name)
            {
                bound->name = syntax->variable->variable->name->get_name();
            }
            if (syntax->variable->variable->type)
            {
                bound->typeExpression = bind_type_expression(syntax->variable->variable->type);
            }
        }
        
        if (syntax->getter)
        {
            // TODO: Implement getter binding
            // bound->getter = bind_statement(syntax->getter);
        }
        
        if (syntax->setter)
        {
            // TODO: Implement setter binding
            // bound->setter = bind_statement(syntax->setter);
        }
        
        return bound;
    }
    
    BoundIfStatement* BoundTreeBuilder::bind_if_statement(IfStmtSyntax* syntax)
    {
        auto bound = arena_.make<BoundIfStatement>();
        bound->location = syntax->location;
        
        bound->condition = bind_expression(syntax->condition);
        bound->thenStatement = bind_statement(syntax->thenBranch);
        
        if (syntax->elseBranch)
        {
            bound->elseStatement = bind_statement(syntax->elseBranch);
        }
        
        return bound;
    }
    
    BoundWhileStatement* BoundTreeBuilder::bind_while_statement(WhileStmtSyntax* syntax)
    {
        auto bound = arena_.make<BoundWhileStatement>();
        bound->location = syntax->location;
        
        bound->condition = bind_expression(syntax->condition);
        bound->body = bind_statement(syntax->body);
        
        return bound;
    }
    
    BoundForStatement* BoundTreeBuilder::bind_for_statement(ForStmtSyntax* syntax)
    {
        auto bound = arena_.make<BoundForStatement>();
        bound->location = syntax->location;
        
        if (syntax->initializer)
        {
            bound->initializer = bind_statement(syntax->initializer);
        }
        
        if (syntax->condition)
        {
            bound->condition = bind_expression(syntax->condition);
        }
        
        for (auto update : syntax->updates)
        {
            if (auto bound_update = bind_expression(update))
            {
                bound->incrementors.push_back(bound_update);
            }
        }
        
        bound->body = bind_statement(syntax->body);
        
        return bound;
    }
    
    BoundReturnStatement* BoundTreeBuilder::bind_return_statement(ReturnStmtSyntax* syntax)
    {
        auto bound = arena_.make<BoundReturnStatement>();
        bound->location = syntax->location;
        
        if (syntax->value)
        {
            bound->value = bind_expression(syntax->value);
        }
        
        return bound;
    }
    
    BoundBreakStatement* BoundTreeBuilder::bind_break_statement(BreakStmtSyntax* syntax)
    {
        auto bound = arena_.make<BoundBreakStatement>();
        bound->location = syntax->location;
        return bound;
    }
    
    BoundContinueStatement* BoundTreeBuilder::bind_continue_statement(ContinueStmtSyntax* syntax)
    {
        auto bound = arena_.make<BoundContinueStatement>();
        bound->location = syntax->location;
        return bound;
    }
    
    BoundExpressionStatement* BoundTreeBuilder::bind_expression_statement(ExpressionStmtSyntax* syntax)
    {
        auto bound = arena_.make<BoundExpressionStatement>();
        bound->location = syntax->location;
        bound->expression = bind_expression(syntax->expression);
        return bound;
    }
    
    BoundUsingStatement* BoundTreeBuilder::bind_using_statement(UsingDirectiveSyntax* syntax)
    {
        auto bound = arena_.make<BoundUsingStatement>();
        bound->location = syntax->location;
        
        if (syntax->target)
        {
            bound->namespaceParts = syntax->target->get_parts();
        }
        
        return bound;
    }
    
    // === Expression binding ===
    
    BoundExpression* BoundTreeBuilder::bind_expression(BaseExprSyntax* syntax)
    {
        if (!syntax) return nullptr;
        
        if (auto literal = syntax->as<LiteralExprSyntax>())
            return bind_literal(literal);
        if (auto name = syntax->as<BaseNameExprSyntax>())
            return bind_name(name);
        if (auto binary = syntax->as<BinaryExprSyntax>())
            return bind_binary_expression(binary);
        if (auto unary = syntax->as<UnaryExprSyntax>())
            return bind_unary_expression(unary);
        if (auto assignment = syntax->as<AssignmentExprSyntax>())
            return bind_assignment_expression(assignment);
        if (auto call = syntax->as<CallExprSyntax>())
            return bind_call_expression(call);
        if (auto member = syntax->as<MemberAccessExprSyntax>())
            return bind_member_access(member);
        if (auto indexer = syntax->as<IndexerExprSyntax>())
            return bind_index_expression(indexer);
        if (auto conditional = syntax->as<ConditionalExprSyntax>())
            return bind_conditional_expression(conditional);
        if (auto cast = syntax->as<CastExprSyntax>())
            return bind_cast_expression(cast);
        if (auto new_expr = syntax->as<NewExprSyntax>())
            return bind_new_expression(new_expr);
        if (auto this_expr = syntax->as<ThisExprSyntax>())
            return bind_this_expression(this_expr);
        if (auto array = syntax->as<ArrayLiteralExprSyntax>())
            return bind_array_creation(array);
        if (auto typeof_expr = syntax->as<TypeOfExprSyntax>())
            return bind_typeof_expression(typeof_expr);
        if (auto sizeof_expr = syntax->as<SizeOfExprSyntax>())
            return bind_sizeof_expression(sizeof_expr);
        if (auto paren = syntax->as<ParenthesizedExprSyntax>())
            return bind_parenthesized_expression(paren);
            
        return nullptr;
    }
    
    BoundLiteralExpression* BoundTreeBuilder::bind_literal(LiteralExprSyntax* syntax)
    {
        auto bound = arena_.make<BoundLiteralExpression>();
        bound->location = syntax->location;
        bound->literalKind = syntax->kind;
        
        // Store the constant value
        switch (syntax->kind)
        {
        case LiteralKind::I32:
        case LiteralKind::I64:
        case LiteralKind::I8:
        case LiteralKind::I16:
            bound->constantValue = static_cast<int64_t>(std::stoll(std::string(syntax->value)));
            break;
        case LiteralKind::U32:
        case LiteralKind::U64:
        case LiteralKind::U8:
        case LiteralKind::U16:
            bound->constantValue = static_cast<uint64_t>(std::stoull(std::string(syntax->value)));
            break;
        case LiteralKind::F32:
        case LiteralKind::F64:
            bound->constantValue = std::stod(std::string(syntax->value));
            break;
        case LiteralKind::Bool:
            bound->constantValue = (syntax->value == "true");
            break;
        case LiteralKind::Char:
            if (syntax->value.length() >= 3)
            {
                bound->constantValue = static_cast<int64_t>(syntax->value[1]);
            }
            break;
        case LiteralKind::String:
            bound->constantValue = std::string(syntax->value);
            break;
        case LiteralKind::Null:
            bound->constantValue = std::monostate{};
            break;
        default:
            break;
        }
        
        return bound;
    }
    
    BoundNameExpression* BoundTreeBuilder::bind_name(BaseNameExprSyntax* syntax)
    {
        auto bound = arena_.make<BoundNameExpression>();
        bound->location = syntax->location;
        bound->parts = syntax->get_parts();
        return bound;
    }
    
    BoundBinaryExpression* BoundTreeBuilder::bind_binary_expression(BinaryExprSyntax* syntax)
    {
        auto bound = arena_.make<BoundBinaryExpression>();
        bound->location = syntax->location;
        bound->left = bind_expression(syntax->left);
        bound->right = bind_expression(syntax->right);
        bound->operatorKind = syntax->op;
        return bound;
    }
    
    BoundUnaryExpression* BoundTreeBuilder::bind_unary_expression(UnaryExprSyntax* syntax)
    {
        auto bound = arena_.make<BoundUnaryExpression>();
        bound->location = syntax->location;
        bound->operand = bind_expression(syntax->operand);
        bound->operatorKind = syntax->op;
        return bound;
    }
    
    BoundAssignmentExpression* BoundTreeBuilder::bind_assignment_expression(AssignmentExprSyntax* syntax)
    {
        auto bound = arena_.make<BoundAssignmentExpression>();
        bound->location = syntax->location;
        bound->target = bind_expression(syntax->target);
        bound->value = bind_expression(syntax->value);
        bound->operatorKind = syntax->op;
        return bound;
    }
    
    BoundCallExpression* BoundTreeBuilder::bind_call_expression(CallExprSyntax* syntax)
    {
        auto bound = arena_.make<BoundCallExpression>();
        bound->location = syntax->location;
        bound->callee = bind_expression(syntax->callee);
        
        for (auto arg : syntax->arguments)
        {
            if (auto bound_arg = bind_expression(arg))
            {
                bound->arguments.push_back(bound_arg);
            }
        }
        
        return bound;
    }
    
    BoundMemberAccessExpression* BoundTreeBuilder::bind_member_access(MemberAccessExprSyntax* syntax)
    {
        auto bound = arena_.make<BoundMemberAccessExpression>();
        bound->location = syntax->location;
        bound->object = bind_expression(syntax->object);
        
        if (syntax->member)
        {
            bound->memberName = syntax->member->get_name();
        }
        
        return bound;
    }
    
    BoundIndexExpression* BoundTreeBuilder::bind_index_expression(IndexerExprSyntax* syntax)
    {
        auto bound = arena_.make<BoundIndexExpression>();
        bound->location = syntax->location;
        bound->object = bind_expression(syntax->object);
        bound->index = bind_expression(syntax->index);
        return bound;
    }
    
    BoundConditionalExpression* BoundTreeBuilder::bind_conditional_expression(ConditionalExprSyntax* syntax)
    {
        auto bound = arena_.make<BoundConditionalExpression>();
        bound->location = syntax->location;
        bound->condition = bind_expression(syntax->condition);
        bound->thenExpression = bind_expression(syntax->thenExpr);
        bound->elseExpression = bind_expression(syntax->elseExpr);
        return bound;
    }
    
    BoundCastExpression* BoundTreeBuilder::bind_cast_expression(CastExprSyntax* syntax)
    {
        auto bound = arena_.make<BoundCastExpression>();
        bound->location = syntax->location;
        bound->expression = bind_expression(syntax->expression);
        bound->targetTypeExpression = bind_type_expression(syntax->targetType);
        return bound;
    }
    
    BoundNewExpression* BoundTreeBuilder::bind_new_expression(NewExprSyntax* syntax)
    {
        auto bound = arena_.make<BoundNewExpression>();
        bound->location = syntax->location;
        bound->typeExpression = bind_type_expression(syntax->type);
        
        for (auto arg : syntax->arguments)
        {
            if (auto bound_arg = bind_expression(arg))
            {
                bound->arguments.push_back(bound_arg);
            }
        }
        
        return bound;
    }
    
    BoundThisExpression* BoundTreeBuilder::bind_this_expression(ThisExprSyntax* syntax)
    {
        auto bound = arena_.make<BoundThisExpression>();
        bound->location = syntax->location;
        return bound;
    }
    
    BoundArrayCreationExpression* BoundTreeBuilder::bind_array_creation(ArrayLiteralExprSyntax* syntax)
    {
        auto bound = arena_.make<BoundArrayCreationExpression>();
        bound->location = syntax->location;
        
        for (auto elem : syntax->elements)
        {
            if (auto bound_elem = bind_expression(elem))
            {
                bound->initializers.push_back(bound_elem);
            }
        }
        
        return bound;
    }
    
    BoundTypeOfExpression* BoundTreeBuilder::bind_typeof_expression(TypeOfExprSyntax* syntax)
    {
        auto bound = arena_.make<BoundTypeOfExpression>();
        bound->location = syntax->location;
        bound->typeExpression = bind_type_expression(syntax->type);
        return bound;
    }
    
    BoundSizeOfExpression* BoundTreeBuilder::bind_sizeof_expression(SizeOfExprSyntax* syntax)
    {
        auto bound = arena_.make<BoundSizeOfExpression>();
        bound->location = syntax->location;
        bound->typeExpression = bind_type_expression(syntax->type);
        return bound;
    }
    
    BoundParenthesizedExpression* BoundTreeBuilder::bind_parenthesized_expression(ParenthesizedExprSyntax* syntax)
    {
        auto bound = arena_.make<BoundParenthesizedExpression>();
        bound->location = syntax->location;
        bound->expression = bind_expression(syntax->expression);
        return bound;
    }
    
    // === Type expression binding ===
    
    BoundTypeExpression* BoundTreeBuilder::bind_type_expression(BaseExprSyntax* syntax)
    {
        if (!syntax) return nullptr;
        
        auto bound = arena_.make<BoundTypeExpression>();
        bound->location = syntax->location;
        
        if (auto name = syntax->as<BaseNameExprSyntax>())
        {
            bound->parts = name->get_parts();
        }
        else if (auto array_type = syntax->as<ArrayTypeSyntax>())
        {
            // For array types, bind the element type
            if (auto element_type = bind_type_expression(array_type->baseType))
            {
                bound->parts.push_back("[]");  // Marker for array
                bound->typeArguments.push_back(element_type);
            }
        }
        else if (auto ptr_type = syntax->as<PointerTypeSyntax>())
        {
            // For pointer types
            if (auto pointee = bind_type_expression(ptr_type->baseType))
            {
                bound->parts.push_back("*");  // Marker for pointer
                bound->typeArguments.push_back(pointee);
            }
        }
        
        return bound;
    }
    
} // namespace Bryo