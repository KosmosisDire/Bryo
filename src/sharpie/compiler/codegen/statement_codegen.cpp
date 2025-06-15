#include "sharpie/compiler/codegen/codegen.hpp"

namespace Mycelium::Scripting::Lang::CodeGen {

llvm::Value* CodeGenerator::cg_statement(std::shared_ptr<StatementNode> node) {
    if (auto block_stmt = std::dynamic_pointer_cast<BlockStatementNode>(node)) {
        return cg_block_statement(block_stmt);
    }
    if (auto var_decl_stmt = std::dynamic_pointer_cast<LocalVariableDeclarationStatementNode>(node)) {
        return cg_local_variable_declaration_statement(var_decl_stmt);
    }
    if (auto expr_stmt = std::dynamic_pointer_cast<ExpressionStatementNode>(node)) {
        return cg_expression_statement(expr_stmt);
    }
    if (auto if_stmt = std::dynamic_pointer_cast<IfStatementNode>(node)) {
        return cg_if_statement(if_stmt);
    }
    if (auto while_stmt = std::dynamic_pointer_cast<WhileStatementNode>(node)) {
        return cg_while_statement(while_stmt);
    }
    if (auto for_stmt = std::dynamic_pointer_cast<ForStatementNode>(node)) {
        return cg_for_statement(for_stmt);
    }
    if (auto return_stmt = std::dynamic_pointer_cast<ReturnStatementNode>(node)) {
        return cg_return_statement(return_stmt);
    }
    if (auto break_stmt = std::dynamic_pointer_cast<BreakStatementNode>(node)) {
        return cg_break_statement(break_stmt);
    }
    if (auto continue_stmt = std::dynamic_pointer_cast<ContinueStatementNode>(node)) {
        return cg_continue_statement(continue_stmt);
    }
    log_compiler_error("Unsupported statement type in code generation.", node ? node->location : std::nullopt);
    return nullptr;
}

llvm::Value* CodeGenerator::cg_block_statement(std::shared_ptr<BlockStatementNode> node) {
    // Push block scope for proper object lifecycle management
    ctx.scope_manager.push_scope(ScopeType::Block, "block");

    llvm::Value* last_val = nullptr;
    for (const auto& stmt : node->statements) {
        if (ctx.builder.GetInsertBlock()->getTerminator()) {
            break; // Stop generating code if the block has already terminated
        }
        last_val = cg_statement(stmt);
    }

    // Pop block scope - this will automatically clean up any objects created in this scope
    ctx.scope_manager.pop_scope();
    return last_val;
}

llvm::Value* CodeGenerator::cg_local_variable_declaration_statement(std::shared_ptr<LocalVariableDeclarationStatementNode> node) {
    llvm::Type* var_llvm_type = get_llvm_type(ctx, node->type);
    const SymbolTable::ClassSymbol* var_static_class_info = nullptr;
    
    if (var_llvm_type->isPointerTy()) {
        if (auto identNode_variant = std::get_if<std::shared_ptr<IdentifierNode>>(&node->type->name_segment)) {
            if (auto identNode = *identNode_variant) {
                const auto* class_symbol = ctx.symbol_table.find_class(identNode->name);
                if (class_symbol) {
                    var_static_class_info = class_symbol;
                }
            }
        }
    }

    for (const auto& declarator : node->declarators) {
        VariableInfo varInfo;
        varInfo.alloca = create_entry_block_alloca(ctx, declarator->name->name, var_llvm_type);
        varInfo.classInfo = var_static_class_info;
        varInfo.declaredTypeNode = node->type;
        ctx.named_values[declarator->name->name] = varInfo;

        if (declarator->initializer) {
            ExpressionCGResult init_res = cg_expression(declarator->initializer.value());
            const SymbolTable::ClassSymbol* init_val_class_info = init_res.classInfo;
            if (!init_res.value) {
                log_compiler_error("Initializer for '" + declarator->name->name + "' compiled to null.", declarator->initializer.value()->location);
            }

            // Type compatibility already validated by semantic analyzer
            // No need for inheritance hierarchy checking here

            // CRITICAL ARC FIX: Add proper retain logic for variable initialization
            // This ensures that `TestObject copy = original;` properly retains the source object
            // But skip retain for new expressions as they already have correct ref_count
            if (var_static_class_info && var_static_class_info->fieldsType && init_res.value->getType()->isPointerTy()) {
                // Check if the initializer is a new expression (ObjectCreationExpression)
                bool is_new_expression = std::dynamic_pointer_cast<ObjectCreationExpressionNode>(declarator->initializer.value()) != nullptr;

                if (!is_new_expression) {
                    // Only retain if this is NOT a new expression
                    llvm::Value* init_object_header = nullptr;
                    if (init_res.header_ptr) {
                        init_object_header = init_res.header_ptr;
                    } else {
                        init_object_header = get_header_ptr_from_fields_ptr(ctx, init_res.value, var_static_class_info->fieldsType);
                    }
                    if (init_object_header) {
                        create_arc_retain(ctx, init_object_header);
                    }
                }
            }
            
            ctx.builder.CreateStore(init_res.value, varInfo.alloca);

            // Check if this is a declared type name to exclude built-in types like 'string'
            std::string declared_type_name;
            if (auto identNode_variant = std::get_if<std::shared_ptr<IdentifierNode>>(&node->type->name_segment)) {
                if (auto identNode = *identNode_variant) {
                    declared_type_name = identNode->name;
                }
            }

            // UNIFIED ARC MANAGEMENT: Use only scope manager, remove dual systems
            // Register ARC objects with scope manager for consistent cleanup
            if (var_static_class_info && var_static_class_info->fieldsType &&
                init_res.value->getType()->isPointerTy() && declared_type_name != "string") {

                // Register with scope manager for unified ARC management
                ctx.scope_manager.register_arc_managed_object(
                    varInfo.alloca,
                    var_static_class_info,
                    declarator->name->name);
            }
        }
    }
    return nullptr;
}

llvm::Value* CodeGenerator::cg_expression_statement(std::shared_ptr<ExpressionStatementNode> node) {
    if (!node->expression) {
        log_compiler_error("ExpressionStatementNode has no expression.", node->location);
    }
    return cg_expression(node->expression).value;
}

llvm::Value* CodeGenerator::cg_if_statement(std::shared_ptr<IfStatementNode> node) {
    ExpressionCGResult cond_res = cg_expression(node->condition);
    if (!cond_res.value) {
        log_compiler_error("If statement condition is null.", node->condition->location);
    }
    
    llvm::Value* cond_val = cond_res.value;
    if (!cond_val->getType()->isIntegerTy(1)) {
        cond_val = ctx.builder.CreateICmpNE(cond_val, llvm::ConstantInt::get(cond_val->getType(), 0), "tobool");
    }

    llvm::Function* TheFunction = ctx.builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* ThenBB = llvm::BasicBlock::Create(ctx.llvm_context, "then", TheFunction);
    llvm::BasicBlock* ElseBB = llvm::BasicBlock::Create(ctx.llvm_context, "else");

    ctx.builder.CreateCondBr(cond_val, ThenBB, ElseBB);

    // Compile then branch
    ctx.builder.SetInsertPoint(ThenBB);
    cg_statement(node->thenStatement);
    bool then_has_terminator = ctx.builder.GetInsertBlock()->getTerminator() != nullptr;
    ThenBB = ctx.builder.GetInsertBlock();

    // Compile else branch
    TheFunction->insert(TheFunction->end(), ElseBB);
    ctx.builder.SetInsertPoint(ElseBB);
    if (node->elseStatement.has_value()) {
        cg_statement(node->elseStatement.value());
    }
    bool else_has_terminator = ctx.builder.GetInsertBlock()->getTerminator() != nullptr;
    ElseBB = ctx.builder.GetInsertBlock();

    // Only create and use merge block if at least one branch doesn't have a terminator
    if (!then_has_terminator || !else_has_terminator) {
        llvm::BasicBlock* MergeBB = llvm::BasicBlock::Create(ctx.llvm_context, "ifcont");

        // Add branches to merge block from branches that don't have terminators
        if (!then_has_terminator) {
            ctx.builder.SetInsertPoint(ThenBB);
            ctx.builder.CreateBr(MergeBB);
        }
        if (!else_has_terminator) {
            ctx.builder.SetInsertPoint(ElseBB);
            ctx.builder.CreateBr(MergeBB);
        }

        // Insert merge block and set as current insert point
        TheFunction->insert(TheFunction->end(), MergeBB);
        ctx.builder.SetInsertPoint(MergeBB);
    }
    // If both branches have terminators, don't create a merge block at all
    // The insert point will be invalid, but that's okay since control flow has ended

    return nullptr;
}

llvm::Value* CodeGenerator::cg_while_statement(std::shared_ptr<WhileStatementNode> node) {
    ExpressionCGResult cond_res = cg_expression(node->condition);
    if (!cond_res.value) {
        log_compiler_error("While statement condition is null.", node->condition->location);
    }

    llvm::Function* function = ctx.builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* condBB = llvm::BasicBlock::Create(ctx.llvm_context, "while.cond", function);
    llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(ctx.llvm_context, "while.body");
    llvm::BasicBlock* exitBB = llvm::BasicBlock::Create(ctx.llvm_context, "while.exit");

    // Jump to condition check
    ctx.builder.CreateBr(condBB);

    // Condition block
    ctx.builder.SetInsertPoint(condBB);
    ExpressionCGResult loop_cond_res = cg_expression(node->condition);
    llvm::Value* cond_val = loop_cond_res.value;
    if (!cond_val->getType()->isIntegerTy(1)) {
        cond_val = ctx.builder.CreateICmpNE(cond_val, llvm::ConstantInt::get(cond_val->getType(), 0), "tobool");
    }
    ctx.builder.CreateCondBr(cond_val, bodyBB, exitBB);

    // Body block
    function->insert(function->end(), bodyBB);
    ctx.builder.SetInsertPoint(bodyBB);

    // Push loop context for break/continue
    ctx.loop_context_stack.push_back({exitBB, condBB});

    cg_statement(node->body);

    // Pop loop context
    ctx.loop_context_stack.pop_back();

    if (!ctx.builder.GetInsertBlock()->getTerminator()) {
        ctx.builder.CreateBr(condBB); // Loop back to condition
    }

    // Exit block
    function->insert(function->end(), exitBB);
    ctx.builder.SetInsertPoint(exitBB);

    return nullptr;
}

llvm::Value* CodeGenerator::cg_for_statement(std::shared_ptr<ForStatementNode> node) {
    llvm::Function* function = ctx.builder.GetInsertBlock()->getParent();

    // Handle initializer
    if (auto var_decl_variant = std::get_if<std::shared_ptr<LocalVariableDeclarationStatementNode>>(&node->initializers)) {
        cg_local_variable_declaration_statement(*var_decl_variant);
    } else if (auto expr_list_variant = std::get_if<std::vector<std::shared_ptr<ExpressionNode>>>(&node->initializers)) {
        for (auto& init_expr : *expr_list_variant) {
            cg_expression(init_expr);
        }
    }

    // Create basic blocks
    llvm::BasicBlock* condBB = llvm::BasicBlock::Create(ctx.llvm_context, "for.cond", function);
    llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(ctx.llvm_context, "for.body");
    llvm::BasicBlock* incBB = llvm::BasicBlock::Create(ctx.llvm_context, "for.inc");
    llvm::BasicBlock* exitBB = llvm::BasicBlock::Create(ctx.llvm_context, "for.exit");

    // Jump to condition
    ctx.builder.CreateBr(condBB);

    // Condition block
    ctx.builder.SetInsertPoint(condBB);
    if (node->condition) {
        ExpressionCGResult cond_res = cg_expression(node->condition.value());
        llvm::Value* cond_val = cond_res.value;
        if (!cond_val->getType()->isIntegerTy(1)) {
            cond_val = ctx.builder.CreateICmpNE(cond_val, llvm::ConstantInt::get(cond_val->getType(), 0), "tobool");
        }
        ctx.builder.CreateCondBr(cond_val, bodyBB, exitBB);
    } else {
        // No condition means infinite loop (unless broken)
        ctx.builder.CreateBr(bodyBB);
    }

    // Body block
    function->insert(function->end(), bodyBB);
    ctx.builder.SetInsertPoint(bodyBB);

    // Push loop context for break/continue
    ctx.loop_context_stack.push_back({exitBB, incBB});

    cg_statement(node->body);

    // Pop loop context
    ctx.loop_context_stack.pop_back();

    if (!ctx.builder.GetInsertBlock()->getTerminator()) {
        ctx.builder.CreateBr(incBB);
    }

    // Increment block
    function->insert(function->end(), incBB);
    ctx.builder.SetInsertPoint(incBB);
    for (auto& inc_expr : node->incrementors) {
        cg_expression(inc_expr);
    }
    ctx.builder.CreateBr(condBB); // Loop back to condition

    // Exit block
    function->insert(function->end(), exitBB);
    ctx.builder.SetInsertPoint(exitBB);

    return nullptr;
}

llvm::Value* CodeGenerator::cg_return_statement(std::shared_ptr<ReturnStatementNode> node) {
    // Generate the return value first
    llvm::Value* return_value = nullptr;
    if (node->expression) {
        ExpressionCGResult ret_res = cg_expression(node->expression.value());
        if (!ret_res.value) {
            log_compiler_error("Return expression compiled to null.", node->expression.value()->location);
        }
        // Return type compatibility already validated by semantic analyzer
        return_value = ret_res.value;
    } else {
        if (!ctx.current_function->getReturnType()->isVoidTy()) {
            log_compiler_error("Non-void function missing return value.", node->location);
        }
    }

    // Clean up function scope before return (handles all cleanup via scope manager)
    ctx.scope_manager.pop_scope();

    // Generate the return instruction
    if (return_value) {
        ctx.builder.CreateRet(return_value);
    } else {
        ctx.builder.CreateRetVoid();
    }

    return nullptr;
}

llvm::Value* CodeGenerator::cg_break_statement(std::shared_ptr<BreakStatementNode> node) {
    if (ctx.loop_context_stack.empty()) {
        log_compiler_error("'break' statement used outside of loop.", node->location);
    }

    const LoopContext& current_loop = ctx.loop_context_stack.back();
    ctx.builder.CreateBr(current_loop.exit_block);
    return nullptr;
}

llvm::Value* CodeGenerator::cg_continue_statement(std::shared_ptr<ContinueStatementNode> node) {
    if (ctx.loop_context_stack.empty()) {
        log_compiler_error("'continue' statement used outside of loop.", node->location);
    }

    // CRITICAL: Clean up scope BEFORE creating the terminator instruction
    // This ensures any object destructors are called before the continue jump
    ctx.scope_manager.cleanup_current_scope_early();

    const LoopContext& current_loop = ctx.loop_context_stack.back();
    ctx.builder.CreateBr(current_loop.continue_block);
    return nullptr;
}

} // namespace Mycelium::Scripting::Lang::CodeGen