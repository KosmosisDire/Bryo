// codegen.cpp - HLIR to LLVM IR Lowering Implementation
#include "codegen.hpp"
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <stdexcept>
#include <iostream>

namespace Fern
{

    // ============================================================================
    // Main Entry Point
    // ============================================================================

    std::unique_ptr<llvm::Module> HLIRCodeGen::lower(HLIR::Module *hlir_module)
    {
        if (!hlir_module)
        {
            throw std::runtime_error("Cannot lower null HLIR module");
        }

        // Phase 1: Declare all types
        declare_types(hlir_module);

        // Phase 2: Declare all functions
        declare_functions(hlir_module);

        // Phase 3: Generate function bodies
        generate_function_bodies(hlir_module);

        // Verify the generated module
        std::string error_msg;
        llvm::raw_string_ostream error_stream(error_msg);
        if (llvm::verifyModule(*module, &error_stream))
        {
            std::cerr << "LLVM Module verification failed:\n"
                      << error_msg << std::endl;
            module->print(llvm::errs(), nullptr);
            throw std::runtime_error("Invalid LLVM module generated");
        }

        return std::move(module);
    }

    // ============================================================================
    // Phase 1: Type Declaration
    // ============================================================================

    void HLIRCodeGen::declare_types(HLIR::Module *hlir_module)
    {
        // Declare all struct types first (opaque)
        for (const auto &type_def : hlir_module->types)
        {
            std::string type_name = type_def->symbol->get_qualified_name();
            auto *struct_type = llvm::StructType::create(context, type_name);
            struct_map[type_def.get()] = struct_type;

            // Also map the TypePtr (shared_ptr) to the struct type
            type_map[type_def->symbol->type] = struct_type;
        }

        // Now define the struct bodies
        for (const auto &type_def : hlir_module->types)
        {
            auto *struct_type = struct_map[type_def.get()];

            // Collect field types
            std::vector<llvm::Type *> field_types;
            for (const auto &member : type_def->symbol->member_order)
            {
                if (auto *var_sym = member->as<VariableSymbol>())
                {
                    llvm::Type *field_type = get_or_create_type(var_sym->type);
                    field_types.push_back(field_type);
                }
            }

            // Set the struct body
            if (!field_types.empty())
            {
                struct_type->setBody(field_types);
            }
        }
    }

    llvm::Type *HLIRCodeGen::get_or_create_type(TypePtr type)
    {
        if (!type)
        {
            return llvm::Type::getVoidTy(context);
        }

        // Check cache first
        auto it = type_map.find(type);
        if (it != type_map.end())
        {
            return it->second;
        }

        llvm::Type *llvm_type = nullptr;

        if (auto *prim_type = type->as<PrimitiveType>())
        {
            switch (prim_type->kind)
            {
            case PrimitiveKind::Void:
                llvm_type = llvm::Type::getVoidTy(context);
                break;
            case PrimitiveKind::Bool:
                llvm_type = llvm::Type::getInt1Ty(context);
                break;
            case PrimitiveKind::Char:
            case PrimitiveKind::I8:
            case PrimitiveKind::U8:
                llvm_type = llvm::Type::getInt8Ty(context);
                break;
            case PrimitiveKind::I16:
            case PrimitiveKind::U16:
                llvm_type = llvm::Type::getInt16Ty(context);
                break;
            case PrimitiveKind::I32:
            case PrimitiveKind::U32:
                llvm_type = llvm::Type::getInt32Ty(context);
                break;
            case PrimitiveKind::I64:
            case PrimitiveKind::U64:
                llvm_type = llvm::Type::getInt64Ty(context);
                break;
            case PrimitiveKind::F32:
                llvm_type = llvm::Type::getFloatTy(context);
                break;
            case PrimitiveKind::F64:
                llvm_type = llvm::Type::getDoubleTy(context);
                break;
            default:
                throw std::runtime_error("Unknown primitive type");
            }
        }
        else if (auto *ptr_type = type->as<PointerType>())
        {
            // LLVM 19+ uses opaque pointers - just use ptr type
            llvm_type = llvm::PointerType::get(context, 0);
        }
        else if (auto *array_type = type->as<ArrayType>())
        {
            llvm::Type *elem = get_or_create_type(array_type->element);

            if (array_type->size >= 0)
            {
                // Fixed-size array: [N x T] (stack allocated)
                llvm_type = llvm::ArrayType::get(elem, array_type->size);
            }
            else
            {
                // Dynamic arrays are represented as a struct { i32 length, ptr data }
                std::vector<llvm::Type *> fields = {
                    llvm::Type::getInt32Ty(context),           // length
                    llvm::PointerType::get(context, 0)         // data pointer (opaque)
                };
                llvm_type = llvm::StructType::create(context, fields, "array");
            }
        }
        else if (auto *named_type = type->as<NamedType>())
        {
            // Named types should already be in the map from declare_types
            auto it = type_map.find(type);
            if (it != type_map.end())
            {
                llvm_type = it->second;
            }
            else
            {
                throw std::runtime_error("Named type not declared: " + type->get_name());
            }
        }
        else
        {
            throw std::runtime_error("Cannot convert type to LLVM: " + type->get_name());
        }

        // Cache and return
        type_map[type] = llvm_type;
        return llvm_type;
    }

