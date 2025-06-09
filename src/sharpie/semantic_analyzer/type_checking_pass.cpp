#include "sharpie/semantic_analyzer/semantic_analyzer.hpp"
#include "sharpie/common/logger.hpp"

using namespace Mycelium::Scripting::Common; // For Logger macros

namespace Mycelium::Scripting::Lang
{

// ============================================================================
// Semantic Analysis (Pass 2) - Placeholder implementations
// ============================================================================

void SemanticAnalyzer::analyze_semantics(std::shared_ptr<AstNode> node) {
    // TODO: Implement semantic validation in future steps
    // For now, this is a placeholder to maintain the interface
}

void SemanticAnalyzer::analyze_semantics(std::shared_ptr<CompilationUnitNode> node) {
    if (!node) return;
    
    // Analyze all top-level members
    for (const auto& member : node->members) {
        if (auto ns_decl = std::dynamic_pointer_cast<NamespaceDeclarationNode>(member)) {
            analyze_semantics(ns_decl);
        } else if (auto class_decl = std::dynamic_pointer_cast<ClassDeclarationNode>(member)) {
            analyze_semantics(class_decl);
        }
    }
}

void SemanticAnalyzer::analyze_semantics(std::shared_ptr<NamespaceDeclarationNode> node) {
    if (!node) return;
    
    // Analyze all namespace members
    for (const auto& member : node->members) {
        if (auto class_decl = std::dynamic_pointer_cast<ClassDeclarationNode>(member)) {
            analyze_semantics(class_decl);
        }
    }
}

void SemanticAnalyzer::analyze_semantics(std::shared_ptr<ClassDeclarationNode> node) {
    if (!node) return;
    
    std::string class_name = node->name->name;
    
    // Analyze all methods in the class
    for (const auto& member : node->members) {
        if (auto method_decl = std::dynamic_pointer_cast<MethodDeclarationNode>(member)) {
            analyze_semantics(method_decl, class_name);
        } else if (auto ctor_decl = std::dynamic_pointer_cast<ConstructorDeclarationNode>(member)) {
            analyze_semantics(ctor_decl, class_name);
        } else if (auto dtor_decl = std::dynamic_pointer_cast<DestructorDeclarationNode>(member)) {
            analyze_semantics(dtor_decl, class_name);
        }
    }
}

void SemanticAnalyzer::analyze_semantics(std::shared_ptr<MethodDeclarationNode> node, const std::string& class_name) {
    if (!node || !node->body.has_value()) {
        return; // No body to analyze
    }
    
    // Set current method context
    context->currentClassName = class_name;
    context->currentMethodName = node->name->name;
    
    // Check if method is static
    context->inStaticMethod = false;
    context->inInstanceMethod = false;
    for (const auto& modifier : node->modifiers) {
        if (modifier.first == ModifierKind::Static) {
            context->inStaticMethod = true;
            break;
        }
    }
    if (!context->inStaticMethod) {
        context->inInstanceMethod = true;
    }
    
    // Push enhanced method scope
    std::string method_scope_name = class_name + "." + node->name->name;
    push_semantic_scope(method_scope_name);
    
    // Add parameters to scope with enhanced tracking
    for (const auto& param : node->parameters) {
        if (param->type) {
            SymbolTable::VariableSymbol param_symbol;
            param_symbol.name = param->name->name;
            param_symbol.type = param->type;
            param_symbol.declaration_location = param->location.value_or(SourceLocation{});
            param_symbol.is_parameter = true;
            param_symbol.is_field = false;
            param_symbol.owning_scope = context->getFullScopePath();
            param_symbol.is_definitely_assigned = true; // Parameters are always assigned
            
            symbolTable->declare_variable(param_symbol);
            LOG_INFO("Added method parameter: " + param_symbol.name + " in scope: " + param_symbol.owning_scope, "COMPILER");
        }
    }
    
    // Analyze method body
    analyze_statement(node->body.value());
    
    // Pop enhanced method scope
    pop_semantic_scope();
    
    // Reset context
    context->currentClassName.clear();
    context->currentMethodName.clear();
    context->inStaticMethod = false;
    context->inInstanceMethod = false;
}

void SemanticAnalyzer::analyze_semantics(std::shared_ptr<ConstructorDeclarationNode> node, const std::string& class_name) {
    // TODO: Implement in future steps
}

void SemanticAnalyzer::analyze_semantics(std::shared_ptr<DestructorDeclarationNode> node, const std::string& class_name) {
    // TODO: Implement in future steps
}

// ============================================================================
// Statement Analysis (Pass 2) - NEW: Basic type checking implementation
// ============================================================================

void SemanticAnalyzer::analyze_statement(std::shared_ptr<StatementNode> node) {
    if (!node) return;
    
    if (auto block_stmt = std::dynamic_pointer_cast<BlockStatementNode>(node)) {
        analyze_statement(block_stmt);
    } else if (auto var_decl = std::dynamic_pointer_cast<LocalVariableDeclarationStatementNode>(node)) {
        analyze_statement(var_decl);
    } else if (auto expr_stmt = std::dynamic_pointer_cast<ExpressionStatementNode>(node)) {
        analyze_statement(expr_stmt);
    } else if (auto if_stmt = std::dynamic_pointer_cast<IfStatementNode>(node)) {
        analyze_statement(if_stmt);
    } else if (auto while_stmt = std::dynamic_pointer_cast<WhileStatementNode>(node)) {
        analyze_statement(while_stmt);
    } else if (auto for_stmt = std::dynamic_pointer_cast<ForStatementNode>(node)) {
        analyze_statement(for_stmt);
    } else if (auto return_stmt = std::dynamic_pointer_cast<ReturnStatementNode>(node)) {
        analyze_statement(return_stmt);
    } else if (auto break_stmt = std::dynamic_pointer_cast<BreakStatementNode>(node)) {
        analyze_statement(break_stmt);
    } else if (auto continue_stmt = std::dynamic_pointer_cast<ContinueStatementNode>(node)) {
        analyze_statement(continue_stmt);
    }
    // TODO: Add other statement types as needed
}

void SemanticAnalyzer::analyze_statement(std::shared_ptr<BlockStatementNode> node) {
    if (!node) return;
    
    // Push enhanced block scope
    std::string block_scope_name = "block_" + std::to_string(context->currentScopeDepth + 1);
    push_semantic_scope(block_scope_name);
    
    // Analyze all statements in the block
    for (const auto& stmt : node->statements) {
        analyze_statement(stmt);
    }
    
    // Pop enhanced block scope
    pop_semantic_scope();
}

void SemanticAnalyzer::analyze_statement(std::shared_ptr<LocalVariableDeclarationStatementNode> node) {
    if (!node || !node->type) {
        add_error("Invalid variable declaration", node ? node->location.value_or(SourceLocation{}) : SourceLocation{});
        return;
    }
    
    // Check each declarator
    for (const auto& declarator : node->declarators) {
        if (!declarator || !declarator->name) {
            add_error("Invalid variable declarator", node->location.value_or(SourceLocation{}));
            continue;
        }
        
        std::string var_name = declarator->name->name;
        
        // Check for redeclaration in current scope only (not outer scopes)
        if (symbolTable->is_variable_declared_in_current_scope(var_name)) {
            add_error("Variable '" + var_name + "' already declared in this scope", 
                     declarator->name->location.value_or(SourceLocation{}));
            continue;
        }
        
        // Add variable to symbol table
        SymbolTable::VariableSymbol var_symbol;
        var_symbol.name = var_name;
        var_symbol.type = node->type;
        var_symbol.declaration_location = declarator->name->location.value_or(SourceLocation{});
        
        // Enhanced semantic information
        var_symbol.is_parameter = false;
        var_symbol.is_field = false;
        var_symbol.owning_scope = context->getFullScopePath();
        var_symbol.is_definitely_assigned = declarator->initializer.has_value(); // Has initializer
        
        // Look up class info if this is a class type
        if (auto ident = std::get_if<std::shared_ptr<IdentifierNode>>(&node->type->name_segment)) {
            auto* class_symbol = symbolTable->find_class((*ident)->name);
            if (class_symbol) {
                var_symbol.class_info = &class_symbol->type_info;
            }
        }
        
        symbolTable->declare_variable(var_symbol);
        
        LOG_INFO("Declared variable: " + var_name + " in scope: " + var_symbol.owning_scope + 
                " (assigned: " + (var_symbol.is_definitely_assigned ? "yes" : "no") + ")", "COMPILER");
        
        // Type check initializer if present
        if (declarator->initializer) {
            ExpressionTypeInfo init_type = analyze_expression(declarator->initializer.value());
            if (init_type.type && !are_types_compatible(node->type, init_type.type)) {
                add_error("Cannot initialize variable '" + var_name + "' with incompatible type",
                         declarator->initializer.value()->location.value_or(SourceLocation{}));
            }
        }
    }
}

void SemanticAnalyzer::analyze_statement(std::shared_ptr<ExpressionStatementNode> node) {
    if (!node || !node->expression) {
        add_error("Invalid expression statement", node ? node->location.value_or(SourceLocation{}) : SourceLocation{});
        return;
    }
    
    // Analyze the expression for type checking
    analyze_expression(node->expression);
}

void SemanticAnalyzer::analyze_statement(std::shared_ptr<IfStatementNode> node) {
    if (!node) return;
    
    // Type check condition
    if (node->condition) {
        ExpressionTypeInfo cond_type = analyze_expression(node->condition);
        if (cond_type.type && !is_bool_type(cond_type.type)) {
            add_warning("If condition should be boolean type", 
                       node->condition->location.value_or(SourceLocation{}));
        }
    }
    
    // Analyze then branch
    if (node->thenStatement) {
        analyze_statement(node->thenStatement);
    }
    
    // Analyze else branch if present
    if (node->elseStatement.has_value()) {
        analyze_statement(node->elseStatement.value());
    }
}

void SemanticAnalyzer::analyze_statement(std::shared_ptr<WhileStatementNode> node) {
    if (!node) return;
    
    // Type check condition
    if (node->condition) {
        ExpressionTypeInfo cond_type = analyze_expression(node->condition);
        if (cond_type.type && !is_bool_type(cond_type.type)) {
            add_warning("While condition should be boolean type",
                       node->condition->location.value_or(SourceLocation{}));
        }
    }
    
    // Track loop for break/continue validation
    context->loopStack.push_back("while");
    
    // Analyze body
    if (node->body) {
        analyze_statement(node->body);
    }
    
    // Pop loop context
    context->loopStack.pop_back();
}

void SemanticAnalyzer::analyze_statement(std::shared_ptr<ForStatementNode> node) {
    // TODO: Implement for loop analysis
}

void SemanticAnalyzer::analyze_statement(std::shared_ptr<ReturnStatementNode> node) {
    if (!node) return;
    
    // Type check return value if present
    if (node->expression.has_value()) {
        ExpressionTypeInfo return_type = analyze_expression(node->expression.value());
        // TODO: Check against method return type
    }
}

void SemanticAnalyzer::analyze_statement(std::shared_ptr<BreakStatementNode> node) {
    if (context->loopStack.empty()) {
        add_error("'break' statement used outside of loop", node ? node->location.value_or(SourceLocation{}) : SourceLocation{});
    }
}

void SemanticAnalyzer::analyze_statement(std::shared_ptr<ContinueStatementNode> node) {
    if (context->loopStack.empty()) {
        add_error("'continue' statement used outside of loop", node ? node->location.value_or(SourceLocation{}) : SourceLocation{});
    }
}

// ============================================================================
// Expression Analysis - Placeholder implementations
// ============================================================================

SemanticAnalyzer::ExpressionTypeInfo SemanticAnalyzer::analyze_expression(std::shared_ptr<ExpressionNode> node) {
    if (!node) return ExpressionTypeInfo{};
    
    // Dispatch to specific expression analysis methods
    if (auto literal = std::dynamic_pointer_cast<LiteralExpressionNode>(node)) {
        return analyze_expression(literal);
    } else if (auto identifier = std::dynamic_pointer_cast<IdentifierExpressionNode>(node)) {
        return analyze_expression(identifier);
    } else if (auto binary = std::dynamic_pointer_cast<BinaryExpressionNode>(node)) {
        return analyze_expression(binary);
    } else if (auto assignment = std::dynamic_pointer_cast<AssignmentExpressionNode>(node)) {
        return analyze_expression(assignment);
    } else if (auto unary = std::dynamic_pointer_cast<UnaryExpressionNode>(node)) {
        return analyze_expression(unary);
    } else if (auto method_call = std::dynamic_pointer_cast<MethodCallExpressionNode>(node)) {
        return analyze_expression(method_call);
    } else if (auto object_creation = std::dynamic_pointer_cast<ObjectCreationExpressionNode>(node)) {
        return analyze_expression(object_creation);
    } else if (auto this_expr = std::dynamic_pointer_cast<ThisExpressionNode>(node)) {
        return analyze_expression(this_expr);
    } else if (auto cast = std::dynamic_pointer_cast<CastExpressionNode>(node)) {
        return analyze_expression(cast);
    } else if (auto member_access = std::dynamic_pointer_cast<MemberAccessExpressionNode>(node)) {
        return analyze_expression(member_access);
    } else if (auto parenthesized = std::dynamic_pointer_cast<ParenthesizedExpressionNode>(node)) {
        return analyze_expression(parenthesized);
    } else {
        add_error("Unsupported expression type in semantic analysis", node->location.value_or(SourceLocation{}));
        return ExpressionTypeInfo{};
    }
}

SemanticAnalyzer::ExpressionTypeInfo SemanticAnalyzer::analyze_expression(std::shared_ptr<LiteralExpressionNode> node) {
    if (!node) {
        add_error("Null literal expression");
        return ExpressionTypeInfo{};
    }
    
    // Determine type based on literal kind
    std::shared_ptr<TypeNameNode> literal_type;
    
    switch (node->kind) {
        case LiteralKind::Integer:
            literal_type = create_primitive_type("int");
            break;
        case LiteralKind::Long:
            literal_type = create_primitive_type("long");
            break;
        case LiteralKind::Float:
            literal_type = create_primitive_type("float");
            break;
        case LiteralKind::Double:
            literal_type = create_primitive_type("double");
            break;
        case LiteralKind::Boolean:
            literal_type = create_primitive_type("bool");
            break;
        case LiteralKind::Char:
            literal_type = create_primitive_type("char");
            break;
        case LiteralKind::String:
            literal_type = create_primitive_type("string");
            break;
        case LiteralKind::Null:
            // Null has no specific type - it's compatible with any reference type
            literal_type = nullptr;
            break;
        default:
            add_error("Unknown literal kind", node->location.value_or(SourceLocation{}));
            return ExpressionTypeInfo{};
    }
    
    return ExpressionTypeInfo{literal_type};
}

SemanticAnalyzer::ExpressionTypeInfo SemanticAnalyzer::analyze_expression(std::shared_ptr<IdentifierExpressionNode> node) {
    if (!node || !node->identifier) {
        add_error("Null identifier expression");
        return ExpressionTypeInfo{};
    }
    
    std::string identifier_name = node->identifier->name;
    
    // Look up variable in symbol table
    auto* variable_symbol = symbolTable->find_variable(identifier_name);
    if (variable_symbol) {
        // Mark variable as used
        symbolTable->mark_variable_used(identifier_name);
        
        // Return type information
        ExpressionTypeInfo type_info{variable_symbol->type};
        type_info.class_info = variable_symbol->class_info;
        type_info.is_lvalue = true; // Variables are lvalues (can be assigned to)
        return type_info;
    }
    
    // TODO: Check for implicit 'this.field' access in instance methods
    // For now, just report undefined variable
    add_error("Undefined variable: " + identifier_name, node->identifier->location.value_or(SourceLocation{}));
    return ExpressionTypeInfo{};
}

SemanticAnalyzer::ExpressionTypeInfo SemanticAnalyzer::analyze_expression(std::shared_ptr<BinaryExpressionNode> node) {
    if (!node || !node->left || !node->right) {
        add_error("Invalid binary expression", node ? node->location.value_or(SourceLocation{}) : SourceLocation{});
        return ExpressionTypeInfo{};
    }
    
    // Analyze both operands
    ExpressionTypeInfo left_info = analyze_expression(node->left);
    ExpressionTypeInfo right_info = analyze_expression(node->right);
    
    if (!left_info.type || !right_info.type) {
        // Operand analysis failed
        return ExpressionTypeInfo{};
    }
    
    // Type checking based on operator
    switch (node->opKind) {
        case BinaryOperatorKind::Add:
            // String concatenation: string + anything = string
            if (is_string_type(left_info.type) || is_string_type(right_info.type)) {
                return ExpressionTypeInfo{create_primitive_type("string")};
            }
            // Numeric addition: require compatible numeric types
            if (is_numeric_type(left_info.type) && is_numeric_type(right_info.type)) {
                return ExpressionTypeInfo{promote_numeric_types(left_info.type, right_info.type)};
            }
            add_error("Invalid operands for addition", node->location.value_or(SourceLocation{}));
            return ExpressionTypeInfo{};
            
        case BinaryOperatorKind::Subtract:
        case BinaryOperatorKind::Multiply:
        case BinaryOperatorKind::Divide:
        case BinaryOperatorKind::Modulo:
            // Arithmetic operations: require numeric types
            if (is_numeric_type(left_info.type) && is_numeric_type(right_info.type)) {
                return ExpressionTypeInfo{promote_numeric_types(left_info.type, right_info.type)};
            }
            add_error("Invalid operands for arithmetic operation", node->location.value_or(SourceLocation{}));
            return ExpressionTypeInfo{};
            
        case BinaryOperatorKind::Equals:
        case BinaryOperatorKind::NotEquals:
            // Equality: operands must be compatible
            if (are_types_compatible(left_info.type, right_info.type)) {
                return ExpressionTypeInfo{create_primitive_type("bool")};
            }
            add_error("Incompatible types for equality comparison", node->location.value_or(SourceLocation{}));
            return ExpressionTypeInfo{};
            
        case BinaryOperatorKind::LessThan:
        case BinaryOperatorKind::GreaterThan:
        case BinaryOperatorKind::LessThanOrEqual:
        case BinaryOperatorKind::GreaterThanOrEqual:
            // Relational: require comparable types (numeric for now)
            if (is_numeric_type(left_info.type) && is_numeric_type(right_info.type)) {
                return ExpressionTypeInfo{create_primitive_type("bool")};
            }
            add_error("Invalid operands for relational comparison", node->location.value_or(SourceLocation{}));
            return ExpressionTypeInfo{};
            
        case BinaryOperatorKind::LogicalAnd:
        case BinaryOperatorKind::LogicalOr:
            // Logical operations: require boolean operands
            if (is_bool_type(left_info.type) && is_bool_type(right_info.type)) {
                return ExpressionTypeInfo{create_primitive_type("bool")};
            }
            add_error("Logical operators require boolean operands", node->location.value_or(SourceLocation{}));
            return ExpressionTypeInfo{};
            
        default:
            add_error("Unsupported binary operator", node->location.value_or(SourceLocation{}));
            return ExpressionTypeInfo{};
    }
}

SemanticAnalyzer::ExpressionTypeInfo SemanticAnalyzer::analyze_expression(std::shared_ptr<AssignmentExpressionNode> node) {
    if (!node || !node->target || !node->source) {
        add_error("Invalid assignment expression", node ? node->location.value_or(SourceLocation{}) : SourceLocation{});
        return ExpressionTypeInfo{};
    }
    
    // Analyze both sides
    ExpressionTypeInfo target_info = analyze_expression(node->target);
    ExpressionTypeInfo source_info = analyze_expression(node->source);
    
    if (!target_info.type || !source_info.type) {
        // Operand analysis failed, error already reported
        return ExpressionTypeInfo{};
    }
    
    // Check if target side is assignable (lvalue)
    if (!target_info.is_lvalue) {
        add_error("Cannot assign to expression - not an lvalue", node->target->location.value_or(SourceLocation{}));
        return ExpressionTypeInfo{};
    }
    
    // Check type compatibility
    if (!are_types_compatible(target_info.type, source_info.type)) {
        add_error("Cannot assign incompatible types", node->location.value_or(SourceLocation{}));
        return ExpressionTypeInfo{};
    }
    
    // Assignment result has the type of the target operand
    return ExpressionTypeInfo{target_info.type};
}

SemanticAnalyzer::ExpressionTypeInfo SemanticAnalyzer::analyze_expression(std::shared_ptr<UnaryExpressionNode> node) {
    // TODO: Implement in future steps
    return ExpressionTypeInfo{};
}

SemanticAnalyzer::ExpressionTypeInfo SemanticAnalyzer::analyze_expression(std::shared_ptr<MethodCallExpressionNode> node) {
    // TODO: Implement in future steps
    return ExpressionTypeInfo{};
}

SemanticAnalyzer::ExpressionTypeInfo SemanticAnalyzer::analyze_expression(std::shared_ptr<ObjectCreationExpressionNode> node) {
    // TODO: Implement in future steps
    return ExpressionTypeInfo{};
}

SemanticAnalyzer::ExpressionTypeInfo SemanticAnalyzer::analyze_expression(std::shared_ptr<ThisExpressionNode> node) {
    // TODO: Implement in future steps
    return ExpressionTypeInfo{};
}

SemanticAnalyzer::ExpressionTypeInfo SemanticAnalyzer::analyze_expression(std::shared_ptr<CastExpressionNode> node) {
    // TODO: Implement in future steps
    return ExpressionTypeInfo{};
}

SemanticAnalyzer::ExpressionTypeInfo SemanticAnalyzer::analyze_expression(std::shared_ptr<MemberAccessExpressionNode> node) {
    // TODO: Implement in future steps
    return ExpressionTypeInfo{};
}

SemanticAnalyzer::ExpressionTypeInfo SemanticAnalyzer::analyze_expression(std::shared_ptr<ParenthesizedExpressionNode> node) {
    if (!node || !node->expression) {
        add_error("Invalid parenthesized expression", node ? node->location.value_or(SourceLocation{}) : SourceLocation{});
        return ExpressionTypeInfo{};
    }
    
    // Parentheses don't change the type, just analyze the inner expression
    return analyze_expression(node->expression);
}

// ============================================================================
// Utility Methods
// ============================================================================

bool SemanticAnalyzer::are_types_compatible(std::shared_ptr<TypeNameNode> left, std::shared_ptr<TypeNameNode> right) {
    // TODO: Implement proper type compatibility checking
    // For now, just do basic name comparison
    if (!left || !right) return false;
    
    // Extract type names for comparison
    std::string left_name, right_name;
    if (auto left_ident = std::get_if<std::shared_ptr<IdentifierNode>>(&left->name_segment)) {
        left_name = (*left_ident)->name;
    }
    if (auto right_ident = std::get_if<std::shared_ptr<IdentifierNode>>(&right->name_segment)) {
        right_name = (*right_ident)->name;
    }
    
    return left_name == right_name;
}

bool SemanticAnalyzer::is_primitive_type(const std::string& type_name) {
    return primitiveRegistry.is_primitive_simple_name(type_name);
}

bool SemanticAnalyzer::is_numeric_type(std::shared_ptr<TypeNameNode> type) {
    if (!type) return false;
    
    if (auto ident = std::get_if<std::shared_ptr<IdentifierNode>>(&type->name_segment)) {
        const std::string& name = (*ident)->name;
        return name == "int" || name == "long" || name == "float" || name == "double";
    }
    return false;
}

std::shared_ptr<TypeNameNode> SemanticAnalyzer::create_primitive_type(const std::string& type_name) {
    auto type_node = std::make_shared<TypeNameNode>();
    auto ident_node = std::make_shared<IdentifierNode>(type_name);
    type_node->name_segment = ident_node;
    return type_node;
}

bool SemanticAnalyzer::is_string_type(std::shared_ptr<TypeNameNode> type) {
    if (!type) return false;
    if (auto ident = std::get_if<std::shared_ptr<IdentifierNode>>(&type->name_segment)) {
        return (*ident)->name == "string";
    }
    return false;
}

bool SemanticAnalyzer::is_bool_type(std::shared_ptr<TypeNameNode> type) {
    if (!type) return false;
    if (auto ident = std::get_if<std::shared_ptr<IdentifierNode>>(&type->name_segment)) {
        return (*ident)->name == "bool";
    }
    return false;
}

std::shared_ptr<TypeNameNode> SemanticAnalyzer::promote_numeric_types(std::shared_ptr<TypeNameNode> left, std::shared_ptr<TypeNameNode> right) {
    // Simple type promotion rules:
    // double > float > long > int
    std::string left_name, right_name;
    
    if (auto ident = std::get_if<std::shared_ptr<IdentifierNode>>(&left->name_segment)) {
        left_name = (*ident)->name;
    }
    if (auto ident = std::get_if<std::shared_ptr<IdentifierNode>>(&right->name_segment)) {
        right_name = (*ident)->name;
    }
    
    // Promote to highest precision type
    if (left_name == "double" || right_name == "double") {
        return create_primitive_type("double");
    }
    if (left_name == "float" || right_name == "float") {
        return create_primitive_type("float");
    }
    if (left_name == "long" || right_name == "long") {
        return create_primitive_type("long");
    }
    // Default to int
    return create_primitive_type("int");
}

} // namespace Mycelium::Scripting::Lang