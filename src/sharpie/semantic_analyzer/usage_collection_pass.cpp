#include "sharpie/semantic_analyzer/semantic_analyzer.hpp"
#include "sharpie/common/logger.hpp"
#include <algorithm> // For std::find

using namespace Mycelium::Scripting::Common; // For Logger macros

namespace Mycelium::Scripting::Lang
{

    // ============================================================================
    // Helper Functions
    // ============================================================================

    // Helper to get the string name from a TypeNameNode (simplified)
    std::string get_type_name_str(std::shared_ptr<TypeNameNode> node)
    {
        if (node)
        {
            if (auto ident = std::get_if<std::shared_ptr<IdentifierNode>>(&node->name_segment))
            {
                return (*ident)->name;
            }
            // TODO: Handle QualifiedNameNode by resolving it
        }
        return "unknown";
    }

    // ============================================================================
    // Pass 3: Type Checking and Usage Collection
    // ============================================================================

    void SemanticAnalyzer::collect_usages_and_type_check(std::shared_ptr<AstNode> node)
    {
        if (!node)
            return;

        // Dispatch to the correct visitor based on node type
        if (auto cu = std::dynamic_pointer_cast<CompilationUnitNode>(node))
            return collect_usages_and_type_check(cu);
        if (auto ns = std::dynamic_pointer_cast<NamespaceDeclarationNode>(node))
            return collect_usages_and_type_check(ns);
        if (auto cd = std::dynamic_pointer_cast<ClassDeclarationNode>(node))
            return collect_usages_and_type_check(cd);
        // Add other top-level nodes if necessary
    }

    void SemanticAnalyzer::collect_usages_and_type_check(std::shared_ptr<CompilationUnitNode> node)
    {
        if (!node)
            return;

        // Analyze all top-level members
        for (const auto &member : node->members)
        {
            collect_usages_and_type_check(member);
        }
    }

    void SemanticAnalyzer::collect_usages_and_type_check(std::shared_ptr<NamespaceDeclarationNode> node)
    {
        if (!node)
            return;

        auto old_namespace = context->currentNamespaceName;
        if (!old_namespace.empty())
        {
            context->currentNamespaceName += "." + node->name->name;
        }
        else
        {
            context->currentNamespaceName = node->name->name;
        }

        // Analyze all namespace members
        for (const auto &member : node->members)
        {
            collect_usages_and_type_check(member);
        }

        context->currentNamespaceName = old_namespace;
    }

    void SemanticAnalyzer::collect_usages_and_type_check(std::shared_ptr<ClassDeclarationNode> node)
    {
        if (!node)
            return;

        std::string class_name = node->name->name;
        if (!context->currentNamespaceName.empty())
        {
            class_name = context->currentNamespaceName + "." + class_name;
        }
        context->currentClassName = class_name;

        // Analyze all methods in the class
        for (const auto &member : node->members)
        {
            if (auto method_decl = std::dynamic_pointer_cast<MethodDeclarationNode>(member))
            {
                collect_usages_and_type_check(method_decl, class_name);
            }
            else if (auto ctor_decl = std::dynamic_pointer_cast<ConstructorDeclarationNode>(member))
            {
                collect_usages_and_type_check(ctor_decl, class_name);
            }
            else if (auto dtor_decl = std::dynamic_pointer_cast<DestructorDeclarationNode>(member))
            {
                collect_usages_and_type_check(dtor_decl, class_name);
            }
        }
        context->currentClassName.clear();
    }

    void SemanticAnalyzer::collect_usages_and_type_check(std::shared_ptr<MethodDeclarationNode> node, const std::string &class_name)
    {
        if (!node || !node->body.has_value())
        {
            return; // No body to analyze
        }

        // Set current method context
        context->currentMethodName = node->name->name;

        // Check if method is static
        context->inStaticMethod = false;
        for (const auto &modifier : node->modifiers)
        {
            if (modifier.first == ModifierKind::Static)
            {
                context->inStaticMethod = true;
                break;
            }
        }
        context->inInstanceMethod = !context->inStaticMethod;

        // Push enhanced method scope
        push_semantic_scope(class_name + "." + node->name->name);

        // Add parameters to scope
        for (const auto &param : node->parameters)
        {
            if (param->type && param->name)
            {
                SymbolTable::VariableSymbol param_symbol;
                param_symbol.name = param->name->name;
                param_symbol.type = param->type;
                param_symbol.declaration_location = param->location.value_or(SourceLocation{});
                param_symbol.is_parameter = true;
                param_symbol.owning_scope = context->getFullScopePath();
                param_symbol.is_definitely_assigned = true; // Parameters are always assigned

                ir->symbol_table.declare_variable(param_symbol);
            }
        }

        // Analyze method body
        analyze_statement(node->body.value());

        pop_semantic_scope();

        // Reset context
        context->currentMethodName.clear();
        context->inStaticMethod = false;
        context->inInstanceMethod = false;
    }

