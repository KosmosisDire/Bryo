// hlir_builder.cpp
#include "bound_to_hlir.hpp"
#include <cassert>

namespace Bryo::HLIR
{
    #pragma region Public Methods
    
    void BoundToHLIR::build(BoundCompilationUnit* unit) {
        visit(unit);
        resolve_pending_phis();
    }
    
    #pragma region Core Expressions
    
    void BoundToHLIR::visit(BoundLiteralExpression* node) {
        HLIR::Value* result = nullptr;
        
        if (std::holds_alternative<int64_t>(node->constantValue)) {
            auto val = std::get<int64_t>(node->constantValue);
            result = builder.const_int(val, node->type);
        }
        else if (std::holds_alternative<bool>(node->constantValue)) {
            auto val = std::get<bool>(node->constantValue);
            result = builder.const_bool(val, node->type);
        }
        else if (std::holds_alternative<double>(node->constantValue)) {
            auto val = std::get<double>(node->constantValue);
            result = builder.const_float(val, node->type);
        }
        else if (std::holds_alternative<std::string>(node->constantValue)) {
            auto& val = std::get<std::string>(node->constantValue);
            result = builder.const_string(val, node->type);
        }
        
        expression_values[node] = result;
    }
    
    void BoundToHLIR::visit(BoundNameExpression* node) {
        if (!node->symbol) return;
        
        auto value = get_symbol_value(node->symbol);
        expression_values[node] = value;
    }
    
    void BoundToHLIR::visit(BoundBinaryExpression* node) {
        auto left = evaluate_expression(node->left);
        auto right = evaluate_expression(node->right);
        
        auto opcode = get_binary_opcode(node->operatorKind);
        auto result = builder.binary(opcode, left, right);
        
        expression_values[node] = result;
    }
    
    void BoundToHLIR::visit(BoundUnaryExpression* node) {
        auto operand = evaluate_expression(node->operand);
        
        auto opcode = get_unary_opcode(node->operatorKind);
        auto result = builder.unary(opcode, operand);
        
        expression_values[node] = result;
    }
    
    void BoundToHLIR::visit(BoundAssignmentExpression* node) {
        auto value = evaluate_expression(node->value);
        
        // Handle simple name assignment
        if (auto name = node->target->as<BoundNameExpression>()) {
            if (name->symbol) {
                set_symbol_value(name->symbol, value);
            }
        }
        // TODO: Handle member access, index expressions
        
        expression_values[node] = value;
    }
    
    void BoundToHLIR::visit(BoundCallExpression* node) {
        std::vector<HLIR::Value*> args;
        for (auto arg : node->arguments) {
            args.push_back(evaluate_expression(arg));
        }
        
        if (node->method && node->method->as<FunctionSymbol>()) {
            auto func_sym = static_cast<FunctionSymbol*>(node->method);
            // TODO: Look up HLIR function from symbol
            HLIR::Function* func = nullptr; // TODO: module->function_map[func_sym->get_mangled_name()];
            
            if (func) {
                auto result = builder.call(func, args);
                expression_values[node] = result;
            }
        }
    }
    
    #pragma region Stub Expressions
    
    void BoundToHLIR::visit(BoundMemberAccessExpression* node) {
        // TODO: Implement field/property access
        expression_values[node] = nullptr;
    }
    
    void BoundToHLIR::visit(BoundIndexExpression* node) {
        // TODO: Implement array/indexer access
        expression_values[node] = nullptr;
    }
    
    void BoundToHLIR::visit(BoundNewExpression* node) {
        // TODO: Implement object creation
        expression_values[node] = nullptr;
    }
    
    void BoundToHLIR::visit(BoundArrayCreationExpression* node) {
        // TODO: Implement array creation
        expression_values[node] = nullptr;
    }
    
    void BoundToHLIR::visit(BoundCastExpression* node) {
        auto expr = evaluate_expression(node->expression);
        // TODO: Implement proper cast based on conversion kind
        auto result = builder.cast(expr, node->type);
        expression_values[node] = result;
    }
    
    void BoundToHLIR::visit(BoundConditionalExpression* node) {
        // TODO: Implement ternary operator with phi nodes
        expression_values[node] = nullptr;
    }
    
    void BoundToHLIR::visit(BoundThisExpression* node) {
        // TODO: Implement this pointer
        expression_values[node] = nullptr;
    }
    
    void BoundToHLIR::visit(BoundTypeOfExpression* node) {
        // TODO: Implement typeof
        expression_values[node] = nullptr;
    }
    
    void BoundToHLIR::visit(BoundSizeOfExpression* node) {
        // TODO: Implement sizeof
        expression_values[node] = nullptr;
    }
    
    void BoundToHLIR::visit(BoundParenthesizedExpression* node) {
        auto inner = evaluate_expression(node->expression);
        expression_values[node] = inner;
    }
    
