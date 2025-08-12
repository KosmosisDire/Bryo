// codegen.cpp - LLVM Code Generator with Pre-declaration Support
#include "codegen/codegen.hpp"
#include <llvm/IR/Constants.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/Support/Casting.h>

namespace Myre
{

    // === Main Entry Points ===

    std::unique_ptr<llvm::Module> CodeGenerator::generate(CompilationUnitNode *unit)
    {
        // Clear any previous state
        locals.clear();
        local_types.clear();
        type_cache.clear();
        declared_functions.clear();
        errors.clear();
        while (!value_stack.empty())
            value_stack.pop();

        // Step 1: Declare all functions from symbol table
        declare_all_functions();

        // Step 2: Generate function bodies and other declarations
        visit(unit);

        // Verify the module
        std::string verify_error;
        llvm::raw_string_ostream error_stream(verify_error);
        if (llvm::verifyModule(*module, &error_stream))
        {
            errors.push_back("Module verification failed: " + verify_error);
        }

        return std::move(module);
    }

    void CodeGenerator::declare_all_functions()
    {
        // Start from global namespace and recursively declare all functions
        auto *global_scope = symbol_table.get_global_namespace();
        if (global_scope)
        {
            declare_all_functions_in_scope(global_scope);
        }
    }

    void CodeGenerator::declare_all_functions_in_scope(Scope *scope)
    {
        if (!scope)
            return;

        // Declare functions in this scope
        for (const auto &[name, symbol] : scope->symbols)
        {
            if (auto *func_sym = symbol->as<FunctionSymbol>())
            {
                declare_function_from_symbol(func_sym);
            }

            // Recursively process nested scopes (namespaces, types, etc.)
            if (auto *nested_scope = symbol->as<Scope>())
            {
                declare_all_functions_in_scope(nested_scope);
            }
        }
    }

    llvm::Function *CodeGenerator::declare_function_from_symbol(FunctionSymbol *func_symbol)
    {
        if (!func_symbol)
            return nullptr;

        const std::string &func_name = func_symbol->get_qualified_name();

        // Check if already declared
        if (declared_functions.count(func_name) > 0)
        {
            return module->getFunction(func_name);
        }

        // Build parameter types
        std::vector<llvm::Type *> param_types;
        for (const auto &param_type : func_symbol->parameter_types())
        {
            param_types.push_back(get_llvm_type(param_type));
        }

        // Get return type
        llvm::Type *return_type = get_llvm_type(func_symbol->return_type());

        // Create function type
        auto *func_type = llvm::FunctionType::get(return_type, param_types, false);

        // Create function declaration (no body)
        auto *function = llvm::Function::Create(
            func_type,
            llvm::Function::ExternalLinkage,
            func_name,
            module.get());

        declared_functions.insert(func_name);
        return function;
    }

    void CodeGenerator::generate_definitions(CompilationUnitNode *unit)
    {
        // Only generate function bodies, skip declarations
        // This is used for multi-file compilation
        if (!unit)
            return;

        for (auto *stmt : unit->statements)
        {
            if (stmt)
            {
                // Could be declarations, statements, or error nodes
                if (auto *decl = stmt->as<DeclarationNode>())
                {
                    decl->accept(this);
                }
                else if (auto *statement = stmt->as<StatementNode>())
                {
                    statement->accept(this);
                }
                else if (stmt->as<ErrorNode>())
                {
                    errors.push_back("Error node in compilation unit");
                }
                else
                {
                    stmt->accept(this);
                }
            }
        }
    }

    // === Helper Methods ===

    void CodeGenerator::push_value(llvm::Value *val)
    {
        if (val)
        {
            value_stack.push(val);
        }
        else
        {
            errors.push_back("Attempted to push null value to stack");
        }
    }

    llvm::Value *CodeGenerator::pop_value()
    {
        if (value_stack.empty())
        {
            errors.push_back("Attempted to pop from empty value stack");
            return nullptr;
        }
        auto *val = value_stack.top();
        value_stack.pop();
        return val;
    }