    void SemanticAnalyzer::collect_usages_and_type_check(std::shared_ptr<ConstructorDeclarationNode> node, const std::string &class_name)
    {
        if (!node || !node->body.has_value())
        {
            return; // No body to analyze
        }

        context->currentMethodName = "%ctor";
        context->inConstructor = true;
        context->inInstanceMethod = true; // A constructor is an instance method
        context->inStaticMethod = false;  // FIX: Explicitly set static context to false

        push_semantic_scope(class_name + ".%ctor");

        for (const auto &param : node->parameters)
        {
            if (param->type && param->name)
            {
                SymbolTable::VariableSymbol param_symbol;
                param_symbol.name = param->name->name;
                param_symbol.type = param->type;
                param_symbol.declaration_location = param->location.value_or(SourceLocation{});
                param_symbol.is_parameter = true;
                param_symbol.is_definitely_assigned = true;
                ir->symbol_table.declare_variable(param_symbol);
            }
        }

        analyze_statement(node->body.value());
        pop_semantic_scope();

        context->currentMethodName.clear();
        context->inConstructor = false;
        context->inInstanceMethod = false;
    }

    void SemanticAnalyzer::collect_usages_and_type_check(std::shared_ptr<DestructorDeclarationNode> node, const std::string &class_name)
    {
        if (!node || !node->body.has_value())
        {
            return;
        }

        context->currentMethodName = "%dtor";
        context->inInstanceMethod = true;
        context->inStaticMethod = false;

        push_semantic_scope(class_name + ".%dtor");
        analyze_statement(node->body.value());
        pop_semantic_scope();

        context->currentMethodName.clear();
        context->inInstanceMethod = false;
    }

    // ============================================================================
    // Statement Analysis
    // ============================================================================

    void SemanticAnalyzer::analyze_statement(std::shared_ptr<StatementNode> node)
    {
        if (!node)
            return;

        if (auto block_stmt = std::dynamic_pointer_cast<BlockStatementNode>(node))
        {
            analyze_statement(block_stmt);
        }
        else if (auto var_decl = std::dynamic_pointer_cast<LocalVariableDeclarationStatementNode>(node))
        {
            analyze_statement(var_decl);
        }
        else if (auto expr_stmt = std::dynamic_pointer_cast<ExpressionStatementNode>(node))
        {
            analyze_statement(expr_stmt);
        }
        else if (auto if_stmt = std::dynamic_pointer_cast<IfStatementNode>(node))
        {
            analyze_statement(if_stmt);
        }
        else if (auto while_stmt = std::dynamic_pointer_cast<WhileStatementNode>(node))
        {
            analyze_statement(while_stmt);
        }
        else if (auto for_stmt = std::dynamic_pointer_cast<ForStatementNode>(node))
        {
            analyze_statement(for_stmt);
        }
        else if (auto return_stmt = std::dynamic_pointer_cast<ReturnStatementNode>(node))
        {
            analyze_statement(return_stmt);
        }
        else if (auto break_stmt = std::dynamic_pointer_cast<BreakStatementNode>(node))
        {
            analyze_statement(break_stmt);
        }
        else if (auto continue_stmt = std::dynamic_pointer_cast<ContinueStatementNode>(node))
        {
            analyze_statement(continue_stmt);
        }
    }

    void SemanticAnalyzer::analyze_statement(std::shared_ptr<BlockStatementNode> node)
    {
        if (!node)
            return;

        push_semantic_scope("block_" + std::to_string(context->currentScopeDepth + 1));
        for (const auto &stmt : node->statements)
        {
            analyze_statement(stmt);
        }
        pop_semantic_scope();
    }

    void SemanticAnalyzer::analyze_statement(std::shared_ptr<LocalVariableDeclarationStatementNode> node)
    {
        if (!node || !node->type)
        {
            add_error("Invalid variable declaration", node ? node->location : std::nullopt);
            return;
        }

        // Record usage of the type in the declaration itself
        record_usage(get_type_name_str(node->type), UsageKind::TypeReference, node->type->location);

        for (const auto &declarator : node->declarators)
        {
            if (!declarator || !declarator->name)
                continue;

            std::string var_name = declarator->name->name;

            if (ir->symbol_table.is_variable_declared_in_current_scope(var_name))
            {
                add_error("Variable '" + var_name + "' already declared in this scope", declarator->name->location);
                continue;
            }

            SymbolTable::VariableSymbol var_symbol;
            var_symbol.name = var_name;
            var_symbol.type = node->type;
            var_symbol.declaration_location = declarator->name->location.value_or(SourceLocation{});
            var_symbol.owning_scope = context->getFullScopePath();
            var_symbol.is_definitely_assigned = declarator->initializer.has_value();

            if (auto ident = std::get_if<std::shared_ptr<IdentifierNode>>(&node->type->name_segment))
            {
                if (auto *class_symbol = ir->symbol_table.find_class((*ident)->name))
                {
                    var_symbol.class_info = class_symbol;
                }
            }

            ir->symbol_table.declare_variable(var_symbol);

            if (declarator->initializer)
            {
                ExpressionTypeInfo init_type = analyze_expression(declarator->initializer.value());
                if (init_type.type && !are_types_compatible(node->type, init_type.type))
                {
                    add_error("Cannot initialize variable '" + var_name + "' with incompatible type", declarator->initializer.value()->location);
                }
            }
        }
    }

    void SemanticAnalyzer::analyze_statement(std::shared_ptr<ExpressionStatementNode> node)
    {
        if (node && node->expression)
        {
            analyze_expression(node->expression);
        }
    }