    void BoundToHLIR::visit(BoundConversionExpression* node) {
        auto expr = evaluate_expression(node->expression);
        // TODO: Handle different conversion kinds
        expression_values[node] = expr;
    }
    
    void BoundToHLIR::visit(BoundTypeExpression* node) {
        // TODO: Type expressions don't produce runtime values
        expression_values[node] = nullptr;
    }
    
    #pragma region Core Statements
    
    void BoundToHLIR::visit(BoundBlockStatement* node) {
        for (auto stmt : node->statements) {
            stmt->accept(this);
        }
    }
    
    void BoundToHLIR::visit(BoundExpressionStatement* node) {
        evaluate_expression(node->expression);
    }
    
    void BoundToHLIR::visit(BoundIfStatement* node) {
        auto cond = evaluate_expression(node->condition);
        
        auto then_block = create_block("if.then");
        auto merge_block = create_block("if.merge");
        auto else_block = node->elseStatement 
            ? create_block("if.else") 
            : merge_block;
        
        builder.cond_br(cond, then_block, else_block);
        
        // Then branch
        builder.set_block(then_block);
        current_block = then_block;
        node->thenStatement->accept(this);
        if (!current_block->terminator()) {
            builder.br(merge_block);
        }
        
        // Else branch (if exists)
        if (node->elseStatement) {
            builder.set_block(else_block);
            current_block = else_block;
            node->elseStatement->accept(this);
            if (!current_block->terminator()) {
                builder.br(merge_block);
            }
        }
        
        // Continue with merge
        builder.set_block(merge_block);
        current_block = merge_block;
    }
    
    void BoundToHLIR::visit(BoundWhileStatement* node) {
        auto header = create_block("while.header");
        auto body = create_block("while.body");
        auto exit = create_block("while.exit");
        
        // Jump to header
        builder.br(header);
        builder.set_block(header);
        current_block = header;
        
        // Set up loop context
        LoopContext ctx;
        ctx.continue_target = header;
        ctx.break_target = exit;
        ctx.loop_entry_values = symbol_values;
        loop_stack.push(ctx);
        
        // Check condition
        auto cond = evaluate_expression(node->condition);
        builder.cond_br(cond, body, exit);
        
        // Body
        builder.set_block(body);
        current_block = body;
        node->body->accept(this);
        if (!current_block->terminator()) {
            builder.br(header);
        }
        
        loop_stack.pop();
        builder.set_block(exit);
        current_block = exit;
    }
    
    void BoundToHLIR::visit(BoundForStatement* node) {
        // Initialize
        if (node->initializer) {
            node->initializer->accept(this);
        }
        
        auto header = create_block("for.header");
        auto body = create_block("for.body");
        auto update = create_block("for.update");
        auto exit = create_block("for.exit");
        
        builder.br(header);
        builder.set_block(header);
        current_block = header;
        
        // Set up loop context
        LoopContext ctx;
        ctx.continue_target = update;
        ctx.break_target = exit;
        ctx.loop_entry_values = symbol_values;
        loop_stack.push(ctx);
        
        // Condition
        if (node->condition) {
            auto cond = evaluate_expression(node->condition);
            builder.cond_br(cond, body, exit);
        } else {
            builder.br(body);
        }
        
        // Body
        builder.set_block(body);
        current_block = body;
        node->body->accept(this);
        if (!current_block->terminator()) {
            builder.br(update);
        }
        
        // Update
        builder.set_block(update);
        current_block = update;
        for (auto inc : node->incrementors) {
            evaluate_expression(inc);
        }
        builder.br(header);
        
        loop_stack.pop();
        builder.set_block(exit);
        current_block = exit;
    }
    
    void BoundToHLIR::visit(BoundBreakStatement* node) {
        if (!loop_stack.empty()) {
            builder.br(loop_stack.top().break_target);
        }
    }
    
    void BoundToHLIR::visit(BoundContinueStatement* node) {
        if (!loop_stack.empty()) {
            builder.br(loop_stack.top().continue_target);
        }
    }
    
    void BoundToHLIR::visit(BoundReturnStatement* node) {
        if (node->value) {
            auto val = evaluate_expression(node->value);
            builder.ret(val);
        } else {
            builder.ret(nullptr);
        }
    }
    
    void BoundToHLIR::visit(BoundUsingStatement* node) {
        // TODO: Using statements don't generate code
    }
    
    #pragma region Declarations
    
    void BoundToHLIR::visit(BoundVariableDeclaration* node) {
        HLIR::Value* init_value = nullptr;
        
        if (node->initializer) {
            init_value = evaluate_expression(node->initializer);
        } else {
            // Default initialize
            init_value = builder.const_null(node->symbol->as<VariableSymbol>()->type);
        }
        
        if (node->symbol) {
            set_symbol_value(node->symbol, init_value);
        }
    }
    
