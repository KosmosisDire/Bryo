#include "sharpie/compiler/codegen/codegen.hpp"
#include "llvm/IR/Constants.h"
#include "llvm/ADT/APFloat.h"
#include "sharpie/common/logger.hpp"



namespace Mycelium::Scripting::Lang::CodeGen {

CodeGenerator::ExpressionCGResult CodeGenerator::cg_expression(std::shared_ptr<ExpressionNode> node) {
    if (auto literal = std::dynamic_pointer_cast<LiteralExpressionNode>(node))
        return cg_literal_expression(literal);
    if (auto identifier = std::dynamic_pointer_cast<IdentifierExpressionNode>(node))
        return cg_identifier_expression(identifier);
    if (auto binary = std::dynamic_pointer_cast<BinaryExpressionNode>(node))
        return cg_binary_expression(binary);
    if (auto assignment = std::dynamic_pointer_cast<AssignmentExpressionNode>(node))
        return cg_assignment_expression(assignment);
    if (auto unary = std::dynamic_pointer_cast<UnaryExpressionNode>(node))
        return cg_unary_expression(unary);
    if (auto method_call = std::dynamic_pointer_cast<MethodCallExpressionNode>(node))
        return cg_method_call_expression(method_call);
    if (auto object_creation = std::dynamic_pointer_cast<ObjectCreationExpressionNode>(node))
        return cg_object_creation_expression(object_creation);
    if (auto this_expr = std::dynamic_pointer_cast<ThisExpressionNode>(node))
        return cg_this_expression(this_expr);
    if (auto cast = std::dynamic_pointer_cast<CastExpressionNode>(node))
        return cg_cast_expression(cast);
    if (auto member_access = std::dynamic_pointer_cast<MemberAccessExpressionNode>(node))
        return cg_member_access_expression(member_access);
    if (auto parenthesized = std::dynamic_pointer_cast<ParenthesizedExpressionNode>(node))
        return cg_parenthesized_expression(parenthesized);

    log_compiler_error("Unsupported expression type in code generation.", node ? node->location : std::nullopt);
}

CodeGenerator::ExpressionCGResult CodeGenerator::cg_literal_expression(std::shared_ptr<LiteralExpressionNode> node) {
    switch (node->kind) {
        case LiteralKind::Integer:
            try {
                return ExpressionCGResult(llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx.llvm_context), static_cast<int32_t>(std::stoll(node->valueText)), true));
            } catch (const std::exception& e) {
                log_compiler_error("Invalid int literal: " + node->valueText, node->location);
            }
            break;
        case LiteralKind::Long:
            try {
                return ExpressionCGResult(llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx.llvm_context), static_cast<int64_t>(std::stoll(node->valueText)), true));
            } catch (const std::exception& e) {
                log_compiler_error("Invalid long literal: " + node->valueText, node->location);
            }
            break;
        case LiteralKind::Float:
            try {
                return ExpressionCGResult(llvm::ConstantFP::get(llvm::Type::getFloatTy(ctx.llvm_context), llvm::APFloat(std::stof(node->valueText))));
            } catch (const std::exception& e) {
                log_compiler_error("Invalid float literal: " + node->valueText, node->location);
            }
            break;
        case LiteralKind::Double:
            try {
                return ExpressionCGResult(llvm::ConstantFP::get(llvm::Type::getDoubleTy(ctx.llvm_context), llvm::APFloat(std::stod(node->valueText))));
            } catch (const std::exception& e) {
                log_compiler_error("Invalid double literal: " + node->valueText, node->location);
            }
            break;
        case LiteralKind::Boolean:
            return ExpressionCGResult(llvm::ConstantInt::get(llvm::Type::getInt1Ty(ctx.llvm_context), (node->valueText == "true")));
        case LiteralKind::Char:
            if (node->valueText.length() == 1) {
                return ExpressionCGResult(llvm::ConstantInt::get(llvm::Type::getInt8Ty(ctx.llvm_context), node->valueText[0]));
            } else {
                log_compiler_error("Invalid char literal: " + node->valueText, node->location);
            }
            break;
        case LiteralKind::String: {
            auto res = ExpressionCGResult(create_string_from_literal(ctx, node->valueText));
            res.primitive_info = ctx.primitive_registry.get_by_simple_name("string");
            return res;
        }
        case LiteralKind::Null:
            return ExpressionCGResult(llvm::ConstantPointerNull::get(llvm::PointerType::get(ctx.llvm_context, 0)));
        default:
            log_compiler_error("Unhandled literal kind.", node->location);
    }
    return ExpressionCGResult(nullptr);
}

CodeGenerator::ExpressionCGResult CodeGenerator::cg_identifier_expression(std::shared_ptr<IdentifierExpressionNode> node) {
    const std::string& name = node->identifier->name;

    // 1. Check for local variables or parameters
    auto it = ctx.named_values.find(name);
    if (it != ctx.named_values.end()) {
        const VariableInfo& varInfo = it->second;
        llvm::Value* loaded_val = ctx.builder.CreateLoad(varInfo.alloca->getAllocatedType(), varInfo.alloca, name.c_str());
        auto result = ExpressionCGResult(loaded_val, varInfo.classInfo);
        
        // If it's a primitive type, attach the primitive info for method chaining
        if (varInfo.declaredTypeNode) {
            std::string type_name_str;
            if (auto ident = std::get_if<std::shared_ptr<IdentifierNode>>(&varInfo.declaredTypeNode->name_segment)) {
                type_name_str = (*ident)->name;
            }
            result.primitive_info = ctx.primitive_registry.get_by_simple_name(type_name_str);
        }
        return result;
    }

    // 2. Check for implicit 'this' field access
    auto this_it = ctx.named_values.find("this");
    if (this_it != ctx.named_values.end() && this_it->second.classInfo) {
        const SymbolTable::ClassSymbol* class_info = this_it->second.classInfo;
        auto field_it = class_info->field_indices.find(name);
        if (field_it != class_info->field_indices.end()) {
            // Create implicit this.field access
            auto this_expr = std::make_shared<ThisExpressionNode>();
            this_expr->location = node->location;
            auto member_access = std::make_shared<MemberAccessExpressionNode>();
            member_access->target = this_expr;
            member_access->memberName = node->identifier;
            member_access->location = node->location;
            return cg_member_access_expression(member_access);
        }
    }

    // 3. Check for a class name (for static access)
    const auto* class_symbol = ctx.symbol_table.find_class(name);
    if (class_symbol) {
        ExpressionCGResult res;
        res.classInfo = class_symbol;
        res.is_static_type = true;
        res.resolved_path = name;
        return res;
    }

    // 4. Check for a namespace
    const auto& all_classes = ctx.symbol_table.get_classes();
    for (const auto& [class_name, class_sym] : all_classes) {
        if (class_name.rfind(name + ".", 0) == 0) {
            ExpressionCGResult res;
            res.resolved_path = name;
            return res;
        }
    }

    // All symbol resolution validated by semantic analyzer
    // This should never be reached with valid SemanticIR
    log_compiler_error("Undefined identifier '" + name + "'.", node->location);
}

