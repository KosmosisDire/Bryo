// codegen.cpp - LLVM Code Generator with Pre-declaration Support
#include "codegen/codegen.hpp"
#include <llvm/IR/Constants.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/Support/Casting.h>

namespace Myre {

// === Main Entry Points ===

std::unique_ptr<llvm::Module> CodeGenerator::generate(CompilationUnit* unit) {
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
    if (llvm::verifyModule(*module, &error_stream)) {
        // Module verification failure isn't tied to a specific line of code.
        report_general_error("Module verification failed: " + verify_error);
    }

    return std::move(module);
}

void CodeGenerator::declare_all_functions() {
    // Start from global namespace and recursively declare all functions
    auto* global_scope = symbol_table.get_global_namespace();
    if (global_scope) {
        declare_all_functions_in_scope(global_scope);
    }
}

void CodeGenerator::declare_all_functions_in_scope(Scope* scope) {
    if (!scope) return;

    // Declare functions in this scope
    for (const auto& [name, symbol] : scope->symbols) {
        if (auto* func_sym = symbol->as<FunctionSymbol>()) {
            declare_function_from_symbol(func_sym);
        }

        // Recursively process nested scopes (namespaces, types, etc.)
        if (auto* nested_scope = symbol->as<Scope>()) {
            declare_all_functions_in_scope(nested_scope);
        }
    }
}

llvm::Function* CodeGenerator::declare_function_from_symbol(FunctionSymbol* func_symbol) {
    if (!func_symbol) return nullptr;

    const std::string& func_name = func_symbol->get_qualified_name();

    // Check if already declared
    if (declared_functions.count(func_name) > 0) {
        return module->getFunction(func_name);
    }

    // Build parameter types
    std::vector<llvm::Type*> param_types;
    for (const auto& param_type : func_symbol->parameter_types()) {
        param_types.push_back(get_llvm_type(param_type));
    }

    // Get return type
    llvm::Type* return_type = get_llvm_type(func_symbol->return_type());

    // Create function type
    auto* func_type = llvm::FunctionType::get(return_type, param_types, false);

    // Create function declaration (no body)
    auto* function = llvm::Function::Create(
        func_type,
        llvm::Function::ExternalLinkage,
        func_name,
        module.get());

    declared_functions.insert(func_name);
    return function;
}

void CodeGenerator::generate_definitions(CompilationUnit* unit) {
    // Only generate function bodies, skip declarations
    // This is used for multi-file compilation
    if (!unit) return;

    for (auto* stmt : unit->topLevelStatements) {
        if (stmt) {
            stmt->accept(this);
        }
    }
}

// === Helper Methods ===

Scope* CodeGenerator::get_containing_scope(Node* node) {
    if (!node || node->containingScope.id == 0) return nullptr;
    return symbol_table.lookup_handle(node->containingScope)->as<Scope>();
}

std::string CodeGenerator::build_qualified_name(NameExpr* name_expr) {
    if (!name_expr || name_expr->parts.empty()) return "";
    
    std::string result;
    for (size_t i = 0; i < name_expr->parts.size(); ++i) {
        if (i > 0) result += "::";
        result += name_expr->parts[i]->text;
    }
    return result;
}

void CodeGenerator::push_value(llvm::Value* val) {
    if (val) {
        value_stack.push(val);
    } else {
        report_general_error("Internal error: Attempted to push null value to stack");
    }
}

llvm::Value* CodeGenerator::pop_value() {
    if (value_stack.empty()) {
        report_general_error("Internal error: Attempted to pop from empty value stack");
        return nullptr;
    }
    auto* val = value_stack.top();
    value_stack.pop();
    return val;
}

llvm::Type* CodeGenerator::get_llvm_type(TypePtr type) {
    if (!type) {
        report_general_error("Internal error: Null type encountered");
        return llvm::Type::getVoidTy(*context);
    }

    // Check cache first
    auto it = type_cache.find(type);
    if (it != type_cache.end()) {
        return it->second;
    }

    llvm::Type* llvm_type = nullptr;

    if (auto* prim = std::get_if<PrimitiveType>(&type->value)) {
        switch (prim->kind) {
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
            report_general_error("Unsupported primitive type");
            llvm_type = llvm::Type::getVoidTy(*context);
            break;
        }
    }
    else if (std::get_if<UnresolvedType>(&type->value)) {
        // This should be caught earlier, but handle defensively.
        report_general_error("Unresolved type encountered during codegen");
        llvm_type = llvm::Type::getVoidTy(*context);
    }
    else {
        report_general_error("Unsupported type kind");
        llvm_type = llvm::Type::getVoidTy(*context);
    }

    type_cache[type] = llvm_type;
    return llvm_type;
}

llvm::Type* CodeGenerator::get_llvm_type_from_ref(TypeRef* type_ref) {
    if (!type_ref) {
        return llvm::Type::getVoidTy(*context);
    }
    
    // For now, simplified handling - would need full type resolution
    if (auto* named = type_ref->as<NamedTypeRef>()) {
        // Look up the type in symbol table
        std::string type_name;
        for (size_t i = 0; i < named->path.size(); ++i) {
            if (i > 0) type_name += "::";
            type_name += named->path[i]->text;
        }
        
        // Simplified mapping for common types
        if (type_name == "i32") return llvm::Type::getInt32Ty(*context);
        if (type_name == "i64") return llvm::Type::getInt64Ty(*context);
        if (type_name == "f32") return llvm::Type::getFloatTy(*context);
        if (type_name == "f64") return llvm::Type::getDoubleTy(*context);
        if (type_name == "bool") return llvm::Type::getInt1Ty(*context);
        if (type_name == "void") return llvm::Type::getVoidTy(*context);
    }
    
    report_error(type_ref, "Complex type references not yet supported");
    return llvm::Type::getVoidTy(*context);
}

llvm::Value* CodeGenerator::create_constant(LiteralExpr* literal) {
    if (!literal) {
        report_general_error("Internal error: Invalid literal node");
        return nullptr;
    }

    std::string text(literal->value);

    switch (literal->kind) {
    case LiteralExpr::Kind::Integer: {
        // Try to determine size from suffix or default to i32
        if (text.find("i64") != std::string::npos || text.find("l") != std::string::npos) {
            int64_t val = std::stoll(text);
            return llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context), val);
        } else {
            int64_t val = std::stoll(text);
            return llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), val);
        }
    }
    case LiteralExpr::Kind::Float: {
        if (text.find("f32") != std::string::npos || text.find("f") != std::string::npos) {
            double val = std::stod(text);
            return llvm::ConstantFP::get(llvm::Type::getFloatTy(*context), val);
        } else {
            double val = std::stod(text);
            return llvm::ConstantFP::get(llvm::Type::getDoubleTy(*context), val);
        }
    }
    case LiteralExpr::Kind::Bool: {
        bool val = (text == "true");
        return llvm::ConstantInt::get(llvm::Type::getInt1Ty(*context), val ? 1 : 0);
    }
    default:
        report_error(literal, "Unsupported literal type");
        return nullptr;
    }
}