    llvm::StructType *HLIRCodeGen::declare_struct_type(HLIR::TypeDefinition *type_def)
    {
        auto it = struct_map.find(type_def);
        if (it != struct_map.end())
        {
            return it->second;
        }

        std::string type_name = type_def->symbol->get_qualified_name();
        auto *struct_type = llvm::StructType::create(context, type_name);
        struct_map[type_def] = struct_type;
        return struct_type;
    }

    // ============================================================================
    // Phase 2: Function Declaration
    // ============================================================================

    void HLIRCodeGen::declare_functions(HLIR::Module *hlir_module)
    {
        for (const auto &hlir_func : hlir_module->functions)
        {
            declare_function(hlir_func.get());
        }
    }

    llvm::Function *HLIRCodeGen::declare_function(HLIR::Function *hlir_func)
    {
        // Check if already declared
        auto it = function_map.find(hlir_func);
        if (it != function_map.end())
        {
            return it->second;
        }

        // Get function type
        llvm::FunctionType *func_type = get_function_type(hlir_func);

        // For external functions, use the simple name (not mangled)
        // For regular functions, use the fully qualified name
        std::string func_name;
        if (hlir_func->is_external && hlir_func->symbol)
        {
            func_name = hlir_func->symbol->name; // Simple name for external linkage
        }
        else
        {
            func_name = hlir_func->name(); // Qualified name for Fern functions
        }

        // Create function
        llvm::Function *llvm_func = llvm::Function::Create(
            func_type,
            llvm::Function::ExternalLinkage,
            func_name,
            module.get());

        // Set parameter names
        size_t param_idx = 0;
        for (auto &arg : llvm_func->args())
        {
            if (param_idx < hlir_func->params.size())
            {
                HLIR::Value *param_value = hlir_func->params[param_idx];
                arg.setName(param_value->debug_name);
            }
            param_idx++;
        }

        // Store mapping
        function_map[hlir_func] = llvm_func;
        return llvm_func;
    }

    llvm::FunctionType *HLIRCodeGen::get_function_type(HLIR::Function *hlir_func)
    {
        // Return type
        llvm::Type *ret_type = get_or_create_type(hlir_func->return_type());

        // Parameter types - HLIR already includes 'this' parameter explicitly for member functions
        std::vector<llvm::Type *> param_types;
        for (HLIR::Value *param : hlir_func->params)
        {
            llvm::Type *param_type = get_or_create_type(param->type);
            param_types.push_back(param_type);
        }

        return llvm::FunctionType::get(ret_type, param_types, false);
    }