CodeGenerator::ExpressionCGResult CodeGenerator::cg_binary_expression(std::shared_ptr<BinaryExpressionNode> node) {
    ExpressionCGResult L_res = cg_expression(node->left);
    ExpressionCGResult R_res = cg_expression(node->right);
    llvm::Value* L = L_res.value;
    llvm::Value* R = R_res.value;
    if (!L || !R) {
        log_compiler_error("One or both operands of binary expression are null.", node->location);
    }

    llvm::Type* LType = L->getType();
    llvm::Type* RType = R->getType();

    // Special handling for string operations
    if (node->opKind == BinaryOperatorKind::Add) {
        llvm::Type* string_type = get_llvm_type_from_string(ctx, "string", node->location);
        
        if (LType == string_type && RType == string_type) {
            // string + string
            llvm::Function* concatFunc = ctx.llvm_module.getFunction("Mycelium_String_concat");
            if (!concatFunc) {
                log_compiler_error("Runtime Mycelium_String_concat not found.", node->location);
            }
            llvm::Value* result_str_ptr = ctx.builder.CreateCall(concatFunc, {L, R}, "concat_str");
            return ExpressionCGResult(result_str_ptr, nullptr);
        }
        else if (LType == string_type && RType->isIntegerTy(32)) {
            // string + int
            llvm::Function* fromIntFunc = ctx.llvm_module.getFunction("Mycelium_String_from_int");
            if (!fromIntFunc) {
                log_compiler_error("Mycelium_String_from_int not found", node->right->location);
            }
            llvm::Value* r_as_str = ctx.builder.CreateCall(fromIntFunc, {R}, "int_to_str_tmp");
            llvm::Function* concatFunc = ctx.llvm_module.getFunction("Mycelium_String_concat");
            if (!concatFunc) {
                log_compiler_error("Mycelium_String_concat not found", node->location);
            }
            llvm::Value* result_str_ptr = ctx.builder.CreateCall(concatFunc, {L, r_as_str}, "concat_str_int");
            return ExpressionCGResult(result_str_ptr, nullptr);
        }
        else if (LType == string_type && RType->isIntegerTy(1)) {
            // string + bool
            llvm::Function* fromBoolFunc = ctx.llvm_module.getFunction("Mycelium_String_from_bool");
            if (!fromBoolFunc) {
                log_compiler_error("Mycelium_String_from_bool not found", node->right->location);
            }
            llvm::Value* r_as_str = ctx.builder.CreateCall(fromBoolFunc, {R}, "bool_to_str_tmp");
            llvm::Function* concatFunc = ctx.llvm_module.getFunction("Mycelium_String_concat");
            if (!concatFunc) {
                log_compiler_error("Mycelium_String_concat not found", node->location);
            }
            llvm::Value* result_str_ptr = ctx.builder.CreateCall(concatFunc, {L, r_as_str}, "concat_str_bool");
            return ExpressionCGResult(result_str_ptr, nullptr);
        }
        else if (LType->isIntegerTy(1) && RType == string_type) {
            // bool + string
            llvm::Function* fromBoolFunc = ctx.llvm_module.getFunction("Mycelium_String_from_bool");
            if (!fromBoolFunc) {
                log_compiler_error("Mycelium_String_from_bool not found", node->left->location);
            }
            llvm::Value* l_as_str = ctx.builder.CreateCall(fromBoolFunc, {L}, "bool_to_str_tmp");
            llvm::Function* concatFunc = ctx.llvm_module.getFunction("Mycelium_String_concat");
            if (!concatFunc) {
                log_compiler_error("Mycelium_String_concat not found", node->location);
            }
            llvm::Value* result_str_ptr = ctx.builder.CreateCall(concatFunc, {l_as_str, R}, "concat_bool_str");
            return ExpressionCGResult(result_str_ptr, nullptr);
        }
        else if (LType->isIntegerTy(32) && RType == string_type) {
            // int + string
            llvm::Function* fromIntFunc = ctx.llvm_module.getFunction("Mycelium_String_from_int");
            if (!fromIntFunc) {
                log_compiler_error("Mycelium_String_from_int not found", node->left->location);
            }
            llvm::Value* l_as_str = ctx.builder.CreateCall(fromIntFunc, {L}, "int_to_str_tmp");
            llvm::Function* concatFunc = ctx.llvm_module.getFunction("Mycelium_String_concat");
            if (!concatFunc) {
                log_compiler_error("Mycelium_String_concat not found", node->location);
            }
            llvm::Value* result_str_ptr = ctx.builder.CreateCall(concatFunc, {l_as_str, R}, "concat_int_str");
            return ExpressionCGResult(result_str_ptr, nullptr);
        }
    }

    // Type promotion for numeric operations
    if (LType != RType) {
        if (LType->isFloatingPointTy() && RType->isIntegerTy()) {
            R = ctx.builder.CreateSIToFP(R, LType, "inttofp_tmp");
            RType = LType;
        }
        else if (RType->isFloatingPointTy() && LType->isIntegerTy()) {
            L = ctx.builder.CreateSIToFP(L, RType, "inttofp_tmp");
            LType = RType;
        }
    }

    llvm::Value* result_val = nullptr;
    switch (node->opKind) {
        case BinaryOperatorKind::Add:
            if (LType->isIntegerTy())
                result_val = ctx.builder.CreateAdd(L, R, "addtmp");
            else if (LType->isFloatingPointTy())
                result_val = ctx.builder.CreateFAdd(L, R, "faddtmp");
            else
                log_compiler_error("Unsupported type for Add", node->location);
            break;
        case BinaryOperatorKind::Subtract:
            if (LType->isIntegerTy())
                result_val = ctx.builder.CreateSub(L, R, "subtmp");
            else if (LType->isFloatingPointTy())
                result_val = ctx.builder.CreateFSub(L, R, "fsubtmp");
            else
                log_compiler_error("Unsupported type for Subtract", node->location);
            break;
        case BinaryOperatorKind::Multiply:
            if (LType->isIntegerTy())
                result_val = ctx.builder.CreateMul(L, R, "multmp");
            else if (LType->isFloatingPointTy())
                result_val = ctx.builder.CreateFMul(L, R, "fmultmp");
            else
                log_compiler_error("Unsupported type for Multiply", node->location);
            break;
        case BinaryOperatorKind::Divide:
            if (LType->isIntegerTy())
                result_val = ctx.builder.CreateSDiv(L, R, "sdivtmp");
            else if (LType->isFloatingPointTy())
                result_val = ctx.builder.CreateFDiv(L, R, "fdivtmp");
            else
                log_compiler_error("Unsupported type for Divide", node->location);
            break;
        case BinaryOperatorKind::Modulo:
            if (LType->isIntegerTy())
                result_val = ctx.builder.CreateSRem(L, R, "sremtmp");
            else
                log_compiler_error("Unsupported type for Modulo", node->location);
            break;
        case BinaryOperatorKind::Equals:
            if (LType->isIntegerTy() || LType->isPointerTy())
                result_val = ctx.builder.CreateICmpEQ(L, R, "eqtmp");
            else if (LType->isFloatingPointTy())
                result_val = ctx.builder.CreateFCmpOEQ(L, R, "feqtmp");
            else
                log_compiler_error("Unsupported type for Equals", node->location);
            break;
        case BinaryOperatorKind::NotEquals:
            if (LType->isIntegerTy() || LType->isPointerTy())
                result_val = ctx.builder.CreateICmpNE(L, R, "netmp");
            else if (LType->isFloatingPointTy())
                result_val = ctx.builder.CreateFCmpONE(L, R, "fnetmp");
            else
                log_compiler_error("Unsupported type for NotEquals", node->location);
            break;
        case BinaryOperatorKind::LessThan:
            if (LType->isIntegerTy())
                result_val = ctx.builder.CreateICmpSLT(L, R, "slttmp");
            else if (LType->isFloatingPointTy())
                result_val = ctx.builder.CreateFCmpOLT(L, R, "folttmp");
            else
                log_compiler_error("Unsupported type for LessThan", node->location);
            break;
        case BinaryOperatorKind::GreaterThan:
            if (LType->isIntegerTy())
                result_val = ctx.builder.CreateICmpSGT(L, R, "sgttmp");
            else if (LType->isFloatingPointTy())
                result_val = ctx.builder.CreateFCmpOGT(L, R, "fogttmp");
            else
                log_compiler_error("Unsupported type for GreaterThan", node->location);
            break;
        case BinaryOperatorKind::LessThanOrEqual:
            if (LType->isIntegerTy())
                result_val = ctx.builder.CreateICmpSLE(L, R, "sletmp");
            else if (LType->isFloatingPointTy())
                result_val = ctx.builder.CreateFCmpOLE(L, R, "foletmp");
            else
                log_compiler_error("Unsupported type for LessThanOrEqual", node->location);
            break;
        case BinaryOperatorKind::GreaterThanOrEqual:
            if (LType->isIntegerTy())
                result_val = ctx.builder.CreateICmpSGE(L, R, "sgetmp");
            else if (LType->isFloatingPointTy())
                result_val = ctx.builder.CreateFCmpOGE(L, R, "fogetmp");
            else
                log_compiler_error("Unsupported type for GreaterThanOrEqual", node->location);
            break;
        case BinaryOperatorKind::LogicalAnd:
            if (LType->isIntegerTy(1) && RType->isIntegerTy(1))
                result_val = ctx.builder.CreateAnd(L, R, "andtmp");
            else
                log_compiler_error("LogicalAnd requires boolean operands.", node->location);
            break;
        case BinaryOperatorKind::LogicalOr:
            if (LType->isIntegerTy(1) && RType->isIntegerTy(1))
                result_val = ctx.builder.CreateOr(L, R, "ortmp");
            else
                log_compiler_error("LogicalOr requires boolean operands.", node->location);
            break;
        default:
            log_compiler_error("Unsupported binary operator.", node->location);
    }
    return ExpressionCGResult(result_val, nullptr);
}