    void SemanticAnalyzer::analyze_statement(std::shared_ptr<IfStatementNode> node)
    {
        if (!node)
            return;

        if (node->condition)
        {
            ExpressionTypeInfo cond_type = analyze_expression(node->condition);
            if (cond_type.type && !is_bool_type(cond_type.type))
            {
                add_error("If condition must be of type 'bool'", node->condition->location);
            }
        }

        if (node->thenStatement)
            analyze_statement(node->thenStatement);
        if (node->elseStatement.has_value())
            analyze_statement(node->elseStatement.value());
    }

    void SemanticAnalyzer::analyze_statement(std::shared_ptr<WhileStatementNode> node)
    {
        if (!node)
            return;

        if (node->condition)
        {
            ExpressionTypeInfo cond_type = analyze_expression(node->condition);
            if (cond_type.type && !is_bool_type(cond_type.type))
            {
                add_error("While condition must be of type 'bool'", node->condition->location);
            }
        }

        context->loopStack.push_back("while");
        if (node->body)
            analyze_statement(node->body);
        context->loopStack.pop_back();
    }

    void SemanticAnalyzer::analyze_statement(std::shared_ptr<ForStatementNode> node)
    {
        if (!node)
            return;

        // Push a new scope for the for loop
        push_semantic_scope("for_" + std::to_string(context->currentScopeDepth + 1));
        
        // Analyze initializers
        if (std::holds_alternative<std::shared_ptr<LocalVariableDeclarationStatementNode>>(node->initializers))
        {
            // Variable declaration initializer
            auto var_decl = std::get<std::shared_ptr<LocalVariableDeclarationStatementNode>>(node->initializers);
            analyze_statement(var_decl);
        }
        else if (std::holds_alternative<std::vector<std::shared_ptr<ExpressionNode>>>(node->initializers))
        {
            // Expression list initializers
            auto expr_list = std::get<std::vector<std::shared_ptr<ExpressionNode>>>(node->initializers);
            for (const auto& expr : expr_list)
            {
                analyze_expression(expr);
            }
        }
        
        // Analyze condition if it exists
        if (node->condition.has_value())
        {
            ExpressionTypeInfo cond_type = analyze_expression(node->condition.value());
            if (cond_type.type && !is_bool_type(cond_type.type))
            {
                add_error("For loop condition must be of type 'bool'", node->condition.value()->location);
            }
        }
        
        // Analyze incrementors
        for (const auto& incrementor : node->incrementors)
        {
            analyze_expression(incrementor);
        }
        
        // Analyze body
        context->loopStack.push_back("for");
        if (node->body)
            analyze_statement(node->body);
        context->loopStack.pop_back();
        
        pop_semantic_scope();
    }

    void SemanticAnalyzer::analyze_statement(std::shared_ptr<ReturnStatementNode> node)
    {
        if (!node)
            return;
        
        // Analyze the return expression if it exists
        if (node->expression.has_value())
        {
            analyze_expression(node->expression.value());
        }
    }

    void SemanticAnalyzer::analyze_statement(std::shared_ptr<BreakStatementNode> node)
    {
        if (context->loopStack.empty())
        {
            add_error("'break' statement not within a loop", node->location);
        }
    }

    void SemanticAnalyzer::analyze_statement(std::shared_ptr<ContinueStatementNode> node)
    {
        if (context->loopStack.empty())
        {
            add_error("'continue' statement not within a loop", node->location);
        }
    }

    // ============================================================================
    // Expression Analysis
    // ============================================================================

    SemanticAnalyzer::ExpressionTypeInfo SemanticAnalyzer::analyze_expression(std::shared_ptr<ExpressionNode> node)
    {
        if (!node)
            return ExpressionTypeInfo{};
        if (auto literal = std::dynamic_pointer_cast<LiteralExpressionNode>(node))
            return analyze_expression(literal);
        if (auto identifier = std::dynamic_pointer_cast<IdentifierExpressionNode>(node))
            return analyze_expression(identifier);
        if (auto binary = std::dynamic_pointer_cast<BinaryExpressionNode>(node))
            return analyze_expression(binary);
        if (auto assignment = std::dynamic_pointer_cast<AssignmentExpressionNode>(node))
            return analyze_expression(assignment);
        if (auto unary = std::dynamic_pointer_cast<UnaryExpressionNode>(node))
            return analyze_expression(unary);
        if (auto method_call = std::dynamic_pointer_cast<MethodCallExpressionNode>(node))
            return analyze_expression(method_call);
        if (auto object_creation = std::dynamic_pointer_cast<ObjectCreationExpressionNode>(node))
            return analyze_expression(object_creation);
        if (auto this_expr = std::dynamic_pointer_cast<ThisExpressionNode>(node))
            return analyze_expression(this_expr);
        if (auto cast = std::dynamic_pointer_cast<CastExpressionNode>(node))
            return analyze_expression(cast);
        if (auto member_access = std::dynamic_pointer_cast<MemberAccessExpressionNode>(node))
            return analyze_expression(member_access);
        if (auto parenthesized = std::dynamic_pointer_cast<ParenthesizedExpressionNode>(node))
            return analyze_expression(parenthesized);

        add_error("Unsupported expression type in semantic analysis", node->location);
        return ExpressionTypeInfo{};
    }