void CodeGenerator::ensure_terminator() {
    auto* bb = builder->GetInsertBlock();
    if (bb && !bb->getTerminator()) {
        // Add a default return based on function type
        if (current_function) {
            auto* ret_type = current_function->getReturnType();
            if (ret_type->isVoidTy()) {
                builder->CreateRetVoid();
            } else if (ret_type->isIntegerTy()) {
                builder->CreateRet(llvm::ConstantInt::get(ret_type, 0));
            } else if (ret_type->isFloatingPointTy()) {
                builder->CreateRet(llvm::ConstantFP::get(ret_type, 0.0));
            }
        }
    }
}

void CodeGenerator::report_error(const Node* node, const std::string& message) {
    if (node) {
        errors.push_back({message, node->location});
    } else {
        errors.push_back({message, {}}); // No location available
    }
}

void CodeGenerator::report_general_error(const std::string& message) {
    errors.push_back({message, {}});
}

// === Visitor Implementations ===

void CodeGenerator::visit(CompilationUnit* node) {
    if (!node) return;

    // Visit all top-level statements and declarations
    for (auto* stmt : node->topLevelStatements) {
        if (stmt) {
            stmt->accept(this);
        }
    }
}

void CodeGenerator::visit(NamespaceDecl* node) {
    if (!node) return;
    
    // Visit namespace body if present
    if (node->body) {
        for (auto* stmt : *node->body) {
            if (stmt) {
                stmt->accept(this);
            }
        }
    }
}