CodeGenerator::ExpressionCGResult CodeGenerator::cg_assignment_expression(std::shared_ptr<AssignmentExpressionNode> node) {
    ExpressionCGResult source_res = cg_expression(node->source);
    llvm::Value* new_llvm_val = source_res.value;
    const SymbolTable::ClassSymbol* new_val_static_ci = source_res.classInfo;
    if (!new_llvm_val) {
        log_compiler_error("Assignment source is null.", node->source->location);
    }

    if (auto id_target = std::dynamic_pointer_cast<IdentifierExpressionNode>(node->target)) {
        auto it = ctx.named_values.find(id_target->identifier->name);
        if (it == ctx.named_values.end()) {
            // Try implicit field assignment: if we're in an instance method/constructor and target not found,
            // try to resolve it as this.fieldName assignment
            auto this_it = ctx.named_values.find("this");
            if (this_it != ctx.named_values.end() && this_it->second.classInfo) {
                const SymbolTable::ClassSymbol* class_info = this_it->second.classInfo;

                // Check if the identifier matches a field name
                auto field_it = class_info->field_indices.find(id_target->identifier->name);
                if (field_it != class_info->field_indices.end()) {
                    // Create a member access assignment: this.fieldName = source
                    auto this_expr = std::make_shared<IdentifierExpressionNode>();
                    this_expr->identifier = std::make_shared<IdentifierNode>("this");
                    this_expr->location = id_target->location;

                    auto member_name = std::make_shared<IdentifierNode>(id_target->identifier->name);

                    auto member_access = std::make_shared<MemberAccessExpressionNode>();
                    member_access->target = this_expr;
                    member_access->memberName = member_name;
                    member_access->location = id_target->location;

                    // Create a new assignment with member access as target
                    auto member_assignment = std::make_shared<AssignmentExpressionNode>();
                    member_assignment->target = member_access;
                    member_assignment->source = node->source;
                    member_assignment->location = node->location;

                    // Recursively resolve the member access assignment
                    return cg_assignment_expression(member_assignment);
                }
            }

            log_compiler_error("Assigning to undeclared var: " + id_target->identifier->name, id_target->location);
        }

        VariableInfo& target_var_info = it->second;
        llvm::Type* target_llvm_type = target_var_info.alloca->getAllocatedType();
        const SymbolTable::ClassSymbol* target_static_ci = target_var_info.classInfo;

        // CRITICAL FIX: Only retain if source is NOT a new expression
        llvm::Value* new_object_header_for_retain = nullptr;
        if (new_val_static_ci && new_val_static_ci->fieldsType) {
            // Check if the source is a new expression (ObjectCreationExpression)
            bool is_new_expression = std::dynamic_pointer_cast<ObjectCreationExpressionNode>(node->source) != nullptr;

            if (!is_new_expression) {
                // Only retain if this is NOT a new expression
                if (source_res.header_ptr) {
                    new_object_header_for_retain = source_res.header_ptr;
                } else {
                    new_object_header_for_retain = get_header_ptr_from_fields_ptr(ctx, new_llvm_val, new_val_static_ci->fieldsType);
                }
                if (new_object_header_for_retain) {
                    create_arc_retain(ctx, new_object_header_for_retain);
                }
            }
        }

        llvm::Value* old_llvm_val = ctx.builder.CreateLoad(target_llvm_type, target_var_info.alloca, "old.val.assign");
        
        // Release old value if it's an object
        if (target_static_ci && target_static_ci->fieldsType) {
            llvm::Value* old_hdr = get_header_ptr_from_fields_ptr(ctx, old_llvm_val, target_static_ci->fieldsType);
            if (old_hdr) {
                llvm::Value* is_old_hdr_null_cond = ctx.builder.CreateICmpNE(old_hdr, llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(old_hdr->getType())));
                llvm::BasicBlock* release_call_bb = llvm::BasicBlock::Create(ctx.llvm_context, "release.call.assign", ctx.current_function);
                llvm::BasicBlock* after_release_bb = llvm::BasicBlock::Create(ctx.llvm_context, "after.release.assign", ctx.current_function);
                ctx.builder.CreateCondBr(is_old_hdr_null_cond, release_call_bb, after_release_bb);
                
                ctx.builder.SetInsertPoint(release_call_bb);
                create_arc_release(ctx, old_hdr);
                ctx.builder.CreateBr(after_release_bb);
                
                ctx.builder.SetInsertPoint(after_release_bb);
            }
        }

        ctx.builder.CreateStore(new_llvm_val, target_var_info.alloca);

        // CRITICAL FIX: For instance method field assignments, also write back to the actual object field
        if (ctx.current_function && ctx.named_values.count("this")) {
            // Check if this variable corresponds to a field in the current class
            const VariableInfo& thisInfo = ctx.named_values["this"];
            if (thisInfo.classInfo && thisInfo.classInfo->field_indices.count(id_target->identifier->name)) {
                // This is a field assignment in an instance method - write back to the actual field
                llvm::Value* this_fields_ptr = ctx.builder.CreateLoad(thisInfo.alloca->getAllocatedType(), thisInfo.alloca, "this.for.field.assign");
                unsigned field_idx = thisInfo.classInfo->field_indices.at(id_target->identifier->name);
                llvm::Value* actual_field_ptr = ctx.builder.CreateStructGEP(thisInfo.classInfo->fieldsType, this_fields_ptr, field_idx, id_target->identifier->name + ".actual.field.ptr");
                ctx.builder.CreateStore(new_llvm_val, actual_field_ptr);
            }
        }

        // Register with scope manager for unified ARC management
        std::string declared_type_name;
        if (target_var_info.declaredTypeNode) {
            if (auto identNode = std::get_if<std::shared_ptr<IdentifierNode>>(&target_var_info.declaredTypeNode->name_segment)) {
                declared_type_name = (*identNode)->name;
            }
        }

        if (target_static_ci && target_static_ci->fieldsType &&
            new_llvm_val->getType()->isPointerTy() && declared_type_name != "string") {
            ctx.scope_manager.register_arc_managed_object(
                target_var_info.alloca,
                target_static_ci,
                id_target->identifier->name);
        }
    }
    else if (auto member_target = std::dynamic_pointer_cast<MemberAccessExpressionNode>(node->target)) {
        ExpressionCGResult obj_res = cg_expression(member_target->target);
        if (!obj_res.value || !obj_res.classInfo || !obj_res.classInfo->fieldsType) {
            log_compiler_error("Invalid member assignment target.", member_target->target->location);
        }
        auto field_it = obj_res.classInfo->field_indices.find(member_target->memberName->name);
        if (field_it == obj_res.classInfo->field_indices.end()) {
            log_compiler_error("Field not found in assignment", member_target->location);
        }
        unsigned field_idx = field_it->second;
        llvm::Value* field_ptr = ctx.builder.CreateStructGEP(obj_res.classInfo->fieldsType, obj_res.value, field_idx);

        // ARC: Release old field value before storing new value
        llvm::Type* field_type = obj_res.classInfo->fieldsType->getElementType(field_idx);
        std::shared_ptr<TypeNameNode> field_ast_type = nullptr;
        if (field_idx < obj_res.classInfo->field_ast_types.size()) {
            field_ast_type = obj_res.classInfo->field_ast_types[field_idx];
        }

        // Check if this is an object field that needs ARC management
        const SymbolTable::ClassSymbol* field_class_info = nullptr;
        if (field_type->isPointerTy() && field_ast_type) {
            if (auto identNode = std::get_if<std::shared_ptr<IdentifierNode>>(&field_ast_type->name_segment)) {
                const auto* field_class_symbol = ctx.symbol_table.find_class((*identNode)->name);
                if (field_class_symbol) {
                    field_class_info = field_class_symbol;
                }
            }
        }

        if (field_class_info && field_class_info->fieldsType) {
            // Load old field value and release it if not null
            llvm::Value* old_field_val = ctx.builder.CreateLoad(field_type, field_ptr, "old.field.val");

            // First check if the old field value itself is not null
            llvm::Value* is_field_not_null = ctx.builder.CreateICmpNE(old_field_val, llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(old_field_val->getType())));
            llvm::BasicBlock* check_release_bb = llvm::BasicBlock::Create(ctx.llvm_context, "check.release.field", ctx.current_function);
            llvm::BasicBlock* after_release_bb = llvm::BasicBlock::Create(ctx.llvm_context, "after.release.field", ctx.current_function);
            ctx.builder.CreateCondBr(is_field_not_null, check_release_bb, after_release_bb);

            ctx.builder.SetInsertPoint(check_release_bb);
            llvm::Value* old_hdr = get_header_ptr_from_fields_ptr(ctx, old_field_val, field_class_info->fieldsType);
            if (old_hdr) {
                llvm::Value* is_hdr_not_null = ctx.builder.CreateICmpNE(old_hdr, llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(old_hdr->getType())));
                llvm::BasicBlock* release_bb = llvm::BasicBlock::Create(ctx.llvm_context, "release.old.field", ctx.current_function);
                llvm::BasicBlock* skip_release_bb = llvm::BasicBlock::Create(ctx.llvm_context, "skip.release.field", ctx.current_function);
                ctx.builder.CreateCondBr(is_hdr_not_null, release_bb, skip_release_bb);

                ctx.builder.SetInsertPoint(release_bb);
                create_arc_release(ctx, old_hdr);
                ctx.builder.CreateBr(skip_release_bb);

                ctx.builder.SetInsertPoint(skip_release_bb);
                ctx.builder.CreateBr(after_release_bb);
            } else {
                ctx.builder.CreateBr(after_release_bb);
            }
            ctx.builder.SetInsertPoint(after_release_bb);
        }

        ctx.builder.CreateStore(new_llvm_val, field_ptr);
    }
    else {
        log_compiler_error("Invalid assignment target.", node->target->location);
    }

    return ExpressionCGResult(new_llvm_val, new_val_static_ci);
}