    SemanticAnalyzer::ExpressionTypeInfo SemanticAnalyzer::analyze_expression(std::shared_ptr<LiteralExpressionNode> node)
    {
        if (!node)
            return ExpressionTypeInfo{};
        switch (node->kind)
        {
        case LiteralKind::Integer:
            return ExpressionTypeInfo{create_primitive_type("int")};
        case LiteralKind::Long:
            return ExpressionTypeInfo{create_primitive_type("long")};
        case LiteralKind::Float:
            return ExpressionTypeInfo{create_primitive_type("float")};
        case LiteralKind::Double:
            return ExpressionTypeInfo{create_primitive_type("double")};
        case LiteralKind::Boolean:
            return ExpressionTypeInfo{create_primitive_type("bool")};
        case LiteralKind::Char:
            return ExpressionTypeInfo{create_primitive_type("char")};
        case LiteralKind::String:
            return ExpressionTypeInfo{create_primitive_type("string")};
        case LiteralKind::Null:
            return ExpressionTypeInfo{create_primitive_type("null")};
        default:
            add_error("Unknown literal kind", node->location);
            return ExpressionTypeInfo{};
        }
    }

    SemanticAnalyzer::ExpressionTypeInfo SemanticAnalyzer::analyze_expression(std::shared_ptr<IdentifierExpressionNode> node)
    {
        if (!node || !node->identifier)
            return ExpressionTypeInfo{};
        std::string name = node->identifier->name;

        // Check if it's a local variable or parameter
        if (auto *var = ir->symbol_table.find_variable(name))
        {
            ir->symbol_table.mark_variable_used(name);
            record_usage(name, UsageKind::Read, node->location);
            return {var->type, var->class_info, true};
        }

        // Check for implicit 'this' field access
        if (context->inInstanceMethod && !context->currentClassName.empty())
        {
            if (auto *field = ir->symbol_table.find_field_in_class(context->currentClassName, name))
            {
                ir->symbol_table.mark_variable_used(name);
                std::string qualified_name = context->currentClassName + "." + name;
                record_usage(qualified_name, UsageKind::Read, node->location);
                return {field->type, field->class_info, true};
            }
        }

        // Check if it's a class name (for static access)
        if (auto *class_sym = ir->symbol_table.find_class(name))
        {
            record_usage(name, UsageKind::TypeReference, node->location);
            auto type_node = std::make_shared<TypeNameNode>();
            type_node->name_segment = node->identifier;
            return {type_node, class_sym, false};
        }

        // Check for extern functions
        if (auto *extern_func = ir->symbol_table.find_method(name))
        {
            if (extern_func->is_external)
            {
                record_usage(name, UsageKind::Read, node->location);
                return {create_primitive_type("function_pointer"), nullptr, false};
            }
        }

        // FIX: Check if the identifier is a namespace prefix
        const auto &all_classes = ir->symbol_table.get_classes();
        for (const auto &[class_name, class_symbol] : all_classes)
        {
            if (class_name.rfind(name + ".", 0) == 0)
            { // Check if `name` is a prefix
                ExpressionTypeInfo ns_info;
                ns_info.namespace_path = name;
                return ns_info; // It's a namespace. Return a special type.
            }
        }

        add_error("Undefined variable, type, or function: " + name, node->identifier->location);
        return ExpressionTypeInfo{};
    }

    SemanticAnalyzer::ExpressionTypeInfo SemanticAnalyzer::analyze_expression(std::shared_ptr<BinaryExpressionNode> node)
    {
        if (!node || !node->left || !node->right)
            return ExpressionTypeInfo{};

        ExpressionTypeInfo left = analyze_expression(node->left);
        ExpressionTypeInfo right = analyze_expression(node->right);

        if (!left.type || !right.type)
            return ExpressionTypeInfo{};

        switch (node->opKind)
        {
        case BinaryOperatorKind::Add:
            if (is_string_type(left.type) || is_string_type(right.type))
                return ExpressionTypeInfo{create_primitive_type("string")};
            if (is_numeric_type(left.type) && is_numeric_type(right.type))
                return ExpressionTypeInfo{promote_numeric_types(left.type, right.type)};
            break;

        case BinaryOperatorKind::Subtract:
        case BinaryOperatorKind::Multiply:
        case BinaryOperatorKind::Divide:
        case BinaryOperatorKind::Modulo:
            if (is_numeric_type(left.type) && is_numeric_type(right.type))
                return ExpressionTypeInfo{promote_numeric_types(left.type, right.type)};
            break;

        case BinaryOperatorKind::Equals:
        case BinaryOperatorKind::NotEquals:
            if (are_types_compatible(left.type, right.type))
                return ExpressionTypeInfo{create_primitive_type("bool")};
            break;

        case BinaryOperatorKind::LessThan:
        case BinaryOperatorKind::GreaterThan:
        case BinaryOperatorKind::LessThanOrEqual:
        case BinaryOperatorKind::GreaterThanOrEqual:
            if (is_numeric_type(left.type) && is_numeric_type(right.type))
                return ExpressionTypeInfo{create_primitive_type("bool")};
            break;

        case BinaryOperatorKind::LogicalAnd:
        case BinaryOperatorKind::LogicalOr:
            if (is_bool_type(left.type) && is_bool_type(right.type))
                return ExpressionTypeInfo{create_primitive_type("bool")};
            break;
        }

        add_error("Operator cannot be applied to these operand types", node->location);
        return ExpressionTypeInfo{};
    }