    // ============================================================================
    // Phase 3: Function Body Generation
    // ============================================================================

    void HLIRCodeGen::generate_function_bodies(HLIR::Module *hlir_module)
    {
        for (const auto &hlir_func : hlir_module->functions)
        {
            if (!hlir_func->is_external && hlir_func->entry)
            {
                generate_function_body(hlir_func.get());
            }
        }
    }

    void HLIRCodeGen::generate_function_body(HLIR::Function *hlir_func)
    {
        // Set current function
        current_hlir_function = hlir_func;
        current_llvm_function = function_map[hlir_func];

        // Clear per-function state
        value_map.clear();
        block_map.clear();

        // Map function parameters to LLVM arguments
        size_t arg_idx = 0;
        for (auto &arg : current_llvm_function->args())
        {
            if (arg_idx < hlir_func->params.size())
            {
                value_map[hlir_func->params[arg_idx]] = &arg;
            }
            arg_idx++;
        }

        // Create LLVM basic blocks for all HLIR basic blocks
        for (const auto &hlir_block : hlir_func->blocks)
        {
            std::string block_name = hlir_block->name.empty()
                ? "bb" + std::to_string(hlir_block->id)
                : hlir_block->name;
            llvm::BasicBlock *llvm_block = llvm::BasicBlock::Create(
                context, block_name, current_llvm_function);
            block_map[hlir_block.get()] = llvm_block;
        }

        // Generate code for each basic block
        for (const auto &hlir_block : hlir_func->blocks)
        {
            generate_basic_block(hlir_block.get());
        }

        // Resolve pending phi nodes now that all values are generated
        for (const auto &[llvm_phi, hlir_phi] : pending_phis)
        {
            for (const auto &incoming : hlir_phi->incoming)
            {
                llvm::Value *value = get_value(incoming.first);
                llvm::BasicBlock *block = get_block(incoming.second);
                llvm_phi->addIncoming(value, block);
            }
        }
        pending_phis.clear();

        // Reset current function
        current_hlir_function = nullptr;
        current_llvm_function = nullptr;
    }

    void HLIRCodeGen::generate_basic_block(HLIR::BasicBlock *hlir_block)
    {
        llvm::BasicBlock *llvm_block = get_block(hlir_block);
        builder->SetInsertPoint(llvm_block);

        // Generate all instructions
        for (const auto &inst : hlir_block->instructions)
        {
            generate_instruction(inst.get());
        }
    }

    // ============================================================================
    // Instruction Generation
    // ============================================================================