CodeGenerator::ExpressionCGResult CodeGenerator::cg_unary_expression(std::shared_ptr<UnaryExpressionNode> node) {
    ExpressionCGResult operand_res = cg_expression(node->operand);
    if (!operand_res.value) {
        log_compiler_error("Operand for unary expression is null.", node->operand->location);
    }

    llvm::Value* operand_val = operand_res.value;
    llvm::Value* result_val = nullptr;
    
    switch (node->opKind) {
        case UnaryOperatorKind::LogicalNot:
            result_val = ctx.builder.CreateNot(operand_val, "nottmp");
            break;
        case UnaryOperatorKind::UnaryMinus:
            if (operand_val->getType()->isIntegerTy())
                result_val = ctx.builder.CreateNeg(operand_val, "negtmp");
            else if (operand_val->getType()->isFloatingPointTy())
                result_val = ctx.builder.CreateFNeg(operand_val, "fnegtmp");
            else
                log_compiler_error("Unsupported type for unary minus.", node->location);
            break;
        // TODO: Pre/Post Increment/Decrement need LValue handling
        case UnaryOperatorKind::PreIncrement:
        case UnaryOperatorKind::PostIncrement:
        case UnaryOperatorKind::PreDecrement:
        case UnaryOperatorKind::PostDecrement:
            log_compiler_error("Pre/Post Increment/Decrement not fully implemented.", node->location);
            result_val = operand_val; // Placeholder
            break;
        default:
            log_compiler_error("Unsupported unary operator.", node->location);
            break;
    }
    return ExpressionCGResult(result_val, nullptr);
}