    SemanticAnalyzer::ExpressionTypeInfo SemanticAnalyzer::analyze_expression(std::shared_ptr<AssignmentExpressionNode> node)
    {
        if (!node || !node->target || !node->source)
            return ExpressionTypeInfo{};

        ExpressionTypeInfo target = analyze_expression(node->target);
        ExpressionTypeInfo source = analyze_expression(node->source);

        // After analyzing the target, if it was an identifier, its last usage
        // was recorded as a Read. We now find and correct it to a Write.
        if (auto ident_expr = std::dynamic_pointer_cast<IdentifierExpressionNode>(node->target))
        {
            std::string name = ident_expr->identifier->name;
            // Find last usage and update kind
            auto it = ir->usage_graph.find(name);
            if (it != ir->usage_graph.end() && !it->second.empty())
            {
                it->second.back().kind = UsageKind::Write;
            }
        }
        else if (auto member_access = std::dynamic_pointer_cast<MemberAccessExpressionNode>(node->target))
        {
            // Handle field write
            ExpressionTypeInfo access_target = analyze_expression(member_access->target);
            if (access_target.class_info)
            {
                std::string qualified_field_name = access_target.class_info->name + "." + member_access->memberName->name;
                auto it = ir->usage_graph.find(qualified_field_name);
                if (it != ir->usage_graph.end() && !it->second.empty())
                {
                    it->second.back().kind = UsageKind::Write;
                }
            }
        }

        if (!target.type || !source.type)
        {
            return ExpressionTypeInfo{}; // Errors already reported
        }

        if (!target.is_lvalue)
        {
            add_error("The left-hand side of an assignment must be a variable, property or indexer", node->target->location);
            return ExpressionTypeInfo{};
        }

        if (!are_types_compatible(target.type, source.type))
        {
            add_error("Cannot implicitly convert type '...' to '...'", node->location);
            return ExpressionTypeInfo{};
        }

        target.is_lvalue = false;
        return target;
    }

    SemanticAnalyzer::ExpressionTypeInfo SemanticAnalyzer::analyze_expression(std::shared_ptr<UnaryExpressionNode> node)
    {
        if (!node || !node->operand)
            return {};

        ExpressionTypeInfo operand_info = analyze_expression(node->operand);
        if (!operand_info.type)
            return {}; // Error already reported

        switch (node->opKind)
        {
        case UnaryOperatorKind::LogicalNot:
            if (!is_bool_type(operand_info.type))
            {
                add_error("Operator '!' cannot be applied to operand of this type", node->operand->location);
                return {};
            }
            return {create_primitive_type("bool")};

        case UnaryOperatorKind::UnaryMinus:
        case UnaryOperatorKind::UnaryPlus:
            if (!is_numeric_type(operand_info.type))
            {
                add_error("Operator '-' or '+' cannot be applied to operand of this type", node->operand->location);
                return {};
            }
            return operand_info;

        case UnaryOperatorKind::PreIncrement:
        case UnaryOperatorKind::PostIncrement:
        case UnaryOperatorKind::PreDecrement:
        case UnaryOperatorKind::PostDecrement:
            if (!is_numeric_type(operand_info.type))
            {
                add_error("Increment/decrement operators can only be applied to numeric types", node->operand->location);
                return {};
            }
            if (!operand_info.is_lvalue)
            {
                add_error("The operand of an increment or decrement operator must be a variable, property or indexer", node->operand->location);
                return {};
            }
            // This is both a read and a write. We record the write.
            if (auto ident_expr = std::dynamic_pointer_cast<IdentifierExpressionNode>(node->operand))
            {
                record_usage(ident_expr->identifier->name, UsageKind::Write, node->operand->location);
            }
            return operand_info;
        default:
            add_error("Unsupported unary operator", node->location);
            return {};
        }
    }

