#include "codegen/codegen.hpp"
#include "ast/ast.hpp"

namespace Myre {

// Helper to convert semantic type to IR type
IRType CodeGenerator::convert_type(Type* type) {
    if (!type) return IRType::void_type;
    
    if (auto* prim = type->as<PrimitiveType>()) {
        switch (prim->primitive_kind) {
            case PrimitiveType::Void: return IRType::void_type;
            case PrimitiveType::Bool: return IRType::i1_type;
            case PrimitiveType::I8: return IRType::i8_type;
            case PrimitiveType::I32: return IRType::i32_type;
            case PrimitiveType::I64: return IRType::i64_type;
            case PrimitiveType::F32: return IRType::f32_type;
            case PrimitiveType::F64: return IRType::f64_type;
            default: return IRType::i32_type;
        }
    }
    
    // Default to i32 for unknown types
    return IRType::i32_type;
}

ValueRef CodeGenerator::emit_type_conversion(ValueRef value, IRType target_type) {
    // For now, no conversion - assume types match
    return value;
}

void CodeGenerator::emit_function_prologue(FunctionDeclarationNode* node) {
    // Function is already declared, nothing special needed for prologue
}

void CodeGenerator::emit_function_epilogue() {
    // Ensure function has a return
    if (!builder_.has_terminator()) {
        builder_.emit_ret_void();
    }
}

// Expression visitors
void CodeGenerator::visit(LiteralExpressionNode* node) {
    if (!node || !node->token) {
        current_value_ = ValueRef::invalid();
        return;
    }
    
    switch (node->kind) {
        case LiteralKind::Integer: {
            // Parse the integer value
            std::string text(node->token->text);
            int32_t value = std::stoi(text);
            current_value_ = builder_.emit_constant_i32(value);
            break;
        }
        case LiteralKind::Boolean: {
            std::string text(node->token->text);
            bool value = (text == "true");
            current_value_ = builder_.emit_constant_bool(value);
            break;
        }
        default:
            current_value_ = builder_.emit_constant_i32(0);  // Default
            break;
    }
}

void CodeGenerator::visit(IdentifierExpressionNode* node) {
    if (!node || !node->name) {
        current_value_ = ValueRef::invalid();
        return;
    }
    
    std::string name(node->name->name);
    
    // Look up variable in symbol table
    Symbol* symbol = symbol_table_.lookup_symbol(name);
    if (!symbol || symbol->kind != Symbol::Variable) {
        errors_.add_error(SemanticError::SymbolNotFound, "Variable not found: " + name);
        current_value_ = ValueRef::invalid();
        return;
    }
    
    // Load from variable storage
    ValueRef var_ptr = builder_.get_variable(name);
    if (var_ptr.is_valid()) {
        current_value_ = builder_.emit_load(var_ptr);
    } else {
        errors_.add_error(SemanticError::SymbolNotFound, "Variable storage not found: " + name);
        current_value_ = ValueRef::invalid();
    }
}

void CodeGenerator::visit(BinaryExpressionNode* node) {
    if (!node || !node->left || !node->right) {
        current_value_ = ValueRef::invalid();
        return;
    }
    
    // Generate left operand
    node->left->accept(this);
    ValueRef left_val = current_value_;
    
    // Generate right operand  
    node->right->accept(this);
    ValueRef right_val = current_value_;
    
    if (!left_val.is_valid() || !right_val.is_valid()) {
        current_value_ = ValueRef::invalid();
        return;
    }
    
    // Generate operation
    switch (node->op) {
        case BinaryOperator::Add:
            current_value_ = builder_.emit_add(left_val, right_val);
            break;
        case BinaryOperator::Subtract:
            current_value_ = builder_.emit_sub(left_val, right_val);
            break;
        case BinaryOperator::Multiply:
            current_value_ = builder_.emit_mul(left_val, right_val);
            break;
        case BinaryOperator::Divide:
            current_value_ = builder_.emit_div(left_val, right_val);
            break;
        case BinaryOperator::Equal:
            current_value_ = builder_.emit_icmp_eq(left_val, right_val);
            break;
        case BinaryOperator::NotEqual:
            current_value_ = builder_.emit_icmp_ne(left_val, right_val);
            break;
        case BinaryOperator::LessThan:
            current_value_ = builder_.emit_icmp_lt(left_val, right_val);
            break;
        case BinaryOperator::GreaterThan:
            current_value_ = builder_.emit_icmp_gt(left_val, right_val);
            break;
        case BinaryOperator::LogicalAnd:
            current_value_ = builder_.emit_and(left_val, right_val);
            break;
        case BinaryOperator::LogicalOr:
            current_value_ = builder_.emit_or(left_val, right_val);
            break;
        default:
            errors_.add_error(SemanticError::InvalidOperation, "Unsupported binary operator");
            current_value_ = ValueRef::invalid();
            break;
    }
}

void CodeGenerator::visit(UnaryExpressionNode* node) {
    if (!node || !node->operand) {
        current_value_ = ValueRef::invalid();
        return;
    }
    
    // Generate operand
    node->operand->accept(this);
    ValueRef operand_val = current_value_;
    
    if (!operand_val.is_valid()) {
        current_value_ = ValueRef::invalid();
        return;
    }
    
    switch (node->op) {
        case UnaryOperator::LogicalNot:
            current_value_ = builder_.emit_not(operand_val);
            break;
        case UnaryOperator::Minus:
            // Emit 0 - operand
            current_value_ = builder_.emit_sub(builder_.emit_constant_i32(0), operand_val);
            break;
        case UnaryOperator::Plus:
            // Plus is a no-op
            current_value_ = operand_val;
            break;
        default:
            errors_.add_error(SemanticError::InvalidOperation, "Unsupported unary operator");
            current_value_ = ValueRef::invalid();
            break;
    }
}

void CodeGenerator::visit(AssignmentExpressionNode* node) {
    if (!node || !node->left || !node->right) {
        current_value_ = ValueRef::invalid();
        return;
    }
    
    // Generate right-hand side
    node->right->accept(this);
    ValueRef rhs_val = current_value_;
    
    if (!rhs_val.is_valid()) {
        current_value_ = ValueRef::invalid();
        return;
    }
    
    // Handle left-hand side (must be lvalue)
    if (auto* id_expr = node->left->as<IdentifierExpressionNode>()) {
        if (!id_expr->name) {
            current_value_ = ValueRef::invalid();
            return;
        }
        
        std::string name(id_expr->name->name);
        ValueRef var_ptr = builder_.get_variable(name);
        
        if (var_ptr.is_valid()) {
            builder_.emit_store(rhs_val, var_ptr);
            current_value_ = rhs_val;  // Assignment returns the assigned value
        } else {
            errors_.add_error(SemanticError::SymbolNotFound, "Cannot assign to: " + name);
            current_value_ = ValueRef::invalid();
        }
    } else {
        errors_.add_error(SemanticError::InvalidAssignment, "Invalid assignment target");
        current_value_ = ValueRef::invalid();
    }
}

void CodeGenerator::visit(CallExpressionNode* node) {
    if (!node || !node->callee) {
        current_value_ = ValueRef::invalid();
        return;
    }
    
    // For now, only support identifier function calls
    auto* id_expr = node->callee->as<IdentifierExpressionNode>();
    if (!id_expr || !id_expr->name) {
        errors_.add_error(SemanticError::NotCallable, "Invalid function call");
        current_value_ = ValueRef::invalid();
        return;
    }
    
    std::string func_name(id_expr->name->name);
    
    // Look up function in symbol table
    Symbol* func_symbol = symbol_table_.lookup_symbol(func_name);
    if (!func_symbol || func_symbol->kind != Symbol::Function) {
        errors_.add_error(SemanticError::FunctionNotFound, "Function not found: " + func_name);
        current_value_ = ValueRef::invalid();
        return;
    }
    
    // Generate arguments
    std::vector<ValueRef> arg_values;
    if (node->arguments) {
        for (size_t i = 0; i < node->arguments->size; ++i) {
            if (node->arguments->nodes[i]) {
                node->arguments->nodes[i]->accept(this);
                if (current_value_.is_valid()) {
                    arg_values.push_back(current_value_);
                }
            }
        }
    }
    
    // Get return type
    auto* func_type = func_symbol->type->as<FunctionType>();
    IRType return_type = func_type ? convert_type(func_type->return_type) : IRType::void_type;
    
    // Emit call
    current_value_ = builder_.emit_call(func_name, arg_values, return_type);
}

void CodeGenerator::visit(MemberAccessExpressionNode* node) {
    // Not implemented yet
    errors_.add_error(SemanticError::InvalidOperation, "Member access not implemented");
    current_value_ = ValueRef::invalid();
}

void CodeGenerator::visit(ParenthesizedExpressionNode* node) {
    if (node && node->expression) {
        node->expression->accept(this);
    } else {
        current_value_ = ValueRef::invalid();
    }
}

// Statement visitors
void CodeGenerator::visit(BlockStatementNode* node) {
    if (!node) return;
    
    for (size_t i = 0; i < node->statements.size; ++i) {
        if (node->statements[i]) {
            node->statements[i]->accept(this);
        }
    }
}

void CodeGenerator::visit(ExpressionStatementNode* node) {
    if (node && node->expression) {
        node->expression->accept(this);
    }
}

void CodeGenerator::visit(IfStatementNode* node) {
    if (!node || !node->condition) return;
    
    // Generate condition
    node->condition->accept(this);
    ValueRef cond_val = current_value_;
    
    if (!cond_val.is_valid()) return;
    
    // Create labels
    std::string then_label = builder_.next_label("if_then");
    std::string else_label = builder_.next_label("if_else");
    std::string end_label = builder_.next_label("if_end");
    
    // Branch on condition
    if (node->elseStatement) {
        builder_.emit_br_cond(cond_val, then_label, else_label);
    } else {
        builder_.emit_br_cond(cond_val, then_label, end_label);
    }
    
    // Then block
    builder_.emit_label(then_label);
    if (node->thenStatement) {
        node->thenStatement->accept(this);
    }
    if (!builder_.has_terminator()) {
        builder_.emit_br(end_label);
    }
    
    // Else block (if present)  
    if (node->elseStatement) {
        builder_.emit_label(else_label);
        node->elseStatement->accept(this);
        if (!builder_.has_terminator()) {
            builder_.emit_br(end_label);
        }
    }
    
    // End block
    builder_.emit_label(end_label);
}

void CodeGenerator::visit(WhileStatementNode* node) {
    if (!node || !node->condition) return;
    
    // Create labels
    std::string loop_label = builder_.next_label("while_loop");
    std::string body_label = builder_.next_label("while_body");
    std::string end_label = builder_.next_label("while_end");
    
    // Jump to loop condition
    builder_.emit_br(loop_label);
    
    // Loop condition
    builder_.emit_label(loop_label);
    node->condition->accept(this);
    ValueRef cond_val = current_value_;
    
    if (cond_val.is_valid()) {
        builder_.emit_br_cond(cond_val, body_label, end_label);
    }
    
    // Loop body
    builder_.emit_label(body_label);
    bool old_in_loop = in_loop_;
    in_loop_ = true;
    
    if (node->body) {
        node->body->accept(this);
    }
    
    in_loop_ = old_in_loop;
    
    if (!builder_.has_terminator()) {
        builder_.emit_br(loop_label);
    }
    
    // End label
    builder_.emit_label(end_label);
}

void CodeGenerator::visit(ForStatementNode* node) {
    if (!node) return;
    
    // Create labels
    std::string loop_label = builder_.next_label("for_loop");
    std::string body_label = builder_.next_label("for_body");
    std::string inc_label = builder_.next_label("for_inc");
    std::string end_label = builder_.next_label("for_end");
    
    // Initializer
    if (node->initializer) {
        node->initializer->accept(this);
    }
    
    // Jump to condition
    builder_.emit_br(loop_label);
    
    // Loop condition
    builder_.emit_label(loop_label);
    if (node->condition) {
        node->condition->accept(this);
        ValueRef cond_val = current_value_;
        if (cond_val.is_valid()) {
            builder_.emit_br_cond(cond_val, body_label, end_label);
        }
    } else {
        // No condition = infinite loop
        builder_.emit_br(body_label);
    }
    
    // Loop body
    builder_.emit_label(body_label);
    bool old_in_loop = in_loop_;
    in_loop_ = true;
    
    if (node->body) {
        node->body->accept(this);
    }
    
    in_loop_ = old_in_loop;
    
    if (!builder_.has_terminator()) {
        builder_.emit_br(inc_label);
    }
    
    // Increment
    builder_.emit_label(inc_label);
    if (node->increment) {
        node->increment->accept(this);
    }
    builder_.emit_br(loop_label);
    
    // End label
    builder_.emit_label(end_label);
}

void CodeGenerator::visit(ReturnStatementNode* node) {
    if (node && node->expression) {
        node->expression->accept(this);
        if (current_value_.is_valid()) {
            builder_.emit_ret(current_value_);
        } else {
            builder_.emit_ret_void();
        }
    } else {
        builder_.emit_ret_void();
    }
}

void CodeGenerator::visit(BreakStatementNode* node) {
    if (!in_loop_) {
        errors_.add_error(SemanticError::BreakNotInLoop, "Break statement not in loop");
        return;
    }
    
    // For now, just emit unreachable - proper break handling needs loop context
    // This would need loop context tracking to jump to proper end labels
    errors_.add_error(SemanticError::InvalidOperation, "Break statement not fully implemented");
}

void CodeGenerator::visit(ContinueStatementNode* node) {
    if (!in_loop_) {
        errors_.add_error(SemanticError::ContinueNotInLoop, "Continue statement not in loop");
        return;
    }
    
    // For now, just emit unreachable - proper continue handling needs loop context
    // This would need loop context tracking to jump to proper increment/condition labels
    errors_.add_error(SemanticError::InvalidOperation, "Continue statement not fully implemented");
}

void CodeGenerator::visit(VariableDeclarationNode* node) {
    if (!node || !node->name) return;
    
    std::string name(node->name->name);
    
    // Look up symbol for type information
    Symbol* symbol = symbol_table_.lookup_symbol(name);
    if (!symbol) {
        errors_.add_error(SemanticError::SymbolNotFound, "Variable symbol not found: " + name);
        return;
    }
    
    // Allocate storage
    IRType var_type = convert_type(symbol->type);
    ValueRef var_ptr = builder_.emit_alloca(var_type, name);
    
    // Initialize if there's an initializer
    if (node->initializer) {
        node->initializer->accept(this);
        if (current_value_.is_valid()) {
            builder_.emit_store(current_value_, var_ptr);
        }
    }
}

// Declaration visitors
void CodeGenerator::visit(FunctionDeclarationNode* node) {
    if (!node || !node->name) return;
    
    std::string func_name(node->name->name);
    current_function_ = nullptr;
    
    // Look up function symbol
    Symbol* func_symbol = symbol_table_.lookup_symbol(func_name);
    if (func_symbol && func_symbol->kind == Symbol::Function) {
        current_function_ = func_symbol->type->as<FunctionType>();
    }
    
    emit_function_prologue(node);
    
    // Generate function body
    if (node->body) {
        node->body->accept(this);
    }
    
    emit_function_epilogue();
    current_function_ = nullptr;
}

void CodeGenerator::visit(CompilationUnitNode* node) {
    if (!node) return;
    
    for (size_t i = 0; i < node->statements.size; ++i) {
        if (node->statements[i]) {
            node->statements[i]->accept(this);
        }
    }
}

} // namespace Myre