    void HLIRCodeGen::generate_instruction(HLIR::Instruction *inst)
    {
        switch (inst->op)
        {
        case HLIR::Opcode::ConstInt:
            gen_const_int(static_cast<HLIR::ConstIntInst *>(inst));
            break;
        case HLIR::Opcode::ConstFloat:
            gen_const_float(static_cast<HLIR::ConstFloatInst *>(inst));
            break;
        case HLIR::Opcode::ConstBool:
            gen_const_bool(static_cast<HLIR::ConstBoolInst *>(inst));
            break;
        case HLIR::Opcode::ConstString:
            gen_const_string(static_cast<HLIR::ConstStringInst *>(inst));
            break;
        case HLIR::Opcode::Alloc:
            gen_alloc(static_cast<HLIR::AllocInst *>(inst));
            break;
        case HLIR::Opcode::Load:
            gen_load(static_cast<HLIR::LoadInst *>(inst));
            break;
        case HLIR::Opcode::Store:
            gen_store(static_cast<HLIR::StoreInst *>(inst));
            break;
        case HLIR::Opcode::FieldAddr:
            gen_field_addr(static_cast<HLIR::FieldAddrInst *>(inst));
            break;
        case HLIR::Opcode::ElementAddr:
            gen_element_addr(static_cast<HLIR::ElementAddrInst *>(inst));
            break;
        case HLIR::Opcode::Add:
        case HLIR::Opcode::Sub:
        case HLIR::Opcode::Mul:
        case HLIR::Opcode::Div:
        case HLIR::Opcode::Rem:
        case HLIR::Opcode::Eq:
        case HLIR::Opcode::Ne:
        case HLIR::Opcode::Lt:
        case HLIR::Opcode::Le:
        case HLIR::Opcode::Gt:
        case HLIR::Opcode::Ge:
        case HLIR::Opcode::And:
        case HLIR::Opcode::Or:
        case HLIR::Opcode::BitAnd:
        case HLIR::Opcode::BitOr:
        case HLIR::Opcode::BitXor:
        case HLIR::Opcode::Shl:
        case HLIR::Opcode::Shr:
            gen_binary(static_cast<HLIR::BinaryInst *>(inst));
            break;
        case HLIR::Opcode::Neg:
        case HLIR::Opcode::Not:
        case HLIR::Opcode::BitNot:
            gen_unary(static_cast<HLIR::UnaryInst *>(inst));
            break;
        case HLIR::Opcode::Cast:
            gen_cast(static_cast<HLIR::CastInst *>(inst));
            break;
        case HLIR::Opcode::Call:
            gen_call(static_cast<HLIR::CallInst *>(inst));
            break;
        case HLIR::Opcode::Ret:
            gen_ret(static_cast<HLIR::RetInst *>(inst));
            break;
        case HLIR::Opcode::Br:
            gen_br(static_cast<HLIR::BrInst *>(inst));
            break;
        case HLIR::Opcode::CondBr:
            gen_cond_br(static_cast<HLIR::CondBrInst *>(inst));
            break;
        case HLIR::Opcode::Phi:
            gen_phi(static_cast<HLIR::PhiInst *>(inst));
            break;
        default:
            throw std::runtime_error("Unsupported HLIR opcode: " +
                std::to_string(static_cast<int>(inst->op)));
        }
    }

    // ============================================================================
    // Constant Instructions
    // ============================================================================

    void HLIRCodeGen::gen_const_int(HLIR::ConstIntInst *inst)
    {
        llvm::Type *type = get_or_create_type(inst->result->type);

        // If type is void or invalid for integer constants, default to i32
        if (!type->isIntegerTy())
        {
            type = llvm::Type::getInt32Ty(context);
        }

        llvm::Value *const_val = llvm::ConstantInt::get(type, inst->value, true);
        value_map[inst->result] = const_val;
    }

    void HLIRCodeGen::gen_const_float(HLIR::ConstFloatInst *inst)
    {
        llvm::Type *type = get_or_create_type(inst->result->type);
        llvm::Value *const_val = llvm::ConstantFP::get(type, inst->value);
        value_map[inst->result] = const_val;
    }

    void HLIRCodeGen::gen_const_bool(HLIR::ConstBoolInst *inst)
    {
        llvm::Value *const_val = llvm::ConstantInt::get(
            llvm::Type::getInt1Ty(context), inst->value ? 1 : 0);
        value_map[inst->result] = const_val;
    }