    SemanticAnalyzer::ExpressionTypeInfo SemanticAnalyzer::analyze_expression(std::shared_ptr<MethodCallExpressionNode> node)
    {
        if (!node || !node->target)
            return ExpressionTypeInfo{};

        ExpressionTypeInfo target_info;
        std::string method_name;
        bool is_static_call_on_type = false;
        std::string class_name_for_call;

        // Step 1: Resolve the target of the call to find the method name and its class.
        if (auto member_access = std::dynamic_pointer_cast<MemberAccessExpressionNode>(node->target))
        {
            // Case: myInstance.method() OR MyClass.staticMethod()
            method_name = member_access->memberName->name;
            target_info = analyze_expression(member_access->target);

            if (target_info.class_info)
            {
                class_name_for_call = target_info.class_info->name;
                // An l-value means it's an instance (e.g., a variable), not a raw type name.
                is_static_call_on_type = !target_info.is_lvalue;
            }
        }
        else if (auto identifier = std::dynamic_pointer_cast<IdentifierExpressionNode>(node->target))
        {
            // Case: myMethod() OR extern_function() OR staticMethodInSameClass()
            method_name = identifier->identifier->name;

            // Check for an extern function first.
            if (auto *extern_func = ir->symbol_table.find_method(method_name))
            {
                if (extern_func->is_external)
                {
                    // Argument checking for extern function
                    size_t provided_arg_count = node->argumentList ? node->argumentList->arguments.size() : 0;
                    if (provided_arg_count != extern_func->parameters.size())
                    {
                        add_error("External function '" + method_name + "' expects " + std::to_string(extern_func->parameters.size()) + " arguments, but " + std::to_string(provided_arg_count) + " were provided.", node->location);
                    }
                    else if (node->argumentList)
                    {
                        for (size_t i = 0; i < provided_arg_count; ++i)
                        {
                            auto arg_expr = node->argumentList->arguments[i]->expression;
                            ExpressionTypeInfo arg_info = analyze_expression(arg_expr);
                            auto &param_decl = extern_func->parameters[i];
                            if (!are_types_compatible(param_decl->type, arg_info.type))
                            {
                                add_error("Argument " + std::to_string(i + 1) + " in call to extern function '" + method_name + "' has incompatible type.", arg_expr->location);
                            }
                        }
                    }
                    record_usage(method_name, UsageKind::Call, node->location);
                    return {extern_func->return_type};
                }
            }

            // If not extern, it's an implicit call within the current class context.
            if (context->currentClassName.empty())
            {
                add_error("Cannot make implicit call to '" + method_name + "' outside of a class context.", node->location);
                return {};
            }
            class_name_for_call = context->currentClassName;
            is_static_call_on_type = context->inStaticMethod;
        }
        else
        {
            add_error("Unsupported method call target.", node->target->location);
            return {};
        }

        // Step 2: Validate the call using the resolved class and method name.
        if (class_name_for_call.empty())
        {
            add_error("Could not determine class for method call '" + method_name + "'.", node->target->location);
            return {};
        }

        auto *method_symbol = ir->symbol_table.find_method_in_class(class_name_for_call, method_name);
        if (!method_symbol)
        {
            add_error("Method '" + method_name + "' not found in class '" + class_name_for_call + "'.", node->target->location);
            return {};
        }

        if (is_static_call_on_type && !method_symbol->is_static)
        {
            add_error("An object reference is required for the non-static method '" + method_name + "'.", node->target->location);
            return {};
        }
        if (!is_static_call_on_type && method_symbol->is_static)
        {
            add_error("Cannot call static method '" + method_name + "' on an instance. Use the type name instead.", node->target->location);
            return {};
        }

        // Step 3: Check arguments.
        size_t provided_arg_count = node->argumentList ? node->argumentList->arguments.size() : 0;
        if (provided_arg_count != method_symbol->parameters.size())
        {
            add_error("Method '" + method_name + "' expects " + std::to_string(method_symbol->parameters.size()) + " arguments, but " + std::to_string(provided_arg_count) + " were provided.", node->location);
            return {};
        }

        if (node->argumentList)
        {
            for (size_t i = 0; i < provided_arg_count; ++i)
            {
                auto arg_expr = node->argumentList->arguments[i]->expression;
                ExpressionTypeInfo arg_info = analyze_expression(arg_expr);
                auto &param_decl = method_symbol->parameters[i];
                if (!are_types_compatible(param_decl->type, arg_info.type))
                {
                    add_error("Argument " + std::to_string(i + 1) + " in call to '" + method_name + "' has incompatible type.", arg_expr->location);
                }
            }
        }

        // Step 4: Record usage and return result type.
        record_usage(method_symbol->qualified_name, UsageKind::Call, node->location);
        return {method_symbol->return_type};
    }

    SemanticAnalyzer::ExpressionTypeInfo SemanticAnalyzer::analyze_expression(std::shared_ptr<ObjectCreationExpressionNode> node)
    {
        if (!node || !node->type)
            return {};

        std::string class_name = get_type_name_str(node->type);
        auto *class_symbol = ir->symbol_table.find_class(class_name);
        if (!class_symbol)
        {
            add_error("Type '" + class_name + "' not found.", node->type->location);
            return {};
        }

        // Record usage of the type and the instantiation event
        record_usage(class_name, UsageKind::TypeReference, node->type->location);
        record_usage(class_name, UsageKind::Instantiation, node->location);

        // Find a matching constructor. For now, we assume one constructor or a default one.
        // A full implementation would handle overloaded constructors.
        auto *ctor_symbol = ir->symbol_table.find_method_in_class(class_name, "%ctor");
        size_t provided_arg_count = node->argumentList.has_value() ? node->argumentList.value()->arguments.size() : 0;

        if (!ctor_symbol)
        {
            // No explicit constructor defined, check for default constructor call (0 args)
            if (provided_arg_count > 0)
            {
                add_error("Class '" + class_name + "' does not have a constructor that takes " + std::to_string(provided_arg_count) + " arguments.", node->location);
                return {};
            }
        }
        else
        {
            // An explicit constructor exists, validate against it.
            if (provided_arg_count != ctor_symbol->parameters.size())
            {
                add_error("Constructor for '" + class_name + "' expects " + std::to_string(ctor_symbol->parameters.size()) + " arguments, but " + std::to_string(provided_arg_count) + " were provided.", node->location);
                return {};
            }

            // Type-check each argument against the constructor's parameters.
            if (node->argumentList.has_value())
            {
                for (size_t i = 0; i < provided_arg_count; ++i)
                {
                    auto arg_expr = node->argumentList.value()->arguments[i]->expression;
                    ExpressionTypeInfo arg_info = analyze_expression(arg_expr);
                    auto &param_decl = ctor_symbol->parameters[i];
                    if (!are_types_compatible(param_decl->type, arg_info.type))
                    {
                        add_error("Argument " + std::to_string(i + 1) + " in constructor call for '" + class_name + "' has incompatible type.", arg_expr->location);
                    }
                }
            }

            // Record the usage of the specific constructor being called.
            record_usage(ctor_symbol->qualified_name, UsageKind::Call, node->location);
        }

        return {node->type, class_symbol};
    }