void CodeGenerator::visit(FunctionDecl* node) {
    if (!node || !node->name) return;

    // Get function symbol from resolved handle
    auto* func_symbol = symbol_table.lookup_handle(node->functionSymbol)->as<FunctionSymbol>();
    if (!func_symbol) {
        report_error(node, "Function symbol not found for '" + std::string(node->name->text) + "'");
        return;
    }

    std::string func_name = func_symbol->get_qualified_name();

    // Get the already-declared function
    auto* function = module->getFunction(func_name);
    if (!function) {
        report_error(node, "Function not declared: " + func_name);
        return;
    }

    // Skip if already has a body
    if (!function->empty()) {
        return;
    }

    // Only generate body for non-abstract functions
    if (!node->body) {
        return;
    }

    // Set up for body generation
    current_function = function;
    locals.clear();
    local_types.clear();

    // Create entry block
    auto* entry = llvm::BasicBlock::Create(*context, "entry", function);
    builder->SetInsertPoint(entry);

    // Map parameters to allocas
    size_t idx = 0;
    for (auto& arg : function->args()) {
        if (idx < node->parameters.size()) {
            auto* param_decl = node->parameters[idx];
            if (param_decl && param_decl->param && param_decl->param->name) {
                // Look up parameter symbol in function scope
                auto* param_sym = func_symbol->lookup(param_decl->param->name->text);
                if (param_sym) {
                    // Create alloca for parameter
                    auto* alloca = builder->CreateAlloca(
                        arg.getType(), nullptr, param_decl->param->name->text);
                    builder->CreateStore(&arg, alloca);
                    locals[param_sym] = alloca;
                    local_types[param_sym] = arg.getType();

                    // Set argument name for readability
                    arg.setName(param_decl->param->name->text);
                }
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

void CodeGenerator::visit(VariableDecl* node) {
    if (!node || !node->variable || !node->variable->name) return;

    // Get variable symbol from resolved handle
    auto* parent_scope = get_containing_scope(node);
    auto* var_symbol = parent_scope->lookup(node->variable->name->text);
    if (!var_symbol) {
        report_error(node, "Variable symbol not found for '" + std::string(node->variable->name->text) + "'");
        return;
    }

    auto* typed_symbol = dynamic_cast<TypedSymbol*>(var_symbol);
    if (!typed_symbol) {
        report_error(node, "Variable '" + std::string(node->variable->name->text) + "' has no type information");
        return;
    }

    // Get LLVM type
    llvm::Type* llvm_type = nullptr;
    if (node->variable->type) {
        llvm_type = get_llvm_type_from_ref(node->variable->type);
    } else {
        llvm_type = get_llvm_type(typed_symbol->type());
    }

    // Create alloca
    std::string var_name(node->variable->name->text);

    if (llvm_type->isVoidTy())
    {
        report_error(node, "Cannot create variable '" + var_name + 
                        "' with void type - check return type inference");
        return;
    }

    auto* alloca = builder->CreateAlloca(llvm_type, nullptr, var_name);
    locals[var_symbol] = alloca;
    local_types[var_symbol] = llvm_type;

    // Handle initializer if present
    if (node->initializer) {
        node->initializer->accept(this);
        if (!value_stack.empty()) {
            auto* init_value = pop_value();
            if (init_value) {
                builder->CreateStore(init_value, alloca);
            }
        }
    }
}

void CodeGenerator::visit(ParameterDecl* node) {
    // Parameters are handled in FunctionDecl
}

void CodeGenerator::visit(TypeDecl* node) {
    // Type declarations not yet implemented
    if (!node) return;
    
    // Visit member declarations
    for (auto* member : node->members) {
        if (member) {
            member->accept(this);
        }
    }
}

void CodeGenerator::visit(Block* node) {
    if (!node) return;

    // Visit all statements in the block
    for (auto* stmt : node->statements) {
        if (stmt) {
            stmt->accept(this);
        }
    }
}

void CodeGenerator::visit(ExpressionStmt* node) {
    if (!node || !node->expression) return;

    // Evaluate expression (result will be discarded)
    node->expression->accept(this);
    
    // Pop and discard the result if any
    if (!value_stack.empty()) {
        pop_value();
    }
}

void CodeGenerator::visit(IfExpr* node) {
    if (!node || !node->condition) return;

    // Evaluate condition
    node->condition->accept(this);
    auto* cond_value = pop_value();
    if (!cond_value) return;

    // Ensure condition is boolean
    if (!cond_value->getType()->isIntegerTy(1)) {
        if (cond_value->getType()->isIntegerTy()) {
            cond_value = builder->CreateICmpNE(
                cond_value,
                llvm::ConstantInt::get(cond_value->getType(), 0),
                "tobool");
        } else {
            report_error(node->condition, "If condition must be a boolean or integer type");
            return;
        }
    }

    // Create blocks
    auto* then_bb = llvm::BasicBlock::Create(*context, "then", current_function);
    auto* else_bb = node->elseBranch ? 
        llvm::BasicBlock::Create(*context, "else", current_function) : nullptr;
    auto* merge_bb = llvm::BasicBlock::Create(*context, "ifcont", current_function);

    // Create conditional branch
    if (else_bb) {
        builder->CreateCondBr(cond_value, then_bb, else_bb);
    } else {
        builder->CreateCondBr(cond_value, then_bb, merge_bb);
    }

    // Generate then block
    builder->SetInsertPoint(then_bb);
    if (node->thenBranch) {
        node->thenBranch->accept(this);
    }
    if (!builder->GetInsertBlock()->getTerminator()) {
        builder->CreateBr(merge_bb);
    }

    // Generate else block if present
    if (else_bb) {
        builder->SetInsertPoint(else_bb);
        if (node->elseBranch) {
            node->elseBranch->accept(this);
        }
        if (!builder->GetInsertBlock()->getTerminator()) {
            builder->CreateBr(merge_bb);
        }
    }

    // Continue with merge block
    builder->SetInsertPoint(merge_bb);
}

void CodeGenerator::visit(ConditionalExpr* node) {
    if (!node) return;
    
    // Similar to IfExpr but returns a value
    node->condition->accept(this);
    auto* cond_value = pop_value();
    if (!cond_value) return;

    // Ensure boolean condition
    if (!cond_value->getType()->isIntegerTy(1)) {
        if (cond_value->getType()->isIntegerTy()) {
            cond_value = builder->CreateICmpNE(
                cond_value,
                llvm::ConstantInt::get(cond_value->getType(), 0),
                "tobool");
        }
    }

    auto* then_bb = llvm::BasicBlock::Create(*context, "ternary.then", current_function);
    auto* else_bb = llvm::BasicBlock::Create(*context, "ternary.else", current_function);
    auto* merge_bb = llvm::BasicBlock::Create(*context, "ternary.cont", current_function);

    builder->CreateCondBr(cond_value, then_bb, else_bb);

    // Then branch
    builder->SetInsertPoint(then_bb);
    node->thenExpr->accept(this);
    auto* then_value = pop_value();
    auto* then_end_bb = builder->GetInsertBlock();
    builder->CreateBr(merge_bb);

    // Else branch
    builder->SetInsertPoint(else_bb);
    node->elseExpr->accept(this);
    auto* else_value = pop_value();
    auto* else_end_bb = builder->GetInsertBlock();
    builder->CreateBr(merge_bb);

    // Merge with PHI
    builder->SetInsertPoint(merge_bb);
    auto* phi = builder->CreatePHI(then_value->getType(), 2, "ternary.result");
    phi->addIncoming(then_value, then_end_bb);
    phi->addIncoming(else_value, else_end_bb);
    push_value(phi);
}

void CodeGenerator::visit(WhileStmt* node) {
    if (!node || !node->condition) return;

    // Create blocks
    auto* cond_bb = llvm::BasicBlock::Create(*context, "while.cond", current_function);
    auto* body_bb = llvm::BasicBlock::Create(*context, "while.body", current_function);
    auto* exit_bb = llvm::BasicBlock::Create(*context, "while.exit", current_function);

    // Branch to condition block
    builder->CreateBr(cond_bb);

    // Generate condition block
    builder->SetInsertPoint(cond_bb);
    node->condition->accept(this);
    auto* cond_value = pop_value();
    if (!cond_value) return;

    // Ensure boolean condition
    if (!cond_value->getType()->isIntegerTy(1)) {
        if (cond_value->getType()->isIntegerTy()) {
            cond_value = builder->CreateICmpNE(
                cond_value,
                llvm::ConstantInt::get(cond_value->getType(), 0),
                "tobool");
        }
    }

    builder->CreateCondBr(cond_value, body_bb, exit_bb);

    // Generate body block
    builder->SetInsertPoint(body_bb);
    if (node->body) {
        node->body->accept(this);
    }
    if (!builder->GetInsertBlock()->getTerminator()) {
        builder->CreateBr(cond_bb);
    }

    // Continue with exit block
    builder->SetInsertPoint(exit_bb);
}

void CodeGenerator::visit(ForStmt* node) {
    if (!node) return;

    // Generate initializer
    if (node->initializer) {
        node->initializer->accept(this);
    }

    // Create blocks
    auto* cond_bb = node->condition ? 
        llvm::BasicBlock::Create(*context, "for.cond", current_function) : nullptr;
    auto* body_bb = llvm::BasicBlock::Create(*context, "for.body", current_function);
    auto* update_bb = !node->updates.empty() ? 
        llvm::BasicBlock::Create(*context, "for.update", current_function) : nullptr;
    auto* exit_bb = llvm::BasicBlock::Create(*context, "for.exit", current_function);

    // Branch to condition or body
    if (cond_bb) {
        builder->CreateBr(cond_bb);
        builder->SetInsertPoint(cond_bb);

        // Evaluate condition
        node->condition->accept(this);
        auto* cond_value = pop_value();
        
        if (!cond_value) {
            report_error(node->condition, "Failed to evaluate for loop condition");
            return;
        }
        
        if (!cond_value->getType()->isIntegerTy(1)) {
            if (cond_value->getType()->isIntegerTy()) {
                cond_value = builder->CreateICmpNE(
                    cond_value,
                    llvm::ConstantInt::get(cond_value->getType(), 0),
                    "tobool");
            }
        }
        builder->CreateCondBr(cond_value, body_bb, exit_bb);
    } else {
        builder->CreateBr(body_bb);
    }

    // Generate body
    builder->SetInsertPoint(body_bb);
    if (node->body) {
        node->body->accept(this);
    }

    // After body, branch to update block or back to condition
    if (!builder->GetInsertBlock()->getTerminator()) {
        if (update_bb) {
            builder->CreateBr(update_bb);
        } else if (cond_bb) {
            builder->CreateBr(cond_bb);
        } else {
            builder->CreateBr(body_bb); // Infinite loop
        }
    }

    // Generate update block
    if (update_bb) {
        builder->SetInsertPoint(update_bb);
        for (auto* update : node->updates) {
            if (update) {
                update->accept(this);
                if (!value_stack.empty()) {
                    pop_value(); // Discard result
                }
            }
        }
        
        if (cond_bb) {
            builder->CreateBr(cond_bb);
        } else {
            builder->CreateBr(body_bb);
        }
    }

    // Continue with exit block
    builder->SetInsertPoint(exit_bb);
}

void CodeGenerator::visit(ForInStmt* node) {
    if (!node) return;
    report_error(node, "For-in loops are not yet implemented");
}

void CodeGenerator::visit(BreakStmt* node) {
    if (!node) return;
    report_error(node, "Break statements are not yet implemented");
}

void CodeGenerator::visit(ContinueStmt* node) {
    if (!node) return;
    report_error(node, "Continue statements are not yet implemented");
}

void CodeGenerator::visit(ReturnStmt* node) {
    if (!node) return;

    if (node->value) {
        // Evaluate return expression
        node->value->accept(this);
        auto* ret_value = pop_value();
        if (ret_value) {
            builder->CreateRet(ret_value);
        }
    } else {
        // Void return
        builder->CreateRetVoid();
    }
}

void CodeGenerator::visit(BinaryExpr* node) {
    if (!node || !node->left || !node->right) return;

    // Handle short-circuiting for logical operators
    if (node->op == BinaryOperatorKind::LogicalAnd ||
        node->op == BinaryOperatorKind::LogicalOr) {
        
        // Evaluate left operand
        node->left->accept(this);
        auto* left = pop_value();
        if (!left) return;

        // Convert to boolean if needed
        if (!left->getType()->isIntegerTy(1)) {
            if (left->getType()->isIntegerTy()) {
                left = builder->CreateICmpNE(
                    left, llvm::ConstantInt::get(left->getType(), 0), "tobool");
            }
        }

        auto* left_end_bb = builder->GetInsertBlock();

        // Create blocks for short-circuit evaluation
        auto* rhs_bb = llvm::BasicBlock::Create(*context, "rhs", current_function);
        auto* merge_bb = llvm::BasicBlock::Create(*context, "merge", current_function);

        if (node->op == BinaryOperatorKind::LogicalAnd) {
            builder->CreateCondBr(left, rhs_bb, merge_bb);
        } else {
            builder->CreateCondBr(left, merge_bb, rhs_bb);
        }

        // Evaluate right operand
        builder->SetInsertPoint(rhs_bb);
        node->right->accept(this);
        auto* right = pop_value();
        if (!right) return;

        if (!right->getType()->isIntegerTy(1)) {
            if (right->getType()->isIntegerTy()) {
                right = builder->CreateICmpNE(
                    right, llvm::ConstantInt::get(right->getType(), 0), "tobool");
            }
        }

        auto* rhs_end_bb = builder->GetInsertBlock();
        builder->CreateBr(merge_bb);

        // Create PHI node to merge results
        builder->SetInsertPoint(merge_bb);
        auto* phi = builder->CreatePHI(llvm::Type::getInt1Ty(*context), 2, "logicaltmp");

        if (node->op == BinaryOperatorKind::LogicalAnd) {
            phi->addIncoming(llvm::ConstantInt::get(llvm::Type::getInt1Ty(*context), 0),
                           left_end_bb);
            phi->addIncoming(right, rhs_end_bb);
        } else {
            phi->addIncoming(llvm::ConstantInt::get(llvm::Type::getInt1Ty(*context), 1),
                           left_end_bb);
            phi->addIncoming(right, rhs_end_bb);
        }

        push_value(phi);
        return;
    }

    // Regular binary operators - evaluate operands
    node->left->accept(this);
    auto* left = pop_value();
    if (!left) return;

    node->right->accept(this);
    auto* right = pop_value();
    if (!right) return;

    llvm::Value* result = nullptr;
    bool is_float = left->getType()->isFloatingPointTy();

    switch (node->op) {
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
        result = is_float ? builder->CreateFRem(left, right, "modtmp")
                         : builder->CreateSRem(left, right, "modtmp");
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
        report_error(node, "Unsupported binary operator");
        break;
    }

    if (result) {
        push_value(result);
    }
}

void CodeGenerator::visit(UnaryExpr* node) {
    if (!node || !node->operand) return;

    // Evaluate operand
    node->operand->accept(this);
    auto* operand = pop_value();
    if (!operand) return;

    llvm::Value* result = nullptr;

    switch (node->op) {
    case UnaryOperatorKind::Minus:
        if (operand->getType()->isFloatingPointTy()) {
            result = builder->CreateFNeg(operand, "negtmp");
        } else {
            result = builder->CreateNeg(operand, "negtmp");
        }
        break;
    case UnaryOperatorKind::Not:
        if (operand->getType()->isIntegerTy(1)) {
            result = builder->CreateNot(operand, "nottmp");
        } else {
            // Convert to bool first then negate
            auto* tobool = builder->CreateICmpNE(
                operand, llvm::ConstantInt::get(operand->getType(), 0), "tobool");
            result = builder->CreateNot(tobool, "nottmp");
        }
        break;
    case UnaryOperatorKind::BitwiseNot:
        result = builder->CreateNot(operand, "bitnottmp");
        break;
    default:
        report_error(node, "Unsupported unary operator");
        break;
    }

    if (result) {
        push_value(result);
    }
}

void CodeGenerator::visit(AssignmentExpr* node) {
    if (!node || !node->target || !node->value) return;

    // Get the identifier being assigned to
    auto* name_expr = node->target->as<NameExpr>();
    if (!name_expr || name_expr->parts.empty()) {
        report_error(node->target, "Assignment target must be an identifier");
        return;
    }

    // Look up the variable
    std::string var_name = build_qualified_name(name_expr);
    auto* parent_symbol = get_containing_scope(node->target);
    if (!parent_symbol) {
        report_error(node->target, "No containing scope found for assignment");
        return;
    }
    auto* var_symbol = parent_symbol->lookup(var_name);

    if (!var_symbol) {
        report_error(node->target, "Variable not found: " + var_name);
        return;
    }

    // Get the alloca for this variable
    auto it = locals.find(var_symbol);
    if (it == locals.end()) {
        report_error(node->target, "Variable not found in local scope: " + var_name);
        return;
    }

    // Evaluate the source expression
    node->value->accept(this);
    auto* value = pop_value();
    if (!value) return;

    // Handle compound assignment operators
    if (node->op != AssignmentOperatorKind::Assign) {
        // Load current value
        auto type_it = local_types.find(var_symbol);
        if (type_it == local_types.end()) {
            report_error(node->target, "Internal error: Variable type not found for '" + var_name + "'");
            return;
        }
        auto* current = builder->CreateLoad(type_it->second, it->second, "loadtmp");
        
        // Perform operation
        bool is_float = current->getType()->isFloatingPointTy();
        switch (node->op) {
        case AssignmentOperatorKind::Add:
            value = is_float ? builder->CreateFAdd(current, value, "addassign")
                            : builder->CreateAdd(current, value, "addassign");
            break;
        case AssignmentOperatorKind::Subtract:
            value = is_float ? builder->CreateFSub(current, value, "subassign")
                            : builder->CreateSub(current, value, "subassign");
            break;
        case AssignmentOperatorKind::Multiply:
            value = is_float ? builder->CreateFMul(current, value, "mulassign")
                            : builder->CreateMul(current, value, "mulassign");
            break;
        case AssignmentOperatorKind::Divide:
            value = is_float ? builder->CreateFDiv(current, value, "divassign")
                            : builder->CreateSDiv(current, value, "divassign");
            break;
        default:
            report_error(node, "Unsupported compound assignment operator");
            break;
        }
    }

    // Store the value
    builder->CreateStore(value, it->second);

    // Push the value back (assignments are expressions)
    push_value(value);
}

void CodeGenerator::visit(CallExpr* node) {
    if (!node || !node->callee) return;

    // Get function name (simplified - assumes direct identifier)
    auto* name_expr = node->callee->as<NameExpr>();
    if (!name_expr || name_expr->parts.empty()) {
        report_error(node->callee, "Function call target must be an identifier");
        return;
    }

    std::string func_name = build_qualified_name(name_expr);

    // Look up function in module
    auto* callee = module->getFunction(func_name);
    if (!callee) {
        report_error(node->callee, "Unknown function: " + func_name);
        return;
    }

    // Evaluate arguments
    std::vector<llvm::Value*> args;
    for (auto* arg : node->arguments) {
        if (arg) {
            arg->accept(this);
            auto* arg_value = pop_value();
            if (arg_value) {
                args.push_back(arg_value);
            }
        }
    }

    // Check argument count
    if (args.size() != callee->arg_size()) {
        report_error(node, "Incorrect number of arguments for '" + func_name + "'. "
            "Expected " + std::to_string(callee->arg_size()) +
            ", but got " + std::to_string(args.size()) + ".");
        return;
    }

    // Create call
    auto* call_value = builder->CreateCall(callee, args,
                                          callee->getReturnType()->isVoidTy() ? "" : "calltmp");

    // Only push non-void results
    if (!callee->getReturnType()->isVoidTy()) {
        push_value(call_value);
    }
}

void CodeGenerator::visit(NameExpr* node) {
    if (!node || node->parts.empty()) return;

    // Look up the variable
    std::string var_name = build_qualified_name(node);
    auto* parent_symbol = get_containing_scope(node);
    auto* var_symbol = parent_symbol->lookup(var_name);

    if (!var_symbol) {
        report_error(node, "Identifier not found: " + var_name);
        return;
    }

    // Check if it's a function (for function pointers later)
    if (var_symbol->as<FunctionSymbol>()) {
        report_error(node, "Using a function name as a value is not yet supported: " + var_name);
        return;
    }

    // Get the alloca for this variable
    auto it = locals.find(var_symbol);
    if (it == locals.end()) {
        report_error(node, "Variable not found in local scope: " + var_name);
        return;
    }

    // Get the type for this variable
    auto type_it = local_types.find(var_symbol);
    if (type_it == local_types.end()) {
        report_error(node, "Internal error: Variable type not found: " + var_name);
        return;
    }

    // Load the value
    auto* loaded = builder->CreateLoad(type_it->second, it->second, var_name);
    push_value(loaded);
}

void CodeGenerator::visit(LiteralExpr* node) {
    auto* constant = create_constant(node);
    if (constant) {
        push_value(constant);
    }
}

// Error handling
void CodeGenerator::visit(ErrorExpression* node) {
    if (!node) return;
    report_error(node, "Error expression: " + std::string(node->message));
}

void CodeGenerator::visit(ErrorStatement* node) {
    if (!node) return;
    report_error(node, "Error statement: " + std::string(node->message));
}

void CodeGenerator::visit(ErrorTypeRef* node) {
    if (!node) return;
    report_error(node, "Error type reference: " + std::string(node->message));
}

} // namespace Myre