CodeGenerator::ExpressionCGResult CodeGenerator::cg_parenthesized_expression(std::shared_ptr<ParenthesizedExpressionNode> node) {
    if (!node || !node->expression) {
        log_compiler_error("ParenthesizedExpressionNode or its inner expression is null.", node ? node->location : std::nullopt);
    }
    return cg_expression(node->expression);
}

CodeGenerator::ExpressionCGResult CodeGenerator::cg_this_expression(std::shared_ptr<ThisExpressionNode> node) {
    auto it = ctx.named_values.find("this");
    if (it == ctx.named_values.end()) {
        log_compiler_error("'this' used inappropriately.", node->location);
    }
    const VariableInfo& thisVarInfo = it->second;
    llvm::Value* loaded_this_ptr = ctx.builder.CreateLoad(thisVarInfo.alloca->getAllocatedType(), thisVarInfo.alloca, "this.val");
    return ExpressionCGResult(loaded_this_ptr, thisVarInfo.classInfo);
}

CodeGenerator::ExpressionCGResult CodeGenerator::cg_member_access_expression(std::shared_ptr<MemberAccessExpressionNode> node) {
    ExpressionCGResult target_res = cg_expression(node->target);
    const std::string& member_name = node->memberName->name;

    // Case 1: Target is a namespace (e.g., MyCompany.Services)
    if (!target_res.resolved_path.empty() && !target_res.classInfo) {
        std::string new_path = target_res.resolved_path + "." + member_name;

        // Check if the new path resolves to a class
        const auto* class_symbol = ctx.symbol_table.find_class(new_path);
        if (class_symbol) {
            ExpressionCGResult res;
            res.classInfo = class_symbol;
            res.is_static_type = true;
            res.resolved_path = new_path;
            return res;
        }

        // Check if the new path is still a namespace prefix
        const auto& all_classes = ctx.symbol_table.get_classes();
        for (const auto& [class_name, class_sym] : all_classes) {
            if (class_name.rfind(new_path + ".", 0) == 0) {
                ExpressionCGResult res;
                res.resolved_path = new_path;
                return res;
            }
        }
        
        log_compiler_error("Symbol '" + member_name + "' not found in namespace '" + target_res.resolved_path + "'.", node->memberName->location);
    }

    // Case 2: Target is a primitive type (for method calls)
    if (target_res.primitive_info) {
        return target_res; // Pass along; method call will handle it
    }

    // Case 3: Target is a class instance or a static type
    if (target_res.classInfo) {
        // SAFETY CHECK: Ensure classInfo is valid
        if (!target_res.classInfo->fieldsType) {
            log_compiler_error("Class '" + target_res.classInfo->name + "' has invalid field structure.", node->target->location);
        }

        // Check for a field first - inherited fields now accessible by original name
        std::string actual_field_name = member_name;
        const SymbolTable::ClassSymbol* field_owner_class = target_res.classInfo;
        auto field_it = target_res.classInfo->field_indices.find(member_name);
        
        if (field_it != target_res.classInfo->field_indices.end()) {
            LOG_DEBUG("Found field: " + member_name + " at index " + std::to_string(field_it->second), "COMPILER");
        }
        
        if (field_it != target_res.classInfo->field_indices.end()) {
            if (target_res.is_static_type) {
                // TODO: Handle static fields when they are supported
                log_compiler_error("Static fields are not yet supported. Cannot access '" + member_name + "'.", node->location);
            }

            if (!target_res.value) {
                log_compiler_error("Cannot access field '" + member_name + "' on a null instance.", node->target->location);
            }

            // It's an instance field access
            unsigned field_idx = field_it->second;
            
            // SAFETY CHECK: Ensure field index is valid
            if (field_idx >= target_res.classInfo->fieldsType->getNumElements()) {
                log_compiler_error("Field index out of bounds for '" + member_name + "' in class '" + target_res.classInfo->name + "'.", node->location);
            }
            
            llvm::Type* field_llvm_type = target_res.classInfo->fieldsType->getElementType(field_idx);
            llvm::Value* field_ptr = ctx.builder.CreateStructGEP(target_res.classInfo->fieldsType, target_res.value, field_idx, actual_field_name + ".ptr");
            llvm::Value* loaded_field = ctx.builder.CreateLoad(field_llvm_type, field_ptr, member_name);
            
            const SymbolTable::ClassSymbol* field_static_ci = nullptr;
            if (field_llvm_type->isPointerTy() && field_idx < target_res.classInfo->field_ast_types.size()) {
                std::shared_ptr<TypeNameNode> field_ast_type = target_res.classInfo->field_ast_types[field_idx];
                if (field_ast_type) {
                    if (auto identNode = std::get_if<std::shared_ptr<IdentifierNode>>(&field_ast_type->name_segment)) {
                        if (*identNode) {
                            const auto* field_class_symbol = ctx.symbol_table.find_class((*identNode)->name);
                            if (field_class_symbol)
                                field_static_ci = field_class_symbol;
                        }
                    }
                }
            }
            return ExpressionCGResult(loaded_field, field_static_ci);
        }

        // If not a field, it might be a method. The MethodCall visitor will verify.
        // We just pass the target info up the chain.
        return target_res;
    }
    
    log_compiler_error("Invalid target for member access. Not a class, instance, or namespace.", node->target->location);
}