    llvm::Type *CodeGenerator::get_llvm_type(TypePtr type)
    {
        if (!type)
        {
            errors.push_back("Null type encountered");
            return llvm::Type::getVoidTy(*context);
        }

        // Check cache first
        auto it = type_cache.find(type);
        if (it != type_cache.end())
        {
            return it->second;
        }

        llvm::Type *llvm_type = nullptr;

        if (auto *prim = std::get_if<PrimitiveType>(&type->value))
        {
            switch (prim->kind)
            {
            case PrimitiveType::I32:
            case PrimitiveType::U32:
                llvm_type = llvm::Type::getInt32Ty(*context);
                break;
            case PrimitiveType::I64:
            case PrimitiveType::U64:
                llvm_type = llvm::Type::getInt64Ty(*context);
                break;
            case PrimitiveType::F32:
                llvm_type = llvm::Type::getFloatTy(*context);
                break;
            case PrimitiveType::F64:
                llvm_type = llvm::Type::getDoubleTy(*context);
                break;
            case PrimitiveType::Bool:
                llvm_type = llvm::Type::getInt1Ty(*context);
                break;
            case PrimitiveType::Void:
                llvm_type = llvm::Type::getVoidTy(*context);
                break;
            default:
                errors.push_back("Unsupported primitive type");
                llvm_type = llvm::Type::getVoidTy(*context);
                break;
            }
        }
        else if (std::get_if<UnresolvedType>(&type->value))
        {
            errors.push_back("Unresolved type encountered during codegen");
            llvm_type = llvm::Type::getVoidTy(*context);
        }
        else
        {
            errors.push_back("Unsupported type kind");
            llvm_type = llvm::Type::getVoidTy(*context);
        }

        type_cache[type] = llvm_type;
        return llvm_type;
    }