    SemanticAnalyzer::ExpressionTypeInfo SemanticAnalyzer::analyze_expression(std::shared_ptr<ThisExpressionNode> node)
    {
        if (context->inStaticMethod)
        {
            add_error("'this' cannot be used in a static method.", node->location);
            return {};
        }
        if (context->currentClassName.empty())
        {
            add_error("'this' can only be used within a class.", node->location);
            return {};
        }

        auto *class_symbol = ir->symbol_table.find_class(context->currentClassName);
        if (!class_symbol)
        {
            add_error("Internal Error: 'this' used but current class not found.", node->location);
            return {};
        }

        // FIX: 'this' is an r-value that refers to an instance. We mark it as an l-value
        // as a proxy for "is an instance" to distinguish it from a raw type name.
        return {create_primitive_type(context->currentClassName), class_symbol, true};
    }

    SemanticAnalyzer::ExpressionTypeInfo SemanticAnalyzer::analyze_expression(std::shared_ptr<CastExpressionNode> node)
    {
        if (!node || !node->targetType || !node->expression)
            return {};

        // Analyze the expression being casted to get its type.
        ExpressionTypeInfo expr_info = analyze_expression(node->expression);

        // Record that the target type name is being used.
        record_usage(get_type_name_str(node->targetType), UsageKind::TypeReference, node->targetType->location);

        if (!expr_info.type)
        {
            // An error was already reported during the analysis of the inner expression.
            return {};
        }

        std::string source_type_name = get_type_name_str(expr_info.type);
        std::string target_type_name = get_type_name_str(node->targetType);

        // --- Casting Validation Logic ---

        // Rule 1: Allow casting between any two numeric types (e.g., int to float, double to long).
        bool is_source_numeric = is_numeric_type(expr_info.type);
        bool is_target_numeric = is_numeric_type(node->targetType);

        if (is_source_numeric && is_target_numeric)
        {
            // This is a valid numeric cast. The result is the target type.
            return {node->targetType};
        }

        // Rule 2: Allow casting from null to any class type (object) or string.
        if (source_type_name == "null")
        {
            bool is_target_class = ir->symbol_table.find_class(target_type_name) != nullptr;
            bool is_target_string = is_string_type(node->targetType);
            if (is_target_class || is_target_string)
            {
                return {node->targetType};
            }
        }

        // Rule 2.5: Allow casting from primitive types to string (using ToString() methods)
        if (is_string_type(node->targetType))
        {
            // Check if source is a primitive type that has ToString() method
            bool is_source_primitive = (source_type_name == "int" || source_type_name == "bool" || 
                                      source_type_name == "float" || source_type_name == "double" ||
                                      source_type_name == "char" || source_type_name == "long");
            if (is_source_primitive)
            {
                return {node->targetType};
            }
        }

        // Rule 3: Allow explicit downcasting and upcasting in an inheritance hierarchy.
        // (This requires implementing base class tracking, which is planned for the future).
        // For now, we'll check if both are class types as a placeholder.
        auto *source_class = ir->symbol_table.find_class(source_type_name);
        auto *target_class = ir->symbol_table.find_class(target_type_name);

        if (source_class && target_class)
        {
            // In a real implementation, you would check if `source_class` is a base of `target_class`
            // or if `target_class` is a base of `source_class`. For now, we permit any cast
            // between two class types, which is a common (though potentially unsafe) starting point.
            LOG_WARN("Casting between class types ('" + source_type_name + "' to '" + target_type_name + "'). This is currently unchecked for inheritance validity.", "CAST_VALIDATION");
            return {node->targetType, target_class, false};
        }

        // If none of the above rules match, the cast is invalid.
        add_error("Cannot cast from type '" + source_type_name + "' to '" + target_type_name + "'.", node->location);
        return {};
    }

    SemanticAnalyzer::ExpressionTypeInfo SemanticAnalyzer::analyze_expression(std::shared_ptr<MemberAccessExpressionNode> node)
    {
        if (!node || !node->target || !node->memberName)
            return {};

        ExpressionTypeInfo target_info = analyze_expression(node->target);
        std::string member_name = node->memberName->name;

        // FIX: Handle namespace traversal
        if (!target_info.namespace_path.empty())
        {
            std::string new_ns_path = target_info.namespace_path + "." + member_name;

            // Is the new path a full class name?
            if (auto *class_symbol = ir->symbol_table.find_class(new_ns_path))
            {
                record_usage(new_ns_path, UsageKind::TypeReference, node->location);
                auto type_node = create_primitive_type(new_ns_path); // Use string name for type
                return {type_node, class_symbol, false};
            }

            // Is it still a namespace prefix?
            const auto &all_classes = ir->symbol_table.get_classes();
            for (const auto &[class_name, class_symbol] : all_classes)
            {
                if (class_name.rfind(new_ns_path + ".", 0) == 0)
                {
                    ExpressionTypeInfo ns_info;
                    ns_info.namespace_path = new_ns_path;
                    return ns_info;
                }
            }

            add_error("Namespace '" + target_info.namespace_path + "' does not contain '" + member_name + "'.", node->memberName->location);
            return {};
        }

        // Original logic for instance.member or Class.static_member
        if (!target_info.type || !target_info.class_info)
        {
            add_error("The left-hand side of a member access must be a class, struct, or namespace.", node->target->location);
            return {};
        }

        std::string qualified_member_name = target_info.class_info->name + "." + member_name;

        if (auto *field_symbol = ir->symbol_table.find_field_in_class(target_info.class_info->name, member_name))
        {
            ir->symbol_table.mark_variable_used(member_name);
            record_usage(qualified_member_name, UsageKind::Read, node->location);
            return {field_symbol->type, field_symbol->class_info, true};
        }

        if (ir->symbol_table.find_method_in_class(target_info.class_info->name, member_name))
        {
            // This isn't a terminal expression, it's the target of a call.
            // The MethodCallExpression analyzer will handle the 'Call' usage.
            // We record this as a 'Read' of the method group itself.
            record_usage(qualified_member_name, UsageKind::Read, node->location);
            return target_info;
        }

        add_error("Class '" + target_info.class_info->name + "' does not contain a definition for '" + member_name + "'.", node->memberName->location);
        return {};
    }