CodeGenerator::ExpressionCGResult CodeGenerator::cg_object_creation_expression(std::shared_ptr<ObjectCreationExpressionNode> node) {
    if (!node->type) {
        log_compiler_error("Object creation missing type.", node->location);
    }
    
    std::string class_name_str;
    if (auto identNode = std::get_if<std::shared_ptr<IdentifierNode>>(&node->type->name_segment))
        class_name_str = (*identNode)->name;
    else {
        log_compiler_error("Unsupported type in new.", node->type->location);
    }
    
    const auto* class_symbol = ctx.symbol_table.find_class(class_name_str);
    if (!class_symbol) {
        log_compiler_error("Undefined class in new: " + class_name_str, node->type->location);
    }
    if (!class_symbol->fieldsType) {
        log_compiler_error("Class " + class_name_str + " has no fieldsType.", node->type->location);
    }
    
    llvm::DataLayout dl = ctx.llvm_module.getDataLayout();
    llvm::Value* data_size_val = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx.llvm_context), dl.getTypeAllocSize(class_symbol->fieldsType));
    llvm::Value* type_id_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx.llvm_context), class_symbol->type_id);
    
    llvm::Function* alloc_func = ctx.llvm_module.getFunction("Mycelium_Object_alloc");
    if (!alloc_func) {
        log_compiler_error("Runtime Mycelium_Object_alloc not found.", node->location);
    }
    
    // Pass the actual VTable for the class (polymorphism support)
    llvm::Value* vtable_ptr_val;
    if (class_symbol->vtable_global) {
        // Cast VTable global to generic pointer for runtime use
        vtable_ptr_val = ctx.builder.CreateBitCast(class_symbol->vtable_global, llvm::PointerType::get(ctx.llvm_context, 0), "vtable_ptr");
    } else {
        // No VTable for this class (no virtual methods)
        vtable_ptr_val = llvm::ConstantPointerNull::get(llvm::PointerType::get(ctx.llvm_context, 0));
    }
    
    llvm::Value* header_ptr_val = ctx.builder.CreateCall(alloc_func, {data_size_val, type_id_val, vtable_ptr_val}, "new.header");
    llvm::Value* fields_obj_opaque_ptr = get_fields_ptr_from_header_ptr(ctx, header_ptr_val, class_symbol->fieldsType);
    
    std::string ctor_name_str = class_name_str + ".%ctor";
    std::vector<llvm::Value*> ctor_args_values = {fields_obj_opaque_ptr};
    if (node->argumentList.has_value()) {
        for (const auto& arg_node : node->argumentList.value()->arguments) {
            ctor_args_values.push_back(cg_expression(arg_node->expression).value);
        }
    }
    
    llvm::Function* constructor_func = ctx.llvm_module.getFunction(ctor_name_str);
    if (!constructor_func) {
        log_compiler_error("Constructor " + ctor_name_str + " not found.", node->location);
    }
    ctx.builder.CreateCall(constructor_func, ctor_args_values);
    
    return ExpressionCGResult(fields_obj_opaque_ptr, class_symbol, header_ptr_val);
}

CodeGenerator::ExpressionCGResult CodeGenerator::cg_cast_expression(std::shared_ptr<CastExpressionNode> node) {
    ExpressionCGResult expr_to_cast_res = cg_expression(node->expression);
    llvm::Value* expr_val = expr_to_cast_res.value;
    if (!expr_val) {
        log_compiler_error("Expression to be cast is null.", node->expression->location);
    }
    
    llvm::Type* target_llvm_type = get_llvm_type(ctx, node->targetType);
    if (!target_llvm_type) {
        log_compiler_error("Target type for cast is null.", node->targetType->location);
    }
    
    const SymbolTable::ClassSymbol* target_static_ci = nullptr;
    if (target_llvm_type->isPointerTy()) {
        if (auto typeNameNode = node->targetType) {
            if (auto identNode = std::get_if<std::shared_ptr<IdentifierNode>>(&typeNameNode->name_segment)) {
                const auto* class_symbol = ctx.symbol_table.find_class((*identNode)->name);
                if (class_symbol) {
                    target_static_ci = class_symbol;
                }
            }
        }
    }
    
    llvm::Value* cast_val = nullptr;
    llvm::Type* src_llvm_type = expr_val->getType();
    
    // Special case: Primitive-to-string conversions using ToString() methods
    if (target_llvm_type->isPointerTy()) {
        if (auto typeNameNode = node->targetType) {
            if (auto identNode = std::get_if<std::shared_ptr<IdentifierNode>>(&typeNameNode->name_segment)) {
                std::string target_type_name = (*identNode)->name;
                if (target_type_name == "string") {
                    // Check if we're converting from a primitive type to string
                    if (src_llvm_type->isIntegerTy()) {
                        unsigned bit_width = src_llvm_type->getIntegerBitWidth();
                        if (bit_width == 32) {
                            // int to string
                            llvm::Function* int_to_string_func = ctx.llvm_module.getFunction("Mycelium_String_from_int");
                            if (int_to_string_func) {
                                cast_val = ctx.builder.CreateCall(int_to_string_func, {expr_val}, "int_to_string");
                            }
                        }
                        else if (bit_width == 64) {
                            // long to string
                            llvm::Function* long_to_string_func = ctx.llvm_module.getFunction("Mycelium_String_from_long");
                            if (long_to_string_func) {
                                cast_val = ctx.builder.CreateCall(long_to_string_func, {expr_val}, "long_to_string");
                            }
                        }
                        else if (bit_width == 1) {
                            // bool to string
                            llvm::Function* bool_to_string_func = ctx.llvm_module.getFunction("Mycelium_String_from_bool");
                            if (bool_to_string_func) {
                                cast_val = ctx.builder.CreateCall(bool_to_string_func, {expr_val}, "bool_to_string");
                            }
                        }
                        else if (bit_width == 8) {
                            // char to string
                            llvm::Function* char_to_string_func = ctx.llvm_module.getFunction("Mycelium_String_from_char");
                            if (char_to_string_func) {
                                cast_val = ctx.builder.CreateCall(char_to_string_func, {expr_val}, "char_to_string");
                            }
                        }
                    }
                    else if (src_llvm_type->isFloatTy()) {
                        // float to string
                        llvm::Function* float_to_string_func = ctx.llvm_module.getFunction("Mycelium_String_from_float");
                        if (float_to_string_func) {
                            cast_val = ctx.builder.CreateCall(float_to_string_func, {expr_val}, "float_to_string");
                        }
                    }
                    else if (src_llvm_type->isDoubleTy()) {
                        // double to string
                        llvm::Function* double_to_string_func = ctx.llvm_module.getFunction("Mycelium_String_from_double");
                        if (double_to_string_func) {
                            cast_val = ctx.builder.CreateCall(double_to_string_func, {expr_val}, "double_to_string");
                        }
                    }
                    
                    if (cast_val) {
                        return ExpressionCGResult(cast_val, target_static_ci);
                    }
                }
            }
        }
    }
    
    if (target_llvm_type == src_llvm_type) {
        cast_val = expr_val;
    }
    else if (target_llvm_type->isIntegerTy() && src_llvm_type->isFloatingPointTy()) {
        cast_val = ctx.builder.CreateFPToSI(expr_val, target_llvm_type, "fptosi_cast");
    }
    else if (target_llvm_type->isFloatingPointTy() && src_llvm_type->isIntegerTy()) {
        cast_val = ctx.builder.CreateSIToFP(expr_val, target_llvm_type, "sitofp_cast");
    }
    else if (target_llvm_type->isIntegerTy() && src_llvm_type->isIntegerTy()) {
        unsigned target_width = target_llvm_type->getIntegerBitWidth();
        unsigned src_width = src_llvm_type->getIntegerBitWidth();
        if (target_width > src_width) {
            cast_val = ctx.builder.CreateSExt(expr_val, target_llvm_type, "sext_cast");
        }
        else if (target_width < src_width) {
            cast_val = ctx.builder.CreateTrunc(expr_val, target_llvm_type, "trunc_cast");
        }
        else {
            cast_val = expr_val;
        }
    }
    else if (target_llvm_type->isPointerTy() && src_llvm_type->isPointerTy()) {
        cast_val = ctx.builder.CreateBitCast(expr_val, target_llvm_type, "ptr_bitcast");
    }
    else if (target_llvm_type->isIntegerTy() && src_llvm_type->isPointerTy()) {
        cast_val = ctx.builder.CreatePtrToInt(expr_val, target_llvm_type, "ptrtoint_cast");
    }
    else if (target_llvm_type->isPointerTy() && src_llvm_type->isIntegerTy()) {
        cast_val = ctx.builder.CreateIntToPtr(expr_val, target_llvm_type, "inttoptr_cast");
    }
    else {
        log_compiler_error("Unsupported cast", node->location);
    }
    
    return ExpressionCGResult(cast_val, target_static_ci);
}