    llvm::Value *CodeGenerator::create_constant(LiteralExpressionNode *literal)
    {
        if (!literal || !literal->token)
        {
            errors.push_back("Invalid literal node");
            return nullptr;
        }

        std::string text(literal->token->text);

        switch (literal->kind)
        {
        case LiteralKind::I32:
        {
            int64_t val = std::stoll(text);
            return llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), val);
        }
        case LiteralKind::I64:
        {
            int64_t val = std::stoll(text);
            return llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context), val);
        }
        case LiteralKind::F32:
        {
            double val = std::stod(text);
            return llvm::ConstantFP::get(llvm::Type::getFloatTy(*context), val);
        }
        case LiteralKind::F64:
        {
            double val = std::stod(text);
            return llvm::ConstantFP::get(llvm::Type::getDoubleTy(*context), val);
        }
        case LiteralKind::Bool:
        {
            bool val = (text == "true");
            return llvm::ConstantInt::get(llvm::Type::getInt1Ty(*context), val ? 1 : 0);
        }
        default:
            errors.push_back("Unsupported literal type");
            return nullptr;
        }
    }

    void CodeGenerator::ensure_terminator()
    {
        auto *bb = builder->GetInsertBlock();
        if (bb && !bb->getTerminator())
        {
            // Add a default return based on function type
            if (current_function)
            {
                auto *ret_type = current_function->getReturnType();
                if (ret_type->isVoidTy())
                {
                    builder->CreateRetVoid();
                }
                else if (ret_type->isIntegerTy(32))
                {
                    builder->CreateRet(llvm::ConstantInt::get(ret_type, 0));
                }
                else if (ret_type->isIntegerTy(64))
                {
                    builder->CreateRet(llvm::ConstantInt::get(ret_type, 0));
                }
                else if (ret_type->isFloatTy())
                {
                    builder->CreateRet(llvm::ConstantFP::get(ret_type, 0.0));
                }
                else if (ret_type->isDoubleTy())
                {
                    builder->CreateRet(llvm::ConstantFP::get(ret_type, 0.0));
                }
                else if (ret_type->isIntegerTy(1))
                {
                    builder->CreateRet(llvm::ConstantInt::get(ret_type, 0));
                }
            }
        }
    }

    // === Visitor Implementations ===

    void CodeGenerator::visit(CompilationUnitNode *node)
    {
        if (!node)
            return;

        // Visit all top-level declarations and statements
        for (auto *stmt : node->statements)
        {
            if (stmt)
            {
                // Could be declarations, statements, or error nodes
                if (auto *decl = stmt->as<DeclarationNode>())
                {
                    decl->accept(this);
                }
                else if (auto *statement = stmt->as<StatementNode>())
                {
                    statement->accept(this);
                }
                else if (stmt->as<ErrorNode>())
                {
                    errors.push_back("Error node in compilation unit");
                }
                else
                {
                    // Fallback - try to visit it anyway
                    stmt->accept(this);
                }
            }
        }
    }

    void CodeGenerator::visit(NamespaceDeclarationNode *node)
    {
        // skip this since current scopes are held inside each AST node
    }

    void CodeGenerator::visit(FunctionDeclarationNode *node)
    {
        if (!node || !node->name)
            return;

        std::string func_name = symbol_table.lookup_handle(node->containingScope)->build_qualified_name(std::string(node->name->name));

        // Get the already-declared function
        auto *function = module->getFunction(func_name);
        if (!function)
        {
            errors.push_back("Function not declared: " + func_name);
            return;
        }

        // Skip if already has a body
        if (!function->empty())
        {
            return;
        }

        // Only generate body for non-abstract functions
        if (!node->body)
        {
            return;
        }

        // Look up function symbol for parameter information
        auto *symbol = symbol_table.lookup_handle(node->containingScope)->as<Scope>()->lookup(func_name);
        if (!symbol)
        {
            errors.push_back("Function symbol not found: " + func_name);
            return;
        }

        auto *func_symbol = symbol->as<FunctionSymbol>();
        if (!func_symbol)
        {
            errors.push_back("Symbol is not a function: " + func_name);
            return;
        }

        // Set up for body generation
        current_function = function;
        locals.clear();
        local_types.clear();

        // Create entry block
        auto *entry = llvm::BasicBlock::Create(*context, "entry", function);
        builder->SetInsertPoint(entry);

        // Map parameters to allocas
        size_t idx = 0;
        for (auto &arg : function->args())
        {
            if (idx < node->parameters.size)
            {
                // Parameters can be ParameterNode or ErrorNode
                auto *param_node = node->parameters[idx]->as<ParameterNode>();
                if (param_node && param_node->name)
                {
                    // Look up parameter symbol in function scope
                    auto *param_sym = func_symbol->lookup_local(param_node->name->name);
                    if (param_sym)
                    {
                        // Create alloca for parameter
                        auto *alloca = builder->CreateAlloca(
                            arg.getType(), nullptr, param_node->name->name);
                        builder->CreateStore(&arg, alloca);
                        locals[param_sym] = alloca;
                        local_types[param_sym] = arg.getType();

                        // Set argument name for readability
                        arg.setName(param_node->name->name);
                    }
                }
                else if (node->parameters[idx]->as<ErrorNode>())
                {
                    errors.push_back("Error node in function parameters");
                }
            }
            idx++;
        }

        // Visit function body
        visit(node->body);

        // Ensure we have a terminator
        ensure_terminator();

        current_function = nullptr;
    }

    void CodeGenerator::visit(VariableDeclarationNode *node)
    {
        if (!node || !node->first_name())
            return;

        // Look up variable symbol
        std::string var_name(node->first_name()->name);
        auto *var_symbol = symbol_table.lookup_handle(node->containingScope)->as<Scope>()->lookup_local(var_name);

        if (!var_symbol)
        {
            errors.push_back("Variable symbol not found: " + var_name);
            return;
        }

        auto *typed_symbol = dynamic_cast<TypedSymbol *>(var_symbol);
        if (!typed_symbol)
        {
            errors.push_back("Variable has no type: " + var_name);
            return;
        }

        // Get LLVM type
        auto *llvm_type = get_llvm_type(typed_symbol->type());

        // Create alloca
        auto *alloca = builder->CreateAlloca(llvm_type, nullptr, var_name);
        locals[var_symbol] = alloca;
        local_types[var_symbol] = llvm_type;

        // Handle initializer if present
        if (node->initializer)
        {
            node->initializer->accept(this);
            if (!value_stack.empty())
            {
                auto *init_value = pop_value();
                if (init_value)
                {
                    builder->CreateStore(init_value, alloca);
                }
            }
        }
    }

    void CodeGenerator::visit(ParameterNode *node)
    {
        // Parameters are handled in FunctionDeclarationNode
    }

    void CodeGenerator::visit(BlockStatementNode *node)
    {
        if (!node)
            return;

        // Visit all statements in the block
        // A block can contain both statements and local declarations
        for (auto *stmt : node->statements)
        {
            if (stmt)
            {
                // Could be statements, declarations, or error nodes
                if (auto *decl = stmt->as<DeclarationNode>())
                {
                    decl->accept(this);
                }
                else if (auto *statement = stmt->as<StatementNode>())
                {
                    statement->accept(this);
                }
                else if (stmt->as<ErrorNode>())
                {
                    errors.push_back("Error node in block statement");
                }
                else
                {
                    // Fallback - try to visit it anyway
                    stmt->accept(this);
                }
            }
        }
    }

    void CodeGenerator::visit(ExpressionStatementNode *node)
    {
        if (!node || !node->expression)
            return;

        // Evaluate expression (result will be discarded)
        if (auto *expr = node->expression->as<ExpressionNode>())
        {
            expr->accept(this);
            // Pop and discard the result if any
            if (!value_stack.empty())
            {
                pop_value();
            }
        }
        else if (node->expression->as<ErrorNode>())
        {
            errors.push_back("Error node in expression statement");
        }
    }

    void CodeGenerator::visit(IfStatementNode *node)
    {
        if (!node || !node->condition)
            return;

        // Evaluate condition
        node->condition->accept(this);
        auto *cond_value = pop_value();
        if (!cond_value)
            return;

        // Ensure condition is boolean
        if (!cond_value->getType()->isIntegerTy(1))
        {
            // Convert to boolean if needed
            if (cond_value->getType()->isIntegerTy())
            {
                cond_value = builder->CreateICmpNE(
                    cond_value,
                    llvm::ConstantInt::get(cond_value->getType(), 0),
                    "tobool");
            }
            else
            {
                errors.push_back("If condition must be boolean");
                return;
            }
        }

        // Create blocks
        auto *then_bb = llvm::BasicBlock::Create(*context, "then", current_function);
        auto *else_bb = node->elseStatement ? llvm::BasicBlock::Create(*context, "else", current_function) : nullptr;
        auto *merge_bb = llvm::BasicBlock::Create(*context, "ifcont", current_function);

        // Create conditional branch
        if (else_bb)
        {
            builder->CreateCondBr(cond_value, then_bb, else_bb);
        }
        else
        {
            builder->CreateCondBr(cond_value, then_bb, merge_bb);
        }

        // Generate then block
        builder->SetInsertPoint(then_bb);
        if (node->thenStatement)
        {
            node->thenStatement->accept(this);
        }
        // Add branch to merge if no terminator
        if (!builder->GetInsertBlock()->getTerminator())
        {
            builder->CreateBr(merge_bb);
        }

        // Generate else block if present
        if (else_bb)
        {
            builder->SetInsertPoint(else_bb);
            if (node->elseStatement)
            {
                node->elseStatement->accept(this);
            }
            // Add branch to merge if no terminator
            if (!builder->GetInsertBlock()->getTerminator())
            {
                builder->CreateBr(merge_bb);
            }
        }

        // Continue with merge block
        builder->SetInsertPoint(merge_bb);
    }

    void CodeGenerator::visit(WhileStatementNode *node)
    {
        if (!node || !node->condition)
            return;

        // Create blocks
        auto *cond_bb = llvm::BasicBlock::Create(*context, "whilecond", current_function);
        auto *body_bb = llvm::BasicBlock::Create(*context, "whilebody", current_function);
        auto *exit_bb = llvm::BasicBlock::Create(*context, "whileexit", current_function);

        // Branch to condition block
        builder->CreateBr(cond_bb);

        // Generate condition block
        builder->SetInsertPoint(cond_bb);
        node->condition->accept(this);
        auto *cond_value = pop_value();
        if (!cond_value)
            return;

        // Ensure boolean condition
        if (!cond_value->getType()->isIntegerTy(1))
        {
            if (cond_value->getType()->isIntegerTy())
            {
                cond_value = builder->CreateICmpNE(
                    cond_value,
                    llvm::ConstantInt::get(cond_value->getType(), 0),
                    "tobool");
            }
        }

        builder->CreateCondBr(cond_value, body_bb, exit_bb);

        // Generate body block
        builder->SetInsertPoint(body_bb);
        if (node->body)
        {
            node->body->accept(this);
        }
        // Loop back to condition
        if (!builder->GetInsertBlock()->getTerminator())
        {
            builder->CreateBr(cond_bb);
        }

        // Continue with exit block
        builder->SetInsertPoint(exit_bb);
    }

    void CodeGenerator::visit(ForStatementNode *node)
    {
        if (!node)
            return;

        // For loop structure: for (initializer; condition; incrementors) body
        // Example: for (i32 i = 0; i < 10; i++) { ... }

        // Generate initializer (e.g., i32 i = 0)
        if (node->initializer)
        {
            node->initializer->accept(this);
        }

        // Create blocks
        auto *cond_bb = node->condition ? llvm::BasicBlock::Create(*context, "forcond", current_function) : nullptr;
        auto *body_bb = llvm::BasicBlock::Create(*context, "forbody", current_function);

        // The update block executes the incrementor expressions (e.g., i++)
        // This happens AFTER the body but BEFORE the next condition check
        auto *update_bb = !node->incrementors.empty() ? llvm::BasicBlock::Create(*context, "forupdate", current_function) : nullptr;
        auto *exit_bb = llvm::BasicBlock::Create(*context, "forexit", current_function);

        // Control flow: init → condition → body → update → condition → ...

        // Branch to condition or body
        if (cond_bb)
        {
            builder->CreateBr(cond_bb);
            builder->SetInsertPoint(cond_bb);

            // Evaluate condition (e.g., i < 10)
            node->condition->accept(this);
            auto *cond_value = pop_value();
            if (!cond_value->getType()->isIntegerTy(1))
            {
                if (cond_value->getType()->isIntegerTy())
                {
                    cond_value = builder->CreateICmpNE(
                        cond_value,
                        llvm::ConstantInt::get(cond_value->getType(), 0),
                        "tobool");
                }
            }
            builder->CreateCondBr(cond_value, body_bb, exit_bb);
        }
        else
        {
            builder->CreateBr(body_bb);
        }

        // Generate body
        builder->SetInsertPoint(body_bb);
        if (node->body)
        {
            node->body->accept(this);
        }

        // After body, branch to update block (if exists) or back to condition
        if (!builder->GetInsertBlock()->getTerminator())
        {
            if (update_bb)
            {
                builder->CreateBr(update_bb);
            }
            else if (cond_bb)
            {
                builder->CreateBr(cond_bb);
            }
            else
            {
                builder->CreateBr(body_bb); // Infinite loop: for (;;)
            }
        }

        // Generate update block - this is where incrementors execute (e.g., i++)
        // The update happens AFTER each iteration but BEFORE checking the condition again
        if (update_bb)
        {
            builder->SetInsertPoint(update_bb);

            // Execute all incrementor expressions
            // Can have multiple: for (i = 0; i < 10; i++, j--)
            for (auto *incrementor : node->incrementors)
            {
                if (incrementor)
                {
                    incrementor->accept(this);
                    if (!value_stack.empty())
                        pop_value(); // Discard result
                }
            }

            // After update, go back to check condition (or body if no condition)
            if (cond_bb)
            {
                builder->CreateBr(cond_bb);
            }
            else
            {
                builder->CreateBr(body_bb);
            }
        }

        // Continue with code after the loop
        builder->SetInsertPoint(exit_bb);
    }

    void CodeGenerator::visit(ForInStatementNode *node)
    {
        if (!node)
            return;

        // ForInStatementNode is for range-based loops like: for (var i in 0..10)
        // This is a complex feature that would require:
        // 1. Evaluating the iterable (range expression or collection)
        // 2. Creating an iterator
        // 3. Generating the loop structure
        // For now, we'll report it as unsupported

        errors.push_back("For-in loops are not yet implemented");

        // Basic structure would be:
        // - Evaluate iterable to get start/end/step
        // - Create loop variable
        // - Generate condition check
        // - Generate body
        // - Generate increment
    }

    void CodeGenerator::visit(BreakStatementNode *node)
    {
        if (!node)
            return;

        // Break statements require maintaining a stack of loop contexts
        // to know which loop's exit block to jump to
        errors.push_back("Break statements are not yet implemented");

        // Would need something like:
        // if (!loop_context_stack.empty()) {
        //     auto* exit_block = loop_context_stack.top().exit_block;
        //     builder->CreateBr(exit_block);
        // }
    }

    void CodeGenerator::visit(ContinueStatementNode *node)
    {
        if (!node)
            return;

        // Continue statements require maintaining a stack of loop contexts
        // to know which loop's update/condition block to jump to
        errors.push_back("Continue statements are not yet implemented");

        // Would need something like:
        // if (!loop_context_stack.empty()) {
        //     auto* continue_block = loop_context_stack.top().continue_block;
        //     builder->CreateBr(continue_block);
        // }
    }

    void CodeGenerator::visit(EmptyStatementNode *node)
    {
        // Empty statement is just a semicolon - no code generation needed
        // Example: for (;;) or just a standalone semicolon
    }

    void CodeGenerator::visit(ReturnStatementNode *node)
    {
        if (!node)
            return;

        if (node->expression)
        {
            // Evaluate return expression
            node->expression->accept(this);
            auto *ret_value = pop_value();
            if (ret_value)
            {
                builder->CreateRet(ret_value);
            }
        }
        else
        {
            // Void return
            builder->CreateRetVoid();
        }
    }

    void CodeGenerator::visit(BinaryExpressionNode *node)
    {
        if (!node || !node->left || !node->right)
            return;

        // Handle short-circuiting for logical operators
        if (node->opKind == BinaryOperatorKind::LogicalAnd ||
            node->opKind == BinaryOperatorKind::LogicalOr)
        {

            // Evaluate left operand
            if (auto *left_expr = node->left->as<ExpressionNode>())
            {
                left_expr->accept(this);
            }
            else if (node->left->as<ErrorNode>())
            {
                errors.push_back("Error node in binary expression left operand");
                return;
            }
            auto *left = pop_value();
            if (!left)
                return;

            // Convert to boolean if needed
            if (!left->getType()->isIntegerTy(1))
            {
                if (left->getType()->isIntegerTy())
                {
                    left = builder->CreateICmpNE(
                        left, llvm::ConstantInt::get(left->getType(), 0), "tobool");
                }
            }

            // Save the current block (after left evaluation)
            auto *left_end_bb = builder->GetInsertBlock();

            // Create blocks for short-circuit evaluation
            auto *rhs_bb = llvm::BasicBlock::Create(*context, "rhs", current_function);
            auto *merge_bb = llvm::BasicBlock::Create(*context, "merge", current_function);

            if (node->opKind == BinaryOperatorKind::LogicalAnd)
            {
                builder->CreateCondBr(left, rhs_bb, merge_bb);
            }
            else
            {
                builder->CreateCondBr(left, merge_bb, rhs_bb);
            }

            // Evaluate right operand
            builder->SetInsertPoint(rhs_bb);
            if (auto *right_expr = node->right->as<ExpressionNode>())
            {
                right_expr->accept(this);
            }
            else if (node->right->as<ErrorNode>())
            {
                errors.push_back("Error node in binary expression right operand");
                return;
            }
            auto *right = pop_value();
            if (!right)
                return;

            if (!right->getType()->isIntegerTy(1))
            {
                if (right->getType()->isIntegerTy())
                {
                    right = builder->CreateICmpNE(
                        right, llvm::ConstantInt::get(right->getType(), 0), "tobool");
                }
            }

            auto *rhs_end_bb = builder->GetInsertBlock();
            builder->CreateBr(merge_bb);

            // Create PHI node to merge results
            builder->SetInsertPoint(merge_bb);
            auto *phi = builder->CreatePHI(llvm::Type::getInt1Ty(*context), 2, "logicaltmp");

            if (node->opKind == BinaryOperatorKind::LogicalAnd)
            {
                phi->addIncoming(llvm::ConstantInt::get(llvm::Type::getInt1Ty(*context), 0),
                                 left_end_bb);
                phi->addIncoming(right, rhs_end_bb);
            }
            else
            {
                phi->addIncoming(llvm::ConstantInt::get(llvm::Type::getInt1Ty(*context), 1),
                                 left_end_bb);
                phi->addIncoming(right, rhs_end_bb);
            }

            push_value(phi);
            return;
        }

        // Regular binary operators - evaluate operands
        if (auto *left_expr = node->left->as<ExpressionNode>())
        {
            left_expr->accept(this);
        }
        else if (node->left->as<ErrorNode>())
        {
            errors.push_back("Error node in binary expression left operand");
            return;
        }
        auto *left = pop_value();
        if (!left)
            return;

        if (auto *right_expr = node->right->as<ExpressionNode>())
        {
            right_expr->accept(this);
        }
        else if (node->right->as<ErrorNode>())
        {
            errors.push_back("Error node in binary expression right operand");
            return;
        }
        auto *right = pop_value();
        if (!right)
            return;

        llvm::Value *result = nullptr;
        bool is_float = left->getType()->isFloatingPointTy();

        switch (node->opKind)
        {
        // Arithmetic
        case BinaryOperatorKind::Add:
            result = is_float ? builder->CreateFAdd(left, right, "addtmp")
                              : builder->CreateAdd(left, right, "addtmp");
            break;
        case BinaryOperatorKind::Subtract:
            result = is_float ? builder->CreateFSub(left, right, "subtmp")
                              : builder->CreateSub(left, right, "subtmp");
            break;
        case BinaryOperatorKind::Multiply:
            result = is_float ? builder->CreateFMul(left, right, "multmp")
                              : builder->CreateMul(left, right, "multmp");
            break;
        case BinaryOperatorKind::Divide:
            result = is_float ? builder->CreateFDiv(left, right, "divtmp")
                              : builder->CreateSDiv(left, right, "divtmp");
            break;
        case BinaryOperatorKind::Modulo:
            if (!is_float)
            {
                result = builder->CreateSRem(left, right, "modtmp");
            }
            else
            {
                result = builder->CreateFRem(left, right, "modtmp");
            }
            break;

        // Comparisons
        case BinaryOperatorKind::Equals:
            result = is_float ? builder->CreateFCmpOEQ(left, right, "eqtmp")
                              : builder->CreateICmpEQ(left, right, "eqtmp");
            break;
        case BinaryOperatorKind::NotEquals:
            result = is_float ? builder->CreateFCmpONE(left, right, "netmp")
                              : builder->CreateICmpNE(left, right, "netmp");
            break;
        case BinaryOperatorKind::LessThan:
            result = is_float ? builder->CreateFCmpOLT(left, right, "lttmp")
                              : builder->CreateICmpSLT(left, right, "lttmp");
            break;
        case BinaryOperatorKind::LessThanOrEqual:
            result = is_float ? builder->CreateFCmpOLE(left, right, "letmp")
                              : builder->CreateICmpSLE(left, right, "letmp");
            break;
        case BinaryOperatorKind::GreaterThan:
            result = is_float ? builder->CreateFCmpOGT(left, right, "gttmp")
                              : builder->CreateICmpSGT(left, right, "gttmp");
            break;
        case BinaryOperatorKind::GreaterThanOrEqual:
            result = is_float ? builder->CreateFCmpOGE(left, right, "getmp")
                              : builder->CreateICmpSGE(left, right, "getmp");
            break;

        // Bitwise
        case BinaryOperatorKind::BitwiseAnd:
            result = builder->CreateAnd(left, right, "andtmp");
            break;
        case BinaryOperatorKind::BitwiseOr:
            result = builder->CreateOr(left, right, "ortmp");
            break;
        case BinaryOperatorKind::BitwiseXor:
            result = builder->CreateXor(left, right, "xortmp");
            break;

        default:
            errors.push_back("Unsupported binary operator");
            break;
        }

        if (result)
        {
            push_value(result);
        }
    }

    void CodeGenerator::visit(UnaryExpressionNode *node)
    {
        if (!node || !node->operand)
            return;

        // Evaluate operand
        node->operand->accept(this);
        auto *operand = pop_value();
        if (!operand)
            return;

        llvm::Value *result = nullptr;

        switch (node->opKind)
        {
        case UnaryOperatorKind::Minus:
            if (operand->getType()->isFloatingPointTy())
            {
                result = builder->CreateFNeg(operand, "negtmp");
            }
            else
            {
                result = builder->CreateNeg(operand, "negtmp");
            }
            break;
        case UnaryOperatorKind::Not:
            if (operand->getType()->isIntegerTy(1))
            {
                result = builder->CreateNot(operand, "nottmp");
            }
            else
            {
                // Convert to bool first then negate
                auto *tobool = builder->CreateICmpNE(
                    operand, llvm::ConstantInt::get(operand->getType(), 0), "tobool");
                result = builder->CreateNot(tobool, "nottmp");
            }
            break;
        case UnaryOperatorKind::BitwiseNot:
            result = builder->CreateNot(operand, "bitnottmp");
            break;
        default:
            errors.push_back("Unsupported unary operator");
            break;
        }

        if (result)
        {
            push_value(result);
        }
    }

    void CodeGenerator::visit(AssignmentExpressionNode *node)
    {
        if (!node || !node->target || !node->source)
            return;

        // Get the identifier being assigned to
        auto *id_expr = node->target->as<IdentifierExpressionNode>();
        if (!id_expr || !id_expr->identifier)
        {
            errors.push_back("Assignment target must be an identifier");
            return;
        }

        // Look up the variable
        std::string var_name(id_expr->identifier->name);
        auto *var_symbol = symbol_table.lookup_handle(node->containingScope)->as<Scope>()->lookup(var_name);

        if (!var_symbol)
        {
            errors.push_back("Variable not found: " + var_name);
            return;
        }

        // Get the alloca for this variable
        auto it = locals.find(var_symbol);
        if (it == locals.end())
        {
            errors.push_back("Variable not in scope: " + var_name);
            return;
        }

        // Evaluate the source expression
        node->source->accept(this);
        auto *value = pop_value();
        if (!value)
            return;

        // Store the value
        builder->CreateStore(value, it->second);

        // Push the value back (assignments are expressions)
        push_value(value);
    }

    void CodeGenerator::visit(CallExpressionNode *node)
    {
        if (!node || !node->target)
            return;

        // Get function name (simplified - assumes direct identifier)
        auto *id_expr = node->target->as<IdentifierExpressionNode>();
        if (!id_expr || !id_expr->identifier)
        {
            errors.push_back("Function call target must be an identifier");
            return;
        }

        std::string func_name(id_expr->identifier->name);

        // Look up function in module
        auto *callee = module->getFunction(func_name);
        if (!callee)
        {
            errors.push_back("Unknown function: " + func_name);
            return;
        }

        // Evaluate arguments
        std::vector<llvm::Value *> args;
        for (auto *arg : node->arguments)
        {
            if (arg)
            {
                // Arguments could be expressions or error nodes
                if (auto *expr = arg->as<ExpressionNode>())
                {
                    expr->accept(this);
                    auto *arg_value = pop_value();
                    if (arg_value)
                    {
                        args.push_back(arg_value);
                    }
                }
                else if (arg->as<ErrorNode>())
                {
                    errors.push_back("Error node in function arguments");
                }
            }
        }

        // Check argument count
        if (args.size() != callee->arg_size())
        {
            errors.push_back("Incorrect number of arguments for: " + func_name);
            return;
        }

        // Create call
        auto *call_value = builder->CreateCall(callee, args,
                                               callee->getReturnType()->isVoidTy() ? "" : "calltmp");

        // Only push non-void results
        if (!callee->getReturnType()->isVoidTy())
        {
            push_value(call_value);
        }
    }

    void CodeGenerator::visit(IdentifierExpressionNode *node)
    {
        if (!node || !node->identifier)
            return;

        // Look up the variable
        std::string var_name(node->identifier->name);
        auto *var_symbol = symbol_table.lookup_handle(node->containingScope)->as<Scope>()->lookup(var_name);

        if (!var_symbol)
        {
            errors.push_back("Identifier not found: " + var_name);
            return;
        }

        // Check if it's a function (for function pointers later)
        if (var_symbol->as<FunctionSymbol>())
        {
            // For now, just error - function pointers not implemented
            errors.push_back("Function used as value: " + var_name);
            return;
        }

        // Get the alloca for this variable
        auto it = locals.find(var_symbol);
        if (it == locals.end())
        {
            errors.push_back("Variable not in scope: " + var_name);
            return;
        }

        // Get the type for this variable (required for opaque pointers)
        auto type_it = local_types.find(var_symbol);
        if (type_it == local_types.end())
        {
            errors.push_back("Variable type not found: " + var_name);
            return;
        }

        // Load the value
        auto *loaded = builder->CreateLoad(type_it->second, it->second, var_name);
        push_value(loaded);
    }

    void CodeGenerator::visit(LiteralExpressionNode *node)
    {
        auto *constant = create_constant(node);
        if (constant)
        {
            push_value(constant);
        }
    }

    void CodeGenerator::visit(ParenthesizedExpressionNode *node)
    {
        if (!node || !node->expression)
            return;

        // Simply evaluate the inner expression
        node->expression->accept(this);
    }

} // namespace Myre