    SemanticAnalyzer::ExpressionTypeInfo SemanticAnalyzer::analyze_expression(std::shared_ptr<ParenthesizedExpressionNode> node)
    {
        return node ? analyze_expression(node->expression) : ExpressionTypeInfo{};
    }

    // ============================================================================
    // Utility Methods
    // ============================================================================

    bool SemanticAnalyzer::are_types_compatible(std::shared_ptr<TypeNameNode> left, std::shared_ptr<TypeNameNode> right)
    {
        if (!left || !right)
            return false;
        std::string left_name = get_type_name_str(left);
        std::string right_name = get_type_name_str(right);

        if (left_name == "unknown" || right_name == "unknown")
            return false;
        if (left_name == right_name)
            return true;

        if ((left_name == "string" || ir->symbol_table.find_class(left_name)) && right_name == "null")
            return true;

        // Check for inheritance-based compatibility (upcast: derived -> base)
        auto *left_class = ir->symbol_table.find_class(left_name);
        auto *right_class = ir->symbol_table.find_class(right_name);
        
        if (left_class && right_class)
        {
            // Traverse the inheritance hierarchy of the right type (derived class)
            // to see if the left type (base class) is one of its ancestors
            auto *current_class = right_class;
            while (current_class)
            {
                if (current_class->name == left_name)
                {
                    return true; // Found the left type in the inheritance chain
                }
                
                // Move to the base class
                if (current_class->base_class.empty())
                {
                    break; // Reached the top of the hierarchy
                }
                
                current_class = ir->symbol_table.find_class(current_class->base_class);
                if (!current_class)
                {
                    break; // Base class not found (should not happen in valid code)
                }
            }
        }

        const std::vector<std::string> numeric_types = {"int", "long", "float", "double"};
        auto left_it = std::find(numeric_types.begin(), numeric_types.end(), left_name);
        auto right_it = std::find(numeric_types.begin(), numeric_types.end(), right_name);

        if (left_it != numeric_types.end() && right_it != numeric_types.end())
        {
            return std::distance(numeric_types.begin(), right_it) <= std::distance(numeric_types.begin(), left_it);
        }

        return false;
    }

    bool SemanticAnalyzer::is_primitive_type(const std::string &type_name)
    {
        return type_name == "int" || type_name == "long" || type_name == "float" || type_name == "double" ||
               type_name == "bool" || type_name == "char" || type_name == "string" || type_name == "void";
    }

    bool SemanticAnalyzer::is_numeric_type(std::shared_ptr<TypeNameNode> type)
    {
        if (!type)
            return false;
        const std::string name = get_type_name_str(type);
        return name == "int" || name == "long" || name == "float" || name == "double";
    }

    bool SemanticAnalyzer::is_string_type(std::shared_ptr<TypeNameNode> type)
    {
        return type && get_type_name_str(type) == "string";
    }

    bool SemanticAnalyzer::is_bool_type(std::shared_ptr<TypeNameNode> type)
    {
        return type && get_type_name_str(type) == "bool";
    }

    std::shared_ptr<TypeNameNode> SemanticAnalyzer::create_primitive_type(const std::string &type_name)
    {
        auto type_node = std::make_shared<TypeNameNode>();
        auto ident_node = std::make_shared<IdentifierNode>();
        ident_node->name = type_name;
        type_node->name_segment = ident_node;
        return type_node;
    }

    std::shared_ptr<TypeNameNode> SemanticAnalyzer::promote_numeric_types(std::shared_ptr<TypeNameNode> left, std::shared_ptr<TypeNameNode> right)
    {
        const std::vector<std::string> promotion_order = {"int", "long", "float", "double"};
        std::string left_name = get_type_name_str(left);
        std::string right_name = get_type_name_str(right);

        auto left_it = std::find(promotion_order.begin(), promotion_order.end(), left_name);
        auto right_it = std::find(promotion_order.begin(), promotion_order.end(), right_name);

        if (left_it == promotion_order.end() || right_it == promotion_order.end())
        {
            return nullptr;
        }

        return (std::distance(promotion_order.begin(), left_it) > std::distance(promotion_order.begin(), right_it))
                   ? left
                   : right;
    }

} // namespace Mycelium::Scripting::Lang