// The complex method call expression with virtual dispatch and inheritance
CodeGenerator::ExpressionCGResult CodeGenerator::cg_method_call_expression(std::shared_ptr<MethodCallExpressionNode> node) {
    std::string method_name;
    const SymbolTable::ClassSymbol* callee_class_info = nullptr;
    llvm::Value* instance_ptr_for_call = nullptr;
    bool is_primitive_call = false;
    PrimitiveStructInfo* primitive_info = nullptr;

    // Extract method context from target
    if (auto member_access = std::dynamic_pointer_cast<MemberAccessExpressionNode>(node->target)) {
        method_name = member_access->memberName->name;
        ExpressionCGResult target_res = cg_expression(member_access->target);

        callee_class_info = target_res.classInfo;
        instance_ptr_for_call = target_res.value; // Will be null for static calls
        primitive_info = target_res.primitive_info;
        is_primitive_call = (primitive_info != nullptr);
    }
    else if (auto identifier = std::dynamic_pointer_cast<IdentifierExpressionNode>(node->target)) {
        method_name = identifier->identifier->name;

        // Check if it's an extern function
        if (ctx.symbol_table.find_method(method_name) && ctx.symbol_table.find_method(method_name)->is_external) {
            // Extern function call. It has no class context.
        }
        else if (ctx.current_function) {
            // It's an implicit call within a method
            std::string current_func_name = ctx.current_function->getName().str();
            size_t dot_pos = current_func_name.find('.');
            if (dot_pos == std::string::npos) {
                // Semantic analyzer should have caught this
                return ExpressionCGResult(nullptr);
            }
            std::string current_class_name = current_func_name.substr(0, dot_pos);

            // Find the target method symbol within the current class
            const auto* target_method_symbol = ctx.symbol_table.find_method_in_class(current_class_name, method_name);
            if (!target_method_symbol) {
                // Semantic analyzer should have validated method exists
                return ExpressionCGResult(nullptr);
            }
            
            // Get the ClassSymbol for the current class
            const auto* class_symbol = ctx.symbol_table.find_class(current_class_name);
            if (!class_symbol) {
                // Class should exist if semantic analyzer passed
                return ExpressionCGResult(nullptr);
            }
            callee_class_info = class_symbol;

            // Check if the call is static or instance based on the target method's properties
            if (target_method_symbol->is_static) {
                // It's a static call. No instance pointer is needed.
                instance_ptr_for_call = nullptr;
            }
            else { // It's an instance call
                // We need 'this'
                auto this_it = ctx.named_values.find("this");
                if (this_it == ctx.named_values.end()) {
                    // Semantic analyzer should have caught static context errors
                    return ExpressionCGResult(nullptr);
                }
                instance_ptr_for_call = ctx.builder.CreateLoad(this_it->second.alloca->getAllocatedType(), this_it->second.alloca, "this.for.implicit.call");
            }
        }
        else {
            // Semantic analyzer should have caught global context errors
            return ExpressionCGResult(nullptr);
        }
    }
    else {
        // Semantic analyzer should have validated method call targets
        return ExpressionCGResult(nullptr);
    }

    // Handle primitive method calls
    if (is_primitive_call && primitive_info) {
        return cg_primitive_method_call(node, primitive_info, instance_ptr_for_call);
    }

    // Build the function name and find it
    std::string resolved_func_name;
    if (callee_class_info) {
        // Use semantic analyzer to find method in inheritance chain
        const auto* method_symbol = ctx.symbol_table.find_method_in_class(callee_class_info->name, method_name);
        if (method_symbol) {
            resolved_func_name = method_symbol->qualified_name;
            LOG_DEBUG("Found method via inheritance: " + method_name + " -> " + resolved_func_name, "COMPILER");
        }
        else {
            // Fallback to old behavior
            resolved_func_name = callee_class_info->name + "." + method_name;
            LOG_DEBUG("Method not found in inheritance chain, using direct name: " + resolved_func_name, "COMPILER");
        }
    } 
    else {
        resolved_func_name = method_name; // For extern functions
    }

    // Check if this is a virtual method call that needs VTable dispatch
    bool use_virtual_dispatch = false;
    size_t virtual_method_index = 0;
    
    if (callee_class_info && instance_ptr_for_call) {
        const auto* method_symbol = ctx.symbol_table.find_method(resolved_func_name);
        if (method_symbol && method_symbol->is_virtual) {
            // Find the virtual method index in the class's VTable order
            const auto* class_symbol = ctx.symbol_table.find_class(callee_class_info->name);
            if (class_symbol) {
                // Search for the method by name in the VTable, checking for inherited methods
                for (size_t i = 0; i < class_symbol->virtual_method_order.size(); ++i) {
                    // Extract method name from qualified name for comparison
                    const std::string& vtable_method = class_symbol->virtual_method_order[i];
                    size_t dot_pos = vtable_method.find_last_of('.');
                    if (dot_pos != std::string::npos) {
                        std::string vtable_method_name = vtable_method.substr(dot_pos + 1);
                        if (vtable_method_name == method_name) {
                            use_virtual_dispatch = true;
                            virtual_method_index = i + 1; // +1 for destructor slot at index 0
                            LOG_DEBUG("Found virtual method at VTable index " + std::to_string(i) + ": " + vtable_method, "COMPILER");
                            break;
                        }
                    }
                }
            }
        }
    }

    std::vector<llvm::Value*> args_values;
    llvm::Value* call_result_val = nullptr;

    if (use_virtual_dispatch) {
        // Virtual method call via VTable lookup
        LOG_DEBUG("Using virtual dispatch for method: " + resolved_func_name, "COMPILER");
        
        // Get object header pointer from instance pointer (fields pointer)
        llvm::Value* header_ptr = get_header_ptr_from_fields_ptr(ctx, instance_ptr_for_call, callee_class_info->fieldsType);
        
        // Load VTable pointer from object header (offset 8 for vtable field)
        llvm::Value* vtable_ptr_ptr = ctx.builder.CreateConstInBoundsGEP1_64(
            llvm::Type::getInt8Ty(ctx.llvm_context), header_ptr, 8, "vtable_ptr_ptr");
        llvm::Value* vtable_ptr = ctx.builder.CreateLoad(
            llvm::PointerType::get(ctx.llvm_context, 0), vtable_ptr_ptr, "vtable_ptr");
        
        // Get function pointer from VTable at the correct index
        llvm::Value* method_ptr_ptr = ctx.builder.CreateConstInBoundsGEP1_64(
            llvm::PointerType::get(ctx.llvm_context, 0), vtable_ptr, virtual_method_index, "method_ptr_ptr");
        llvm::Value* method_ptr = ctx.builder.CreateLoad(
            llvm::PointerType::get(ctx.llvm_context, 0), method_ptr_ptr, "method_ptr");
        
        // Prepare arguments for virtual call (header pointer + method args)
        args_values.push_back(instance_ptr_for_call);
        
        if (node->argumentList) {
            for (const auto& arg_node : node->argumentList->arguments) {
                ExpressionCGResult arg_res = cg_expression(arg_node->expression);
                if (!arg_res.value) {
                    // Semantic analyzer should have validated arguments
                    return ExpressionCGResult(nullptr);
                }
                args_values.push_back(arg_res.value);
            }
        }
        
        // Create indirect call through function pointer
        llvm::Function* direct_callee = ctx.llvm_module.getFunction(resolved_func_name);
        if (!direct_callee) {
            // Semantic analyzer should have validated function exists
            return ExpressionCGResult(nullptr);
        }
        
        llvm::FunctionType* func_type = direct_callee->getFunctionType();
        call_result_val = ctx.builder.CreateCall(func_type, method_ptr, args_values, 
            func_type->getReturnType()->isVoidTy() ? "" : "virtual_call");
    }
    else {
        // Direct method call (non-virtual or static)
        llvm::Function* callee = ctx.llvm_module.getFunction(resolved_func_name);
        if (!callee) {
            // Semantic analyzer should have validated function exists
            return ExpressionCGResult(nullptr);
        }

        if (instance_ptr_for_call) {
            args_values.push_back(instance_ptr_for_call);
        }
    
        if (node->argumentList) {
            for (const auto& arg_node : node->argumentList->arguments) {
                ExpressionCGResult arg_res = cg_expression(arg_node->expression);
                if (!arg_res.value) {
                    // Semantic analyzer should have validated arguments
                    return ExpressionCGResult(nullptr);
                }
                args_values.push_back(arg_res.value);
            }
        }

        // Verify argument count
        if (callee->arg_size() != args_values.size()) {
            log_compiler_error("Incorrect number of arguments for function " + resolved_func_name + ". Expected " + std::to_string(callee->arg_size()) + ", got " + std::to_string(args_values.size()), node->location);
        }

        call_result_val = ctx.builder.CreateCall(callee, args_values, callee->getReturnType()->isVoidTy() ? "" : "calltmp");
    }

    const SymbolTable::ClassSymbol* return_static_ci = nullptr;
    if (!use_virtual_dispatch) {
        // For direct calls, we can look up return type info
        llvm::Function* callee = ctx.llvm_module.getFunction(resolved_func_name);
        if (callee) {
            auto return_info_it = ctx.function_return_class_info_map.find(callee);
            if (return_info_it != ctx.function_return_class_info_map.end()) {
                return_static_ci = return_info_it->second;
            }
        }
    }
    // For virtual calls, return type info handling can be enhanced later

    return ExpressionCGResult(call_result_val, return_static_ci);
}