    void BoundToHLIR::visit(BoundFunctionDeclaration* node) {
        auto func_sym = node->symbol->as<FunctionSymbol>();
        if (!func_sym) return;
        
        auto func = module->create_function(func_sym);
        
        current_function = func;
        auto entry = func->create_block("entry");
        func->entry = entry;
        current_block = entry;  // Set current_block
        builder.set_function(func);
        builder.set_block(entry);
        
        // Create parameter values
        for (size_t i = 0; i < node->parameters.size(); i++) {
            auto param = func->create_value(
                node->parameters[i]->symbol->as<ParameterSymbol>()->type,
                node->parameters[i]->name
            );
            func->params.push_back(param);
            set_symbol_value(node->parameters[i]->symbol, param);
        }
        
        // Process body
        if (node->body) {
            node->body->accept(this);
        }
        
        // Add implicit return if needed
        if (current_block && !current_block->terminator()) {
            builder.ret(nullptr);
        }
        
        current_function = nullptr;
        current_block = nullptr;  // Clear current_block
    }
    
    void BoundToHLIR::visit(BoundPropertyDeclaration* node) {
        // TODO: Generate getter/setter functions
    }
    
    void BoundToHLIR::visit(BoundTypeDeclaration* node)
    {
        auto type_sym = node->symbol->as<TypeSymbol>();
        if (!type_sym) return;
        
        auto type_def = module->define_type(type_sym);
    }
    
    void BoundToHLIR::visit(BoundNamespaceDeclaration* node) {
        // Process namespace members
        for (auto member : node->members) {
            member->accept(this);
        }
    }
    
    void BoundToHLIR::visit(BoundCompilationUnit* node) {
        for (auto stmt : node->statements) {
            stmt->accept(this);
        }
    }
    
    #pragma region Helper Methods
    
    HLIR::Value* BoundToHLIR::evaluate_expression(BoundExpression* expr) {
        expr->accept(this);
        return expression_values[expr];
    }
    
    HLIR::Value* BoundToHLIR::get_symbol_value(Symbol* sym) {
        auto it = symbol_values.find(sym);
        if (it != symbol_values.end()) {
            return it->second;
        }
        // TODO: Handle undefined variable
        return nullptr;
    }
    
    void BoundToHLIR::set_symbol_value(Symbol* sym, HLIR::Value* val) {
        symbol_values[sym] = val;
    }
    
    HLIR::BasicBlock* BoundToHLIR::create_block(const std::string& name) {
        auto block = current_function->create_block(name);
        current_block = block;
        return block;
    }
    
    void BoundToHLIR::resolve_pending_phis() {
        // TODO: Complete phi nodes with proper values
        for (auto& pending : pending_phis) {
            // Find value for symbol at each predecessor
        }
        pending_phis.clear();
    }
    
    HLIR::Opcode BoundToHLIR::get_binary_opcode(BinaryOperatorKind kind) {
        switch (kind) {
            case BinaryOperatorKind::Add: return HLIR::Opcode::Add;
            case BinaryOperatorKind::Subtract: return HLIR::Opcode::Sub;
            case BinaryOperatorKind::Multiply: return HLIR::Opcode::Mul;
            case BinaryOperatorKind::Divide: return HLIR::Opcode::Div;
            case BinaryOperatorKind::Modulo: return HLIR::Opcode::Rem;
            case BinaryOperatorKind::Equals: return HLIR::Opcode::Eq;
            case BinaryOperatorKind::NotEquals: return HLIR::Opcode::Ne;
            case BinaryOperatorKind::LessThan: return HLIR::Opcode::Lt;
            case BinaryOperatorKind::LessThanOrEqual: return HLIR::Opcode::Le;
            case BinaryOperatorKind::GreaterThan: return HLIR::Opcode::Gt;
            case BinaryOperatorKind::GreaterThanOrEqual: return HLIR::Opcode::Ge;
            case BinaryOperatorKind::LogicalAnd: return HLIR::Opcode::And;
            case BinaryOperatorKind::LogicalOr: return HLIR::Opcode::Or;
            case BinaryOperatorKind::BitwiseAnd: return HLIR::Opcode::BitAnd;
            case BinaryOperatorKind::BitwiseOr: return HLIR::Opcode::BitOr;
            case BinaryOperatorKind::BitwiseXor: return HLIR::Opcode::BitXor;
            case BinaryOperatorKind::LeftShift: return HLIR::Opcode::Shl;
            case BinaryOperatorKind::RightShift: return HLIR::Opcode::Shr;
            default: return HLIR::Opcode::Add; // TODO: Error handling
        }
    }
    
    HLIR::Opcode BoundToHLIR::get_unary_opcode(UnaryOperatorKind kind) {
        switch (kind) {
            case UnaryOperatorKind::Minus: return HLIR::Opcode::Neg;
            case UnaryOperatorKind::Not: return HLIR::Opcode::Not;
            case UnaryOperatorKind::BitwiseNot: return HLIR::Opcode::BitNot;
            default: return HLIR::Opcode::Neg; // TODO: Handle inc/dec and others
        }
    }
}