    void HLIRCodeGen::gen_const_string(HLIR::ConstStringInst *inst)
    {
        // Create a global string constant
        llvm::Constant *str_const = llvm::ConstantDataArray::getString(context, inst->value);
        llvm::GlobalVariable *global_str = new llvm::GlobalVariable(
            *module,
            str_const->getType(),
            true,
            llvm::GlobalValue::PrivateLinkage,
            str_const,
            ".str");

        // Get pointer to the string (opaque pointer in LLVM 19+)
        llvm::Value *indices[] = {
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0),
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0)
        };
        llvm::Value *str_ptr = builder->CreateInBoundsGEP(
            str_const->getType(),
            global_str,
            indices,
            "str");

        value_map[inst->result] = str_ptr;
    }

    // ============================================================================
    // Memory Instructions
    // ============================================================================

    void HLIRCodeGen::gen_alloc(HLIR::AllocInst *inst)
    {
        llvm::Type *alloc_type = get_or_create_type(inst->alloc_type);

        llvm::Value *ptr;
        if (inst->on_stack)
        {
            // Stack allocation using alloca
            ptr = builder->CreateAlloca(alloc_type, nullptr, "alloc");
        }
        else
        {
            // Heap allocation using malloc
            llvm::Value *size = llvm::ConstantInt::get(
                llvm::Type::getInt64Ty(context),
                module->getDataLayout().getTypeAllocSize(alloc_type));

            // Declare/get malloc function
            llvm::FunctionType *malloc_type = llvm::FunctionType::get(
                llvm::PointerType::get(context, 0),  // Returns opaque ptr
                {llvm::Type::getInt64Ty(context)},
                false);
            llvm::FunctionCallee malloc_func = module->getOrInsertFunction("malloc", malloc_type);

            // Call malloc - returns opaque pointer
            ptr = builder->CreateCall(malloc_func, {size}, "heap_alloc");
        }

        value_map[inst->result] = ptr;
    }

    void HLIRCodeGen::gen_load(HLIR::LoadInst *inst)
    {
        llvm::Value *addr = get_value(inst->address);

        // Validate that we're loading from a pointer
        if (!addr->getType()->isPointerTy())
        {
            std::string error = "Load instruction expects pointer, but got: ";
            llvm::raw_string_ostream os(error);
            addr->getType()->print(os);
            os << "\n  Address HLIR value: %" << inst->address->id;
            if (!inst->address->debug_name.empty())
            {
                os << " <" << inst->address->debug_name << ">";
            }
            os << " : " << inst->address->type->get_name();
            throw std::runtime_error(os.str());
        }

        llvm::Type *load_type = get_or_create_type(inst->result->type);
        llvm::Value *loaded = builder->CreateLoad(load_type, addr, "load");
        value_map[inst->result] = loaded;
    }

    void HLIRCodeGen::gen_store(HLIR::StoreInst *inst)
    {
        llvm::Value *val = get_value(inst->value);
        llvm::Value *addr = get_value(inst->address);
        builder->CreateStore(val, addr);
    }

    void HLIRCodeGen::gen_field_addr(HLIR::FieldAddrInst *inst)
    {
        llvm::Value *obj = get_value(inst->object);

        // Get the struct type we're accessing
        // If object is a pointer, we need to get the pointee type
        llvm::Type *struct_type = get_or_create_type(inst->object->type);

        // If the HLIR type is a pointer, get the pointee type for GEP
        if (auto *ptr_type = inst->object->type->as<PointerType>())
        {
            struct_type = get_or_create_type(ptr_type->pointee);
        }

        // GEP to get field address
        llvm::Value *field_ptr = builder->CreateStructGEP(
            struct_type,
            obj,
            inst->field_index,
            "field_addr");

        value_map[inst->result] = field_ptr;
    }

    void HLIRCodeGen::gen_element_addr(HLIR::ElementAddrInst *inst)
    {
        llvm::Value *array = get_value(inst->array);
        llvm::Value *index = get_value(inst->index);

        // Get array type info
        llvm::Type *array_llvm_type = get_or_create_type(inst->array->type);

        // Get element type
        llvm::Type *elem_type = get_or_create_type(inst->result->type);
        if (auto *ptr_type = inst->result->type->as<PointerType>())
        {
            elem_type = get_or_create_type(ptr_type->pointee);
        }

        // Check if this is a fixed-size array [N x T]
        if (array_llvm_type->isArrayTy())
        {
            // For fixed arrays, we need GEP with two indices: [0, index]
            // First index (0) is because we have a pointer to the array
            // Second index is the actual element index
            llvm::Value *zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);
            llvm::Value *indices[] = { zero, index };
            llvm::Value *elem_ptr = builder->CreateInBoundsGEP(
                array_llvm_type,
                array,
                indices,
                "elem_addr");

            value_map[inst->result] = elem_ptr;
        }
        // Check if this is a dynamic array (struct { i32, ptr })
        else if (array_llvm_type->isStructTy())
        {
            // Load the data pointer (second field of the array struct)
            llvm::Value *data_ptr_addr = builder->CreateStructGEP(
                array_llvm_type,
                array,
                1,
                "data_ptr_addr");

            llvm::Type *ptr_type = llvm::PointerType::get(context, 0);
            llvm::Value *data_ptr = builder->CreateLoad(
                ptr_type,
                data_ptr_addr,
                "data_ptr");

            // GEP into the data pointer
            llvm::Value *elem_ptr = builder->CreateGEP(
                elem_type,
                data_ptr,
                index,
                "elem_addr");

            value_map[inst->result] = elem_ptr;
        }
        else
        {
            // Direct pointer - simple GEP
            llvm::Value *elem_ptr = builder->CreateGEP(
                elem_type,
                array,
                index,
                "elem_addr");

            value_map[inst->result] = elem_ptr;
        }
    }

    // ============================================================================
    // Binary Instructions
    // ============================================================================

    void HLIRCodeGen::gen_binary(HLIR::BinaryInst *inst)
    {
        llvm::Value *left = get_value(inst->left);
        llvm::Value *right = get_value(inst->right);
        llvm::Value *result = nullptr;

        TypePtr operand_type = inst->left->type;
        bool is_float_op = is_float(operand_type);
        bool is_signed = is_signed_int(operand_type);

        switch (inst->op)
        {
        case HLIR::Opcode::Add:
            result = is_float_op ? builder->CreateFAdd(left, right, "add")
                                 : builder->CreateAdd(left, right, "add");
            break;
        case HLIR::Opcode::Sub:
            result = is_float_op ? builder->CreateFSub(left, right, "sub")
                                 : builder->CreateSub(left, right, "sub");
            break;
        case HLIR::Opcode::Mul:
            result = is_float_op ? builder->CreateFMul(left, right, "mul")
                                 : builder->CreateMul(left, right, "mul");
            break;
        case HLIR::Opcode::Div:
            if (is_float_op)
                result = builder->CreateFDiv(left, right, "div");
            else if (is_signed)
                result = builder->CreateSDiv(left, right, "div");
            else
                result = builder->CreateUDiv(left, right, "div");
            break;
        case HLIR::Opcode::Rem:
            if (is_float_op)
                result = builder->CreateFRem(left, right, "rem");
            else if (is_signed)
                result = builder->CreateSRem(left, right, "rem");
            else
                result = builder->CreateURem(left, right, "rem");
            break;
        case HLIR::Opcode::Eq:
            result = is_float_op ? builder->CreateFCmpOEQ(left, right, "eq")
                                 : builder->CreateICmpEQ(left, right, "eq");
            break;
        case HLIR::Opcode::Ne:
            result = is_float_op ? builder->CreateFCmpONE(left, right, "ne")
                                 : builder->CreateICmpNE(left, right, "ne");
            break;
        case HLIR::Opcode::Lt:
            if (is_float_op)
                result = builder->CreateFCmpOLT(left, right, "lt");
            else if (is_signed)
                result = builder->CreateICmpSLT(left, right, "lt");
            else
                result = builder->CreateICmpULT(left, right, "lt");
            break;
        case HLIR::Opcode::Le:
            if (is_float_op)
                result = builder->CreateFCmpOLE(left, right, "le");
            else if (is_signed)
                result = builder->CreateICmpSLE(left, right, "le");
            else
                result = builder->CreateICmpULE(left, right, "le");
            break;
        case HLIR::Opcode::Gt:
            if (is_float_op)
                result = builder->CreateFCmpOGT(left, right, "gt");
            else if (is_signed)
                result = builder->CreateICmpSGT(left, right, "gt");
            else
                result = builder->CreateICmpUGT(left, right, "gt");
            break;
        case HLIR::Opcode::Ge:
            if (is_float_op)
                result = builder->CreateFCmpOGE(left, right, "ge");
            else if (is_signed)
                result = builder->CreateICmpSGE(left, right, "ge");
            else
                result = builder->CreateICmpUGE(left, right, "ge");
            break;
        case HLIR::Opcode::And:
            result = builder->CreateAnd(left, right, "and");
            break;
        case HLIR::Opcode::Or:
            result = builder->CreateOr(left, right, "or");
            break;
        case HLIR::Opcode::BitAnd:
            result = builder->CreateAnd(left, right, "bitand");
            break;
        case HLIR::Opcode::BitOr:
            result = builder->CreateOr(left, right, "bitor");
            break;
        case HLIR::Opcode::BitXor:
            result = builder->CreateXor(left, right, "bitxor");
            break;
        case HLIR::Opcode::Shl:
            result = builder->CreateShl(left, right, "shl");
            break;
        case HLIR::Opcode::Shr:
            result = is_signed ? builder->CreateAShr(left, right, "shr")
                               : builder->CreateLShr(left, right, "shr");
            break;
        default:
            throw std::runtime_error("Unsupported binary operation");
        }

        value_map[inst->result] = result;
    }

    // ============================================================================
    // Unary Instructions
    // ============================================================================

    void HLIRCodeGen::gen_unary(HLIR::UnaryInst *inst)
    {
        llvm::Value *operand = get_value(inst->operand);
        llvm::Value *result = nullptr;

        switch (inst->op)
        {
        case HLIR::Opcode::Neg:
            if (is_float(inst->operand->type))
                result = builder->CreateFNeg(operand, "neg");
            else
                result = builder->CreateNeg(operand, "neg");
            break;
        case HLIR::Opcode::Not:
            result = builder->CreateNot(operand, "not");
            break;
        case HLIR::Opcode::BitNot:
            result = builder->CreateNot(operand, "bitnot");
            break;
        default:
            throw std::runtime_error("Unsupported unary operation");
        }

        value_map[inst->result] = result;
    }

    // ============================================================================
    // Cast Instruction
    // ============================================================================

    void HLIRCodeGen::gen_cast(HLIR::CastInst *inst)
    {
        llvm::Value *value = get_value(inst->value);
        llvm::Type *target_type = get_or_create_type(inst->target_type);

        llvm::Value *result = nullptr;

        TypePtr source_type = inst->value->type;
        bool src_is_float = is_float(source_type);
        bool dst_is_float = is_float(inst->target_type);
        bool src_is_signed = is_signed_int(source_type);

        if (src_is_float && dst_is_float)
        {
            // Float to float
            result = builder->CreateFPCast(value, target_type, "cast");
        }
        else if (src_is_float && !dst_is_float)
        {
            // Float to int
            if (is_signed_int(inst->target_type))
                result = builder->CreateFPToSI(value, target_type, "cast");
            else
                result = builder->CreateFPToUI(value, target_type, "cast");
        }
        else if (!src_is_float && dst_is_float)
        {
            // Int to float
            if (src_is_signed)
                result = builder->CreateSIToFP(value, target_type, "cast");
            else
                result = builder->CreateUIToFP(value, target_type, "cast");
        }
        else
        {
            // Int to int
            result = builder->CreateIntCast(value, target_type, src_is_signed, "cast");
        }

        value_map[inst->result] = result;
    }

    // ============================================================================
    // Call Instruction
    // ============================================================================

    void HLIRCodeGen::gen_call(HLIR::CallInst *inst)
    {
        llvm::Function *callee = function_map[inst->callee];
        if (!callee)
        {
            throw std::runtime_error("Function not declared: " + inst->callee->name());
        }

        // Collect arguments
        std::vector<llvm::Value *> args;
        for (HLIR::Value *arg : inst->args)
        {
            args.push_back(get_value(arg));
        }

        // Create call - only name the result if it's not void
        llvm::Value *call_result = builder->CreateCall(callee, args,
            callee->getReturnType()->isVoidTy() ? "" : "call");

        // Map result if not void
        if (inst->result)
        {
            value_map[inst->result] = call_result;
        }
    }

    // ============================================================================
    // Control Flow Instructions
    // ============================================================================

    void HLIRCodeGen::gen_ret(HLIR::RetInst *inst)
    {
        if (inst->value)
        {
            llvm::Value *ret_val = get_value(inst->value);
            builder->CreateRet(ret_val);
        }
        else
        {
            builder->CreateRetVoid();
        }
    }

    void HLIRCodeGen::gen_br(HLIR::BrInst *inst)
    {
        llvm::BasicBlock *target = get_block(inst->target);
        builder->CreateBr(target);
    }

    void HLIRCodeGen::gen_cond_br(HLIR::CondBrInst *inst)
    {
        llvm::Value *cond = get_value(inst->condition);
        llvm::BasicBlock *true_block = get_block(inst->true_block);
        llvm::BasicBlock *false_block = get_block(inst->false_block);
        builder->CreateCondBr(cond, true_block, false_block);
    }

    void HLIRCodeGen::gen_phi(HLIR::PhiInst *inst)
    {
        llvm::Type *phi_type = get_or_create_type(inst->result->type);
        llvm::PHINode *phi = builder->CreatePHI(phi_type, inst->incoming.size(), "phi");

        // Register the phi node result immediately so it can be referenced
        value_map[inst->result] = phi;

        // Defer adding incoming values until all blocks are processed
        // This is necessary because incoming values might be defined in blocks
        // that haven't been generated yet
        pending_phis.push_back({phi, inst});
    }

    // ============================================================================
    // Helper Functions
    // ============================================================================

    llvm::Value *HLIRCodeGen::get_value(HLIR::Value *hlir_value)
    {
        auto it = value_map.find(hlir_value);
        if (it == value_map.end())
        {
            throw std::runtime_error("HLIR value not found: %" +
                std::to_string(hlir_value->id));
        }
        return it->second;
    }

    llvm::BasicBlock *HLIRCodeGen::get_block(HLIR::BasicBlock *hlir_block)
    {
        auto it = block_map.find(hlir_block);
        if (it == block_map.end())
        {
            throw std::runtime_error("HLIR block not found: bb" +
                std::to_string(hlir_block->id));
        }
        return it->second;
    }

    bool HLIRCodeGen::is_signed_int(TypePtr type)
    {
        if (auto *prim = type->as<PrimitiveType>())
        {
            return prim->kind == PrimitiveKind::I8 ||
                   prim->kind == PrimitiveKind::I16 ||
                   prim->kind == PrimitiveKind::I32 ||
                   prim->kind == PrimitiveKind::I64;
        }
        return false;
    }

    bool HLIRCodeGen::is_unsigned_int(TypePtr type)
    {
        if (auto *prim = type->as<PrimitiveType>())
        {
            return prim->kind == PrimitiveKind::U8 ||
                   prim->kind == PrimitiveKind::U16 ||
                   prim->kind == PrimitiveKind::U32 ||
                   prim->kind == PrimitiveKind::U64;
        }
        return false;
    }

    bool HLIRCodeGen::is_float(TypePtr type)
    {
        if (auto *prim = type->as<PrimitiveType>())
        {
            return prim->kind == PrimitiveKind::F32 ||
                   prim->kind == PrimitiveKind::F64;
        }
        return false;
    }

} // namespace Fern