CodeGenerator::ExpressionCGResult CodeGenerator::cg_primitive_method_call(std::shared_ptr<MethodCallExpressionNode> node, PrimitiveStructInfo* primitive_info, llvm::Value* instance_ptr) {
    std::string method_name;
    if (auto memberAccess = std::dynamic_pointer_cast<MemberAccessExpressionNode>(node->target)) {
        method_name = memberAccess->memberName->name;
    } else {
        log_compiler_error("Invalid method call structure for primitive method.", node->location);
    }

    std::string runtime_func_name;
    if (primitive_info->simple_name == "string") {
        if (method_name == "get_Length") runtime_func_name = "Mycelium_String_get_length";
        else if (method_name == "Substring") runtime_func_name = "Mycelium_String_substring";
        else if (method_name == "get_Empty") runtime_func_name = "Mycelium_String_get_empty";
    }

    if (runtime_func_name.empty()) {
        log_compiler_error("Unsupported primitive method: " + primitive_info->simple_name + "." + method_name, node->location);
    }

    llvm::Function* callee = ctx.llvm_module.getFunction(runtime_func_name);
    if (!callee) {
        log_compiler_error("Runtime function for primitive method not found: " + runtime_func_name, node->location);
    }

    std::vector<llvm::Value*> args;
    if (instance_ptr) { // For instance methods
        args.push_back(instance_ptr);
    }
    if (node->argumentList) {
        for (const auto& arg : node->argumentList->arguments) {
            args.push_back(cg_expression(arg->expression).value);
        }
    }

    llvm::Value* result = ctx.builder.CreateCall(callee, args);
    
    // Determine the primitive info of the return type for chaining
    auto result_node = ExpressionCGResult(result);
    std::string return_type_name;
    if (method_name == "get_Length") return_type_name = "int";
    else if (method_name == "Substring" || method_name == "get_Empty") return_type_name = "string";
    
    if (!return_type_name.empty()) {
        result_node.primitive_info = ctx.primitive_registry.get_by_simple_name(return_type_name);
    }

    return result_node;
}

} // namespace Mycelium::Scripting::Lang::CodeGen