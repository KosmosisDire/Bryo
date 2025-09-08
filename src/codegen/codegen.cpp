#include "codegen/codegen.hpp"
#include "semantic/conversions.hpp"
#include <llvm/IR/Constants.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/Support/Casting.h>
#include "semantic/type.hpp"
#include <iostream>

namespace Bryo
{

    // === Main API ===

    std::unique_ptr<llvm::Module> CodeGenerator::generate(CompilationUnitSyntax *unit)
    {
        // Clear any previous state from prior runs
        locals.clear();
        local_types.clear();
        type_cache.clear();
        defined_types.clear();
        declared_functions.clear();
        errors.clear();
        while (!value_stack.empty())
            value_stack.pop();

        // Step 1: Declare all user-defined types (structs) recursively from the global namespace.
        // This pass ensures that any function or type can reference any other type, regardless of order.
        declare_all_types();

        // Step 2: Declare all function signatures.
        // This allows for mutual recursion and calls to functions defined later in the file.
        declare_all_functions();

        generate_builtin_functions();

        // Step 3: Generate the actual code for function bodies and global initializers.
        visit(unit);

        // Step 4: Verify the generated module for consistency.
        std::string verify_error;
        llvm::raw_string_ostream error_stream(verify_error);
        if (llvm::verifyModule(*module, &error_stream))
        {
            report_general_error("LLVM Module verification failed: " + verify_error);
        }

        return std::move(module);
    }

    // === Pre-declaration Passes ===

    void CodeGenerator::declare_all_types()
    {
        auto global_scope = symbol_table.get_global_namespace();
        if (global_scope)
        {
            declare_all_types_in_scope(global_scope);
        }
    }

    void CodeGenerator::declare_all_types_in_scope(Scope *scope)
    {
        if (!scope)
            return;

        // Phase 1: Create all opaque struct types first
        for (const auto &[name, symbol] : scope->symbols)
        {
            if (auto type_sym = symbol->as<TypeSymbol>())
            {
                if (defined_types.count(type_sym) > 0)
                    continue;

                // Create an "opaque" struct type first. This is crucial for handling
                // recursive types (e.g., struct BaseSyntax { BaseSyntax* next; }).
                llvm::StructType *struct_type = llvm::StructType::create(*context, type_sym->get_qualified_name());
                defined_types[type_sym] = struct_type;
            }

            // Recursively process nested scopes (e.g., namespaces)
            if (auto nested_scope = symbol->as<Scope>())
            {
                declare_all_types_in_scope(nested_scope);
            }
        }

        // Phase 2: Now set the body of all struct types
        for (const auto &[name, symbol] : scope->symbols)
        {
            if (auto type_sym = symbol->as<TypeSymbol>())
            {
                auto type_it = defined_types.find(type_sym);
                if (type_it == defined_types.end())
                    continue; // Already processed

                llvm::StructType *struct_type = llvm::cast<llvm::StructType>(type_it->second);
                if (!struct_type->isOpaque())
                    continue; // Body already set

                // Now, define the body of the struct by resolving its member types.
                std::vector<llvm::Type *> member_types;
                for (const auto &[member_name, member_symbol] : type_sym->symbols)
                {
                    if (auto field = member_symbol->as<VariableSymbol>())
                    {
                        member_types.push_back(get_llvm_type(field->type()));
                    }
                }
                struct_type->setBody(member_types);
            }
        }
    }

    void CodeGenerator::declare_all_functions()
    {
        auto global_scope = symbol_table.get_global_namespace();
        if (global_scope)
        {
            declare_all_functions_in_scope(global_scope);
        }
    }

    void CodeGenerator::declare_all_functions_in_scope(Scope *scope)
    {
        if (!scope)
            return;

        for (const auto &[name, symbol_node] : scope->symbols)
        {
            if (auto func_sym = symbol_node->as<FunctionGroupSymbol>())
            {
                auto overloads = func_sym->get_overloads();
                for (const auto &overload : overloads)
                {
                    declare_function_from_symbol(overload);
                }
            }

            // Look inside types for methods
            if (auto type_sym = symbol_node->as<TypeSymbol>())
            {
                // Iterate through type members to find methods
                for (const auto &[member_name, member_symbol] : type_sym->symbols)
                {
                    if (auto method_sym = member_symbol->as<FunctionSymbol>())
                    {
                        declare_function_from_symbol(method_sym);
                    }
                }
            }

            // Add property getter/setter declarations
            if (auto prop_sym = symbol_node->as<PropertySymbol>())
            {
                // Generate getter function declaration
                std::string getter_name = prop_sym->get_qualified_name() + ".get";

                // Getter takes 'this' pointer and returns property type
                std::vector<llvm::Type *> param_types;

                // Get the containing type for 'this' parameter
                if (auto type_sym = prop_sym->parent->as<TypeSymbol>())
                {
                    auto type_it = defined_types.find(type_sym);
                    if (type_it != defined_types.end())
                    {
                        param_types.push_back(llvm::PointerType::get(*context, 0)); // Opaque pointer
                    }
                }

                llvm::Type *return_type = get_llvm_type(prop_sym->type());
                auto func_type = llvm::FunctionType::get(return_type, param_types, false);

                llvm::Function::Create(
                    func_type, llvm::Function::ExternalLinkage, getter_name, module.get());

                declared_functions.insert(getter_name);
            }

            // Recursively process nested scopes
            if (auto nested_scope = symbol_node->as<Scope>())
            {
                declare_all_functions_in_scope(nested_scope);
            }
        }
    }

    llvm::Function *CodeGenerator::declare_function_from_symbol(FunctionSymbol *func_symbol)
    {
        if (!func_symbol)
            return nullptr;

        const std::string &func_name = func_symbol->get_mangled_name();
        if (declared_functions.count(func_name) > 0)
        {
            return module->getFunction(func_name);
        }

        std::vector<llvm::Type *> param_types;

        // Check if this is a method (function inside a type)
        // If so, add 'this' as the first parameter
        TypeSymbol *parent_type = nullptr;
        if (func_symbol->parent)
        {
            // Check if parent is directly a TypeSymbol
            parent_type = func_symbol->parent->as<TypeSymbol>();

            // If not, check if parent is a FunctionGroupSymbol whose parent is a TypeSymbol
            if (!parent_type)
            {
                if (auto func_group = func_symbol->parent->as<FunctionGroupSymbol>())
                {
                    parent_type = func_group->parent ? func_group->parent->as<TypeSymbol>() : nullptr;
                }
            }
        }

        if (parent_type)
        {
            // This is a method - add 'this' pointer as first parameter
            auto type_it = defined_types.find(parent_type);
            if (type_it != defined_types.end())
            {
                param_types.push_back(llvm::PointerType::get(*context, 0)); // Opaque pointer for 'this'
            }
        }

        // Add the explicitly declared parameters
        for (const auto &param_handle : func_symbol->parameters())
        {
            auto param_symbol = param_handle;
            if (!param_symbol)
            {
                report_general_error("Internal error: Parameter symbol not found");
                continue;
            }

            llvm::Type *param_type = get_llvm_type(param_symbol->type());

            // If parameter is an array, use pointer instead of array type
            if (param_type->isArrayTy())
            {
                param_types.push_back(llvm::PointerType::get(*context, 0)); // Opaque pointer for arrays
            }
            else
            {
                param_types.push_back(param_type);
            }
        }

        llvm::Type *return_type = get_llvm_type(func_symbol->return_type());
        auto func_type = llvm::FunctionType::get(return_type, param_types, false);

        auto function = llvm::Function::Create(
            func_type, llvm::Function::ExternalLinkage, func_name, module.get());

        declared_functions.insert(func_name);
        return function;
    }

    void CodeGenerator::generate_definitions(CompilationUnitSyntax *unit)
    {
        if (unit)
        {
            unit->accept(this);
        }
    }

    // === Helper Methods ===

    llvm::Value *CodeGenerator::castPrimitive(llvm::Value *value, PrimitiveType::Kind sourceKind, PrimitiveType::Kind targetKind, BaseSyntax *node)
    {
        // Get the conversion kind
        auto convKind = Conversions::ClassifyConversion(sourceKind, targetKind);

        if (convKind == ConversionKind::NoConversion)
        {
            report_error(node, "Invalid cast between primitive types");
            return nullptr;
        }

        if (convKind == ConversionKind::Identity)
        {
            return value; // No cast needed
        }

        // Get LLVM types
        auto source_llvm_type = value->getType();
        auto target_llvm_type = get_llvm_type(symbol_table.get_type_system().get_primitive_type(targetKind));

        // Helper to check if a type is signed
        auto isSigned = [](PrimitiveType::Kind kind)
        {
            switch (kind)
            {
            case PrimitiveType::I8:
            case PrimitiveType::I16:
            case PrimitiveType::I32:
            case PrimitiveType::I64:
            case PrimitiveType::Char: // char is signed in your implementation
                return true;
            default:
                return false;
            }
        };

        // Helper to check if a type is floating point
        auto isFloat = [](PrimitiveType::Kind kind)
        {
            return kind == PrimitiveType::F32 || kind == PrimitiveType::F64;
        };

        // Perform the appropriate cast based on source and target types
        llvm::Value *result = nullptr;

        // 1. Integer to Integer conversions
        if (!isFloat(sourceKind) && !isFloat(targetKind) &&
            sourceKind != PrimitiveType::Bool && targetKind != PrimitiveType::Bool)
        {
            auto source_bits = source_llvm_type->getIntegerBitWidth();
            auto target_bits = target_llvm_type->getIntegerBitWidth();

            if (source_bits < target_bits)
            {
                // Widening conversion
                if (isSigned(sourceKind))
                {
                    // Sign extension for signed types
                    result = builder->CreateSExt(value, target_llvm_type, "sext");
                }
                else
                {
                    // Zero extension for unsigned types
                    result = builder->CreateZExt(value, target_llvm_type, "zext");
                }
            }
            else if (source_bits > target_bits)
            {
                // Narrowing conversion - truncate
                result = builder->CreateTrunc(value, target_llvm_type, "trunc");
            }
            else
            {
                // Same bit width but different signedness - bitcast
                result = builder->CreateBitCast(value, target_llvm_type, "bitcast");
            }
        }
        // 2. Float to Float conversions
        else if (isFloat(sourceKind) && isFloat(targetKind))
        {
            if (sourceKind == PrimitiveType::F32 && targetKind == PrimitiveType::F64)
            {
                // Float extension (f32 -> f64)
                result = builder->CreateFPExt(value, target_llvm_type, "fpext");
            }
            else if (sourceKind == PrimitiveType::F64 && targetKind == PrimitiveType::F32)
            {
                // Float truncation (f64 -> f32)
                result = builder->CreateFPTrunc(value, target_llvm_type, "fptrunc");
            }
        }
        // 3. Integer to Float conversions
        else if (!isFloat(sourceKind) && isFloat(targetKind))
        {
            // Special case for bool to float
            if (sourceKind == PrimitiveType::Bool)
            {
                // First extend bool to i32, then convert to float
                auto i32_type = llvm::Type::getInt32Ty(*context);
                auto extended = builder->CreateZExt(value, i32_type, "bool.to.i32");
                result = builder->CreateUIToFP(extended, target_llvm_type, "uitofp");
            }
            else if (isSigned(sourceKind))
            {
                // Signed int to float
                result = builder->CreateSIToFP(value, target_llvm_type, "sitofp");
            }
            else
            {
                // Unsigned int to float
                result = builder->CreateUIToFP(value, target_llvm_type, "uitofp");
            }
        }
        // 4. Float to Integer conversions
        else if (isFloat(sourceKind) && !isFloat(targetKind))
        {
            // Special case for float to bool
            if (targetKind == PrimitiveType::Bool)
            {
                // Compare float with 0.0
                auto zero = llvm::ConstantFP::get(source_llvm_type, 0.0);
                result = builder->CreateFCmpONE(value, zero, "fp.to.bool");
            }
            else if (isSigned(targetKind))
            {
                // Float to signed int
                result = builder->CreateFPToSI(value, target_llvm_type, "fptosi");
            }
            else
            {
                // Float to unsigned int
                result = builder->CreateFPToUI(value, target_llvm_type, "fptoui");
            }
        }
        // 5. Bool conversions
        else if (sourceKind == PrimitiveType::Bool && !isFloat(targetKind))
        {
            // Bool to integer - zero extend
            result = builder->CreateZExt(value, target_llvm_type, "bool.zext");
        }
        else if (!isFloat(sourceKind) && targetKind == PrimitiveType::Bool)
        {
            // Integer to bool - compare with zero
            auto zero = llvm::ConstantInt::get(source_llvm_type, 0);
            result = builder->CreateICmpNE(value, zero, "to.bool");
        }
        // 6. Char conversions (char is i8 in your implementation)
        else if (sourceKind == PrimitiveType::Char || targetKind == PrimitiveType::Char)
        {
            // Treat char as signed i8
            if (sourceKind == PrimitiveType::Char)
            {
                // Casting from char - treat as signed i8
                return castPrimitive(value, PrimitiveType::I8, targetKind, node);
            }
            else
            {
                // Casting to char - treat as signed i8
                return castPrimitive(value, sourceKind, PrimitiveType::I8, node);
            }
        }

        if (!result)
        {
            report_error(node, "Failed to generate cast instruction");
        }

        return result;
    }

    llvm::Value *CodeGenerator::genExpression(BaseExprSyntax *expr, bool wantAddress)
    {
        if (!expr)
            return nullptr;

        if (wantAddress)
        {
            if (!expr->is_l_value())
            {
                report_error(expr, "Cannot take address of rvalue expression");
                return nullptr;
            }
            return genLValue(expr);
        }
        else
        {
            return genRValue(expr);
        }
    }

    llvm::Value *CodeGenerator::genLValue(BaseExprSyntax *expr)
    {
        if (!expr || !expr->is_l_value())
        {
            report_error(expr, "Expected lvalue expression");
            return nullptr;
        }

        // Visit the expression - should push an address
        expr->accept(this);
        return pop_value();
    }

    llvm::Value *CodeGenerator::genRValue(BaseExprSyntax *expr)
    {
        if (!expr)
            return nullptr;

        if (expr->is_l_value())
        {
            // Get address then load based on storage kind
            auto addr = genLValue(expr);
            if (!addr)
                return nullptr;
            return loadValue(addr, expr->resolvedType);
        }
        else
        {
            // Direct rvalue - just evaluate
            expr->accept(this);
            return pop_value();
        }
    }

    llvm::Value *CodeGenerator::loadValue(llvm::Value *ptr, TypePtr type)
    {
        if (!ptr || !type)
            return nullptr;

        auto storageKind = type->get_storage_kind();

        switch (storageKind)
        {
        case Type::StorageKind::Direct:
            // Value types: load the actual value
            return builder->CreateLoad(get_llvm_type(type), ptr, "load.direct");

        case Type::StorageKind::Indirect:
            // Reference types: the pointer IS the value
            // Load the pointer itself from the variable
            return builder->CreateLoad(
                llvm::PointerType::get(*context, 0),
                ptr,
                "load.ref");

        case Type::StorageKind::Explicit:
            // Explicit pointer: load the pointer value
            return builder->CreateLoad(
                llvm::PointerType::get(*context, 0),
                ptr,
                "load.ptr");
        }

        return nullptr;
    }

    void CodeGenerator::storeValue(llvm::Value *value, llvm::Value *ptr, TypePtr type)
    {
        if (!value || !ptr || !type)
            return;

        // For all storage kinds, just store the value
        // The difference is what the "value" represents:
        // - Direct: the actual data
        // - Indirect: a pointer to the data
        // - Explicit: a pointer value
        builder->CreateStore(value, ptr);
    }

    llvm::Value *CodeGenerator::ensureValue(llvm::Value *val, TypePtr type)
    {
        if (!val || !type)
            return nullptr;

        // If it's already a value (not a pointer), return as-is
        if (!val->getType()->isPointerTy())
        {
            return val;
        }

        // It's a pointer - load based on storage kind
        return loadValue(val, type);
    }

    llvm::Value *CodeGenerator::ensureAddress(llvm::Value *val, TypePtr type)
    {
        if (!val || !type)
            return nullptr;

        // If it's already a pointer, return as-is
        if (val->getType()->isPointerTy())
        {
            return val;
        }

        // It's a value - need to create temporary storage
        auto llvm_type = get_llvm_type(type);
        auto temp = builder->CreateAlloca(llvm_type, nullptr, "temp.addr");
        builder->CreateStore(val, temp);
        return temp;
    }

    bool CodeGenerator::isUnsignedType(TypePtr type)
    {
        if (!type)
            return false;

        if (auto prim = std::get_if<PrimitiveType>(&type->value))
        {
            switch (prim->kind)
            {
            case PrimitiveType::U8:
            case PrimitiveType::U16:
            case PrimitiveType::U32:
            case PrimitiveType::U64:
                return true;
            default:
                return false;
            }
        }
        return false;
    }

    bool CodeGenerator::isSignedType(TypePtr type)
    {
        if (!type)
            return false;

        if (auto prim = std::get_if<PrimitiveType>(&type->value))
        {
            switch (prim->kind)
            {
            case PrimitiveType::I8:
            case PrimitiveType::I16:
            case PrimitiveType::I32:
            case PrimitiveType::I64:
                return true;
            default:
                return false;
            }
        }
        return false;
    }

    bool CodeGenerator::isFloatingPointType(TypePtr type)
    {
        if (!type)
            return false;

        if (auto prim = std::get_if<PrimitiveType>(&type->value))
        {
            switch (prim->kind)
            {
            case PrimitiveType::F32:
            case PrimitiveType::F64:
                return true;
            default:
                return false;
            }
        }
        return false;
    }

    bool CodeGenerator::isIntegerType(TypePtr type)
    {
        if (!type)
            return false;

        if (auto prim = std::get_if<PrimitiveType>(&type->value))
        {
            switch (prim->kind)
            {
            case PrimitiveType::I8:
            case PrimitiveType::I16:
            case PrimitiveType::I32:
            case PrimitiveType::I64:
            case PrimitiveType::U8:
            case PrimitiveType::U16:
            case PrimitiveType::U32:
            case PrimitiveType::U64:
                return true;
            default:
                return false;
            }
        }
        return false;
    }

    void CodeGenerator::generate_builtin_functions()
    {
        // Save the current insertion point
        auto saved_function = current_function;
        auto saved_block = builder->GetInsertBlock();

        // ===== 1. Generate Print function =====
        {
            // First, declare the external printf function if not already declared
            llvm::Function *printf_func = module->getFunction("printf");
            if (!printf_func)
            {
                // printf signature: int printf(const char*, ...)
                std::vector<llvm::Type *> printf_params;
                printf_params.push_back(llvm::PointerType::get(*context, 0)); // Opaque pointer for format string

                auto printf_type = llvm::FunctionType::get(
                    llvm::Type::getInt32Ty(*context), // Returns int
                    printf_params,
                    true // Variadic function
                );

                printf_func = llvm::Function::Create(
                    printf_type,
                    llvm::Function::ExternalLinkage,
                    "printf",
                    module.get());
            }

            // Check if Print function already exists (might be declared by declare_all_functions)
            llvm::Function *print_func = module->getFunction("Print_void_char*");
            if (print_func && print_func->empty()) // Function declared but no body
            {
                // Create the function body
                current_function = print_func;

                auto entry = llvm::BasicBlock::Create(*context, "entry", print_func);
                builder->SetInsertPoint(entry);

                // Get the array parameter (now passed as a pointer)
                llvm::Value *array_ptr = print_func->arg_begin();
                array_ptr->setName("message.ptr");

                // The array is already a pointer, so we can directly call printf with it
                builder->CreateCall(printf_func, {array_ptr});

                // Return void
                builder->CreateRetVoid();
            }
        }

        // ===== 2. Generate Malloc function (heap allocation) =====
        {
            // First, declare the external malloc function if not already declared
            llvm::Function *malloc_func = module->getFunction("malloc");
            if (!malloc_func)
            {
                // malloc signature: void* malloc(size_t size)
                std::vector<llvm::Type *> malloc_params;
                malloc_params.push_back(llvm::Type::getInt32Ty(*context)); // size_t (using i32)

                auto malloc_type = llvm::FunctionType::get(
                    llvm::PointerType::get(*context, 0), // Returns void* (opaque pointer)
                    malloc_params,
                    false // Not variadic
                );

                malloc_func = llvm::Function::Create(
                    malloc_type,
                    llvm::Function::ExternalLinkage,
                    "malloc",
                    module.get());
            }

            // Check if Malloc function already exists
            llvm::Function *Malloc_func = module->getFunction("Malloc_i8*_");
            if (Malloc_func && Malloc_func->empty()) // Function declared but no body
            {
                // Create the function body
                current_function = Malloc_func;

                auto entry = llvm::BasicBlock::Create(*context, "entry", Malloc_func);
                builder->SetInsertPoint(entry);

                // Get the size parameter
                llvm::Value *size = Malloc_func->arg_begin();
                size->setName("size");

                // Call malloc and return the result
                llvm::Value *result = builder->CreateCall(malloc_func, {size}, "malloc.result");
                builder->CreateRet(result);
            }
        }

        // ===== 3. Generate Alloc function (stack allocation) =====
        {
            // this is handled in CallExprSyntax so that it is inlined properly
        }

        // ===== 4. Generate Free function (heap deallocation) =====
        {
            // First, declare the external free function if not already declared
            llvm::Function *free_func = module->getFunction("free");
            if (!free_func)
            {
                // free signature: void free(void* ptr)
                std::vector<llvm::Type *> free_params;
                free_params.push_back(llvm::PointerType::get(*context, 0)); // void* (opaque pointer)

                auto free_type = llvm::FunctionType::get(
                    llvm::Type::getVoidTy(*context), // Returns void
                    free_params,
                    false // Not variadic
                );

                free_func = llvm::Function::Create(
                    free_type,
                    llvm::Function::ExternalLinkage,
                    "free",
                    module.get());
            }

            // Check if Free function already exists
            llvm::Function *Free_func = module->getFunction("Free_void_i8*");
            if (Free_func && Free_func->empty()) // Function declared but no body
            {
                // Create the function body
                current_function = Free_func;

                auto entry = llvm::BasicBlock::Create(*context, "entry", Free_func);
                builder->SetInsertPoint(entry);

                // Get the pointer parameter
                llvm::Value *ptr = Free_func->arg_begin();
                ptr->setName("ptr");

                // Call free
                builder->CreateCall(free_func, {ptr});

                // Return void
                builder->CreateRetVoid();
            }
        }

        // ===== 5. Generate Input function (console input) =====
        {
            // Declare scanf function
            llvm::Function *scanf_func = module->getFunction("scanf");
            if (!scanf_func)
            {
                // scanf signature: int scanf(const char* format, ...)
                std::vector<llvm::Type *> scanf_params;
                scanf_params.push_back(llvm::PointerType::get(*context, 0)); // const char* format

                auto scanf_type = llvm::FunctionType::get(
                    llvm::Type::getInt32Ty(*context), // Returns int
                    scanf_params,
                    true // Variadic function
                );

                scanf_func = llvm::Function::Create(
                    scanf_type,
                    llvm::Function::ExternalLinkage,
                    "scanf",
                    module.get());
            }

            // Declare strlen function for calculating the length
            llvm::Function *strlen_func = module->getFunction("strlen");
            if (!strlen_func)
            {
                // strlen signature: size_t strlen(const char* str)
                std::vector<llvm::Type *> strlen_params;
                strlen_params.push_back(llvm::PointerType::get(*context, 0)); // const char*

                auto strlen_type = llvm::FunctionType::get(
                    llvm::Type::getInt32Ty(*context), // Returns size_t (using i32)
                    strlen_params,
                    false // Not variadic
                );

                strlen_func = llvm::Function::Create(
                    strlen_type,
                    llvm::Function::ExternalLinkage,
                    "strlen",
                    module.get());
            }

            // Check if Input function already exists
            llvm::Function *input_func = module->getFunction("Input_i32_char*");
            if (input_func && input_func->empty()) // Function declared but no body
            {
                // Create the function body
                current_function = input_func;

                auto entry = llvm::BasicBlock::Create(*context, "entry", input_func);
                builder->SetInsertPoint(entry);

                // Get the buffer parameter
                llvm::Value *buffer = input_func->arg_begin();
                buffer->setName("buffer");

                // Create format string "%1023[^\n]" to read up to 1023 chars until newline
                // This prevents buffer overflow and stops at newline
                llvm::Constant *format_str = llvm::ConstantDataArray::getString(*context, "%1023[^\n]", true);
                llvm::GlobalVariable *format_global = new llvm::GlobalVariable(
                    *module,
                    format_str->getType(),
                    true, // Constant
                    llvm::GlobalValue::PrivateLinkage,
                    format_str,
                    ".str.scanf.format");

                llvm::Value *format_ptr = builder->CreatePointerCast(
                    format_global,
                    llvm::PointerType::get(*context, 0),
                    "format.ptr");

                // Call scanf with the format string and buffer
                llvm::Value *result = builder->CreateCall(
                    scanf_func,
                    {format_ptr, buffer},
                    "scanf.result");

                // scanf returns the number of successfully scanned items (should be 1 on success)
                llvm::Value *one = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), 1);
                llvm::Value *success = builder->CreateICmpEQ(result, one, "scanf.success");

                // Create format string "%*c" to consume the newline character left in the buffer
                llvm::Constant *consume_str = llvm::ConstantDataArray::getString(*context, "%*c", true);
                llvm::GlobalVariable *consume_global = new llvm::GlobalVariable(
                    *module,
                    consume_str->getType(),
                    true, // Constant
                    llvm::GlobalValue::PrivateLinkage,
                    consume_str,
                    ".str.consume.newline");

                llvm::Value *consume_ptr = builder->CreatePointerCast(
                    consume_global,
                    llvm::PointerType::get(*context, 0),
                    "consume.ptr");

                // Consume the newline character
                builder->CreateCall(scanf_func, {consume_ptr});

                // Create blocks for success/fail
                auto success_block = llvm::BasicBlock::Create(*context, "success", input_func);
                auto fail_block = llvm::BasicBlock::Create(*context, "fail", input_func);
                auto return_block = llvm::BasicBlock::Create(*context, "return", input_func);

                builder->CreateCondBr(success, success_block, fail_block);

                // Success block - get string length
                builder->SetInsertPoint(success_block);
                llvm::Value *length = builder->CreateCall(strlen_func, {buffer}, "length");
                builder->CreateBr(return_block);

                // Fail block - return 0
                builder->SetInsertPoint(fail_block);

                // Set first byte to null terminator on failure
                llvm::Value *zero_char = llvm::ConstantInt::get(llvm::Type::getInt8Ty(*context), 0);
                llvm::Value *first_byte = builder->CreateGEP(
                    llvm::Type::getInt8Ty(*context),
                    buffer,
                    llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), 0),
                    "first.byte.ptr");
                builder->CreateStore(zero_char, first_byte);

                llvm::Value *zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), 0);
                builder->CreateBr(return_block);

                // Return block - phi node to select return value
                builder->SetInsertPoint(return_block);
                llvm::PHINode *return_value = builder->CreatePHI(
                    llvm::Type::getInt32Ty(*context),
                    2,
                    "return.value");
                return_value->addIncoming(length, success_block);
                return_value->addIncoming(zero, fail_block);

                builder->CreateRet(return_value);
            }
        }

        // Restore the previous insertion point
        current_function = saved_function;
        if (saved_block)
        {
            builder->SetInsertPoint(saved_block);
        }
    }

    void CodeGenerator::debug_print_module_state(const std::string &phase)
    {
        std::cerr << "\n===== MODULE STATE: " << phase << " =====\n";

        // Print all functions in the module
        std::cerr << "Functions in module:\n";
        for (auto &F : module->functions())
        {
            std::cerr << "  Function: " << F.getName().str() << "\n";
            std::cerr << "    Type: ";
            F.getFunctionType()->print(llvm::errs());
            std::cerr << "\n";
            std::cerr << "    Num args declared: " << F.arg_size() << "\n";
            std::cerr << "    Return type: ";
            F.getReturnType()->print(llvm::errs());
            std::cerr << "\n";

            // Print argument types
            if (F.arg_size() > 0)
            {
                std::cerr << "    Arg types: ";
                for (size_t i = 0; i < F.getFunctionType()->getNumParams(); i++)
                {
                    F.getFunctionType()->getParamType(i)->print(llvm::errs());
                    std::cerr << " ";
                }
                std::cerr << "\n";
            }

            std::cerr << "    Has body: " << (!F.empty() ? "yes" : "no") << "\n";
        }

        // Print the entire module IR
        std::cerr << "\n--- Full Module IR ---\n";
        module->print(llvm::errs(), nullptr);
        std::cerr << "===== END MODULE STATE =====\n\n";
    }

    void CodeGenerator::generate_property_getter(PropertyDeclSyntax *prop_decl, TypeSymbol *type_symbol, llvm::StructType *struct_type)
    {
        if (!prop_decl || !prop_decl->variable || !prop_decl->variable->variable ||
            !prop_decl->variable->variable->name || !prop_decl->getter)
            return;

        std::string prop_name(prop_decl->variable->variable->name->text);

        // Find the property symbol
        auto prop_symbol = type_symbol->lookup_local(prop_name)->as<PropertySymbol>();
        if (!prop_symbol)
            return;

        std::string getter_name = prop_symbol->get_qualified_name() + ".get";

        auto getter_func = module->getFunction(getter_name);
        if (!getter_func || !getter_func->empty())
            return; // Already has a body or doesn't exist

        // Save current function context
        auto saved_function = current_function;
        auto saved_locals = locals;
        auto saved_local_types = local_types;

        current_function = getter_func;
        locals.clear();
        local_types.clear();

        // Create entry block
        auto entry = llvm::BasicBlock::Create(*context, "entry", getter_func);
        builder->SetInsertPoint(entry);

        // Set up 'this' parameter
        llvm::Value *this_param = getter_func->arg_begin();
        this_param->setName("this");

        // Make struct fields accessible as locals
        int field_index = 0;
        for (const auto &[member_name, member_node] : type_symbol->symbols)
        {
            if (auto field_sym = member_node->as<VariableSymbol>())
            {
                // Create a GEP to access this field through the 'this' pointer
                auto field_ptr = builder->CreateStructGEP(struct_type, this_param,
                                                          field_index, member_name);
                locals[field_sym] = field_ptr;
                local_types[field_sym] = get_llvm_type(field_sym->type());
                field_index++;
            }
        }

        // Generate the getter body
        if (auto expr = std::get_if<BaseExprSyntax *>(&prop_decl->getter->body))
        {
            (*expr)->accept(this);
            auto result = pop_value();
            if (result)
            {
                // Load the value if it's a pointer (but not for structs)
                auto prop_type = get_llvm_type((*expr)->resolvedType);
                if (result->getType()->isPointerTy() && prop_type && !prop_type->isStructTy())
                {
                    result = builder->CreateLoad(prop_type, result);
                }
                builder->CreateRet(result);
            }
        }
        else if (auto block = std::get_if<BlockSyntax *>(&prop_decl->getter->body))
        {
            (*block)->accept(this);
            ensure_terminator();
        }

        // Restore context
        current_function = saved_function;
        locals = saved_locals;
        local_types = saved_local_types;
    }

    Scope *CodeGenerator::get_containing_scope(BaseSyntax *node)
    {
        if (!node || node->containingScope.id == 0)
            return nullptr;
        return symbol_table.lookup_handle(node->containingScope)->as<Scope>();
    }

    Symbol *CodeGenerator::get_expression_symbol(BaseExprSyntax *expr)
    {
        if (!expr)
            return nullptr;

        // Use is() and as() to get the appropriate symbol
        if (expr->is<BaseNameExprSyntax>())
        {
            auto nameExpr = expr->as<BaseNameExprSyntax>();
            return nameExpr->resolvedSymbol.id != 0
                       ? symbol_table.lookup_handle(nameExpr->resolvedSymbol)->as<Symbol>()
                       : nullptr;
        }
        else if (expr->is<QualifiedNameSyntax>())
        {
            auto memberExpr = expr->as<QualifiedNameSyntax>();
            return memberExpr->resolvedMember.id != 0
                       ? symbol_table.lookup_handle(memberExpr->resolvedMember)->as<Symbol>()
                       : nullptr;
        }
        else if (expr->is<CallExprSyntax>())
        {
            auto callExpr = expr->as<CallExprSyntax>();
            return callExpr->resolvedCallee.id != 0
                       ? symbol_table.lookup_handle(callExpr->resolvedCallee)->as<Symbol>()
                       : nullptr;
        }

        return nullptr;
    }

    std::string CodeGenerator::build_qualified_name(BaseNameExprSyntax *name_expr)
    {
        if (!name_expr || !name_expr->name)
            return "";
        return std::string(name_expr->name->text);
    }

    void CodeGenerator::push_value(llvm::Value *val)
    {
        if (val)
        {
            value_stack.push(val);
        }
        else
        {
            report_general_error("Internal error: Attempted to push a null value to the stack");
        }
    }

    llvm::Value *CodeGenerator::pop_value()
    {
        if (value_stack.empty())
        {
            report_general_error("Internal error: Attempted to pop from an empty value stack");
            return nullptr;
        }
        auto val = value_stack.top();
        value_stack.pop();
        return val;
    }

    llvm::Type *CodeGenerator::get_llvm_type(TypePtr type)
    {
        if (!type)
        {
            // Default to void if a null type is encountered
            return llvm::Type::getVoidTy(*context);
        }

        // Check cache first
        auto it = type_cache.find(type);
        if (it != type_cache.end())
        {
            return it->second;
        }

        llvm::Type *llvm_type = nullptr;

        // Unresolved types should not reach code generation
        assert(!type->is<UnresolvedType>() && "Unresolved types should not reach code generation");

        if (auto prim = std::get_if<PrimitiveType>(&type->value))
        {
            switch (prim->kind)
            {
            case PrimitiveType::I8:
            case PrimitiveType::U8:
                llvm_type = llvm::Type::getInt8Ty(*context);
                break;
            case PrimitiveType::I16:
            case PrimitiveType::U16:
                llvm_type = llvm::Type::getInt16Ty(*context);
                break;
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
            case PrimitiveType::Char:
                llvm_type = llvm::Type::getInt8Ty(*context); // char is i8
                break;
            case PrimitiveType::Void:
                llvm_type = llvm::Type::getVoidTy(*context);
                break;
            default:
                report_general_error("Unsupported primitive type for codegen");
                llvm_type = llvm::Type::getVoidTy(*context);
                break;
            }
        }
        else if (auto arr = std::get_if<ArrayType>(&type->value))
        {
            // Arrays are just pointers to their element type
            llvm_type = llvm::PointerType::get(*context, 0);
        }
        else if (auto ptr = std::get_if<PointerType>(&type->value))
        {
            // All pointers are opaque in LLVM
            llvm_type = llvm::PointerType::get(*context, 0);
        }
        else if (auto ref = std::get_if<TypeReference>(&type->value))
        {
            if (ref->definition)
            {
                auto type_sym = ref->definition->as<TypeSymbol>();
                if (type_sym)
                {
                    auto defined_it = defined_types.find(type_sym);
                    if (defined_it != defined_types.end())
                    {
                        llvm_type = defined_it->second;
                    }
                    else
                    {
                        report_general_error("Internal error: Type '" + type_sym->name() +
                                             "' was not pre-declared");
                        llvm_type = llvm::Type::getVoidTy(*context);
                    }
                }
            }
        }
        else if (auto func = std::get_if<FunctionType>(&type->value))
        {
            // Function types are represented as function pointers
            std::vector<llvm::Type *> param_types;
            for (const auto &param_type : func->parameterTypes)
            {
                param_types.push_back(get_llvm_type(param_type));
            }

            llvm::Type *return_type = get_llvm_type(func->returnType);
            auto func_type = llvm::FunctionType::get(return_type, param_types, false);

            // Return pointer to function type
            llvm_type = llvm::PointerType::get(*context, 0);
        }
        else if (auto generic = std::get_if<GenericType>(&type->value))
        {
            // For now, generic types are not supported in code generation
            // They should be monomorphized before reaching this point
            report_general_error("Generic types should be monomorphized before code generation");
            llvm_type = llvm::Type::getVoidTy(*context);
        }
        else if (auto type_param = std::get_if<TypeParameter>(&type->value))
        {
            // Type parameters should be substituted before code generation
            report_general_error("Type parameter '" + type_param->name +
                                 "' should be substituted before code generation");
            llvm_type = llvm::Type::getVoidTy(*context);
        }
        else
        {
            report_general_error("Unsupported type kind for codegen");
            llvm_type = llvm::Type::getVoidTy(*context);
        }

        if (!llvm_type)
        {
            llvm_type = llvm::Type::getVoidTy(*context);
        }

        // Cache the result
        type_cache[type] = llvm_type;
        return llvm_type;
    }

    llvm::Value *CodeGenerator::create_constant(LiteralExprSyntax *literal)
    {
        if (!literal)
            return nullptr;

        std::string text(literal->value);

        switch (literal->kind)
        {
        case LiteralKind::I8:
            return llvm::ConstantInt::get(
                llvm::Type::getInt8Ty(*context),
                std::stoll(text),
                true // signed
            );

        case LiteralKind::U8:
            return llvm::ConstantInt::get(
                llvm::Type::getInt8Ty(*context),
                std::stoull(text),
                false // unsigned
            );

        case LiteralKind::I16:
            return llvm::ConstantInt::get(
                llvm::Type::getInt16Ty(*context),
                std::stoll(text),
                true // signed
            );

        case LiteralKind::U16:
            return llvm::ConstantInt::get(
                llvm::Type::getInt16Ty(*context),
                std::stoull(text),
                false // unsigned
            );

        case LiteralKind::I32:
            return llvm::ConstantInt::get(
                llvm::Type::getInt32Ty(*context),
                std::stoll(text),
                true // signed
            );

        case LiteralKind::U32:
            return llvm::ConstantInt::get(
                llvm::Type::getInt32Ty(*context),
                std::stoull(text),
                false // unsigned
            );

        case LiteralKind::I64:
            return llvm::ConstantInt::get(
                llvm::Type::getInt64Ty(*context),
                std::stoll(text),
                true // signed
            );

        case LiteralKind::U64:
            return llvm::ConstantInt::get(
                llvm::Type::getInt64Ty(*context),
                std::stoull(text),
                false // unsigned
            );

        case LiteralKind::F32:
            return llvm::ConstantFP::get(
                llvm::Type::getFloatTy(*context),
                std::stod(text));

        case LiteralKind::F64:
            return llvm::ConstantFP::get(
                llvm::Type::getDoubleTy(*context),
                std::stod(text));

        case LiteralKind::Bool:
            return llvm::ConstantInt::get(
                llvm::Type::getInt1Ty(*context),
                (text == "true") ? 1 : 0,
                false // unsigned
            );

        case LiteralKind::Char:
            // Handle character literals - extract the character value
            // Assuming format is 'c' or similar
            {
                char ch = text[0];
                if (text.length() > 1 && text[0] == '\'')
                {
                    ch = text[1]; // Skip the quote
                }
                return llvm::ConstantInt::get(
                    llvm::Type::getInt8Ty(*context),
                    static_cast<int8_t>(ch),
                    true // char is signed i8
                );
            }

        case LiteralKind::String:
            // String literals are handled specially in visit(LiteralExprSyntax)
            // This shouldn't be reached
            report_error(literal, "String literals should be handled in visit(LiteralExprSyntax)");
            return nullptr;

        default:
            report_error(literal, "Unsupported literal type");
            return nullptr;
        }
    }

    void CodeGenerator::ensure_terminator()
    {
        auto bb = builder->GetInsertBlock();
        if (bb && !bb->getTerminator())
        {
            if (current_function)
            {
                auto ret_type = current_function->getReturnType();
                if (ret_type->isVoidTy())
                {
                    // Void function - add implicit return
                    builder->CreateRetVoid();
                }
                else
                {
                    // Non-void function without explicit return
                    // This is technically undefined behavior
                    // Create unreachable to satisfy LLVM's requirements
                    builder->CreateUnreachable();
                }
            }
            else
            {
                // Not in a function context - shouldn't happen
                report_general_error("ensure_terminator called outside of function context");
            }
        }
    }

    void CodeGenerator::report_error(const BaseSyntax *node, const std::string &message)
    {
        errors.push_back({message, node ? node->location : SourceRange{}});
    }

    void CodeGenerator::report_general_error(const std::string &message)
    {
        errors.push_back({message, {}});
    }

    // === Visitor Implementations ===

    // --- Root and Declarations ---

    void CodeGenerator::visit(CompilationUnitSyntax *node)
    {
        if (!node)
            return;

        for (auto stmt : node->topLevelStatements)
        {
            if (stmt)
            {
                stmt->accept(this);
            }
        }
    }

    void CodeGenerator::visit(NamespaceDeclSyntax *node)
    {
        if (!node || !node->body)
            return;

        // Namespaces don't generate code directly, just visit their contents
        for (auto stmt : *node->body)
        {
            if (stmt)
            {
                stmt->accept(this);
            }
        }
    }

    void CodeGenerator::visit(TypeDeclSyntax *node)
    {
        if (!node || !node->name)
            return;

        // Find the type symbol
        auto scope = get_containing_scope(node);
        if (!scope)
            return;

        auto type_symbol = scope->lookup(node->name->text)->as<TypeSymbol>();
        if (!type_symbol)
            return;

        // Get the LLVM struct type
        auto type_it = defined_types.find(type_symbol);
        if (type_it == defined_types.end())
            return;

        llvm::StructType *struct_type = llvm::cast<llvm::StructType>(type_it->second);

        // Process method declarations to generate their bodies
        for (auto member : node->members)
        {
            if (auto func_decl = member->as<FunctionDeclSyntax>())
            {
                // Get the already-declared function
                auto func_symbol = symbol_table.lookup_handle(func_decl->functionSymbol)->as<FunctionSymbol>();
                if (!func_symbol)
                    continue;

                auto function = module->getFunction(func_symbol->get_mangled_name());
                if (!function || !function->empty())
                    continue; // Already has a body or doesn't exist

                // Save current context
                auto saved_function = current_function;
                auto saved_locals = locals;
                auto saved_local_types = local_types;

                current_function = function;
                locals.clear();
                local_types.clear();

                // Create entry block
                auto entry = llvm::BasicBlock::Create(*context, "entry", function);
                builder->SetInsertPoint(entry);

                // Check if this function should have a 'this' parameter
                // This depends on whether it was declared as a method (with 'this' parameter)
                bool is_method = false;
                llvm::Value *this_param = nullptr;

                // If the function has at least one argument and it's a method, first arg is 'this'
                if (function->arg_size() > 0)
                {
                    // Check if this function's parent (through FunctionGroupSymbol) is this TypeSymbol
                    bool parent_is_this_type = false;

                    if (func_symbol->parent)
                    {
                        // Direct parent is the type
                        if (func_symbol->parent == type_symbol)
                        {
                            parent_is_this_type = true;
                        }
                        // Parent is a FunctionGroupSymbol whose parent is the type
                        else if (auto func_group = func_symbol->parent->as<FunctionGroupSymbol>())
                        {
                            if (func_group->parent == type_symbol)
                            {
                                parent_is_this_type = true;
                            }
                        }
                    }

                    if (parent_is_this_type)
                    {
                        is_method = true;
                        this_param = function->arg_begin();
                        this_param->setName("this");
                    }
                }

                // Make struct fields accessible as locals through 'this' (if this is a method)
                if (is_method && this_param)
                {
                    int field_index = 0;
                    for (const auto &[member_name, member_node] : type_symbol->symbols)
                    {
                        if (auto field_sym = member_node->as<VariableSymbol>())
                        {
                            // Create GEP to access this field through the 'this' pointer
                            auto field_ptr = builder->CreateStructGEP(
                                struct_type,
                                this_param,
                                field_index,
                                member_name);
                            locals[field_sym] = field_ptr;
                            local_types[field_sym] = get_llvm_type(field_sym->type());
                            field_index++;
                        }
                    }
                }

                // Handle other parameters (starting from index 1 if method, 0 if static)
                size_t llvm_param_index = is_method ? 1 : 0;
                for (const auto &param_symbol : func_symbol->parameters())
                {
                    if (!param_symbol)
                        continue;

                    // Make sure we don't go out of bounds
                    if (llvm_param_index >= function->arg_size())
                    {
                        report_error(func_decl, "Parameter count mismatch");
                        break;
                    }

                    auto param_type = param_symbol->type();
                    auto llvm_param_type = get_llvm_type(param_type);
                    auto param_value = function->arg_begin() + llvm_param_index;
                    param_value->setName(param_symbol->name());

                    // Handle parameter storage based on type
                    auto storage_kind = param_type->get_storage_kind();

                    if (auto array_type = std::get_if<ArrayType>(&param_type->value))
                    {
                        // Arrays are passed as pointers
                        locals[param_symbol] = param_value;
                        local_types[param_symbol] = llvm_param_type;
                    }
                    else if (storage_kind == Type::StorageKind::Indirect)
                    {
                        // Reference type parameter
                        auto alloca = builder->CreateAlloca(
                            llvm::PointerType::get(*context, 0),
                            nullptr,
                            param_symbol->name());
                        builder->CreateStore(param_value, alloca);
                        locals[param_symbol] = alloca;
                        local_types[param_symbol] = llvm_param_type;
                    }
                    else
                    {
                        // Value type or explicit pointer
                        auto alloca = builder->CreateAlloca(llvm_param_type, nullptr, param_symbol->name());
                        builder->CreateStore(param_value, alloca);
                        locals[param_symbol] = alloca;
                        local_types[param_symbol] = llvm_param_type;
                    }

                    llvm_param_index++;
                }

                // Generate the method body
                func_decl->body->accept(this);
                ensure_terminator();

                // Restore context
                current_function = saved_function;
                locals = saved_locals;
                local_types = saved_local_types;
            }
            else if (auto prop_decl = member->as<PropertyDeclSyntax>())
            {
                // Generate getter function body
                if (prop_decl->getter)
                {
                    generate_property_getter(prop_decl, type_symbol, struct_type);
                }
                // TODO: Handle setters when needed
                if (prop_decl->setter)
                {
                    report_error(prop_decl->setter, "Setter generation not implemented");
                }
            }
        }
    }
    void CodeGenerator::visit(FunctionDeclSyntax *node)
    {
        if (!node || !node->name)
            return;

        // Skip if this is a method inside a type - it will be handled by visit(TypeDeclSyntax)
        auto parent_scope = get_containing_scope(node);
        if (parent_scope && parent_scope->scope_as<TypeSymbol>())
            return; // This is a method, handled elsewhere

        auto func_symbol = symbol_table.lookup_handle(node->functionSymbol)->as<FunctionSymbol>();
        if (!func_symbol)
        {
            report_error(node, "Function symbol not found for '" + std::string(node->name->text) + "'");
            return;
        }

        auto function = module->getFunction(func_symbol->get_mangled_name());
        if (!function)
        {
            report_error(node, "Function not found in module: '" + func_symbol->get_mangled_name() + "' for function: '" + func_symbol->get_qualified_name() + "'");
            return;
        }

        if (!function->empty())
            return; // Already has a body

        if (!node->body)
            return; // Abstract function, no body to generate

        // Save current state
        auto saved_function = current_function;
        auto saved_locals = locals;
        auto saved_local_types = local_types;

        current_function = function;
        locals.clear();
        local_types.clear();

        // Create entry block
        auto entry = llvm::BasicBlock::Create(*context, "entry", function);
        builder->SetInsertPoint(entry);

        // Set up parameters
        size_t param_index = 0;
        for (const auto &param_symbol : func_symbol->parameters())
        {
            if (!param_symbol)
            {
                report_error(node, "Parameter symbol not found for function '" + func_symbol->name() + "'");
                param_index++;
                continue;
            }

            TypePtr param_type = param_symbol->type();
            llvm::Type *llvm_param_type = get_llvm_type(param_type);
            auto param_value = function->arg_begin() + param_index;
            param_value->setName(param_symbol->name());

            // Handle parameter storage based on type
            auto storage_kind = param_type->get_storage_kind();

            if (auto array_type = std::get_if<ArrayType>(&param_type->value))
            {
                // Arrays are passed as pointers - store directly
                auto alloca = builder->CreateAlloca(
                    llvm::PointerType::get(*context, 0),
                    nullptr,
                    param_symbol->name());
                builder->CreateStore(param_value, alloca);
                locals[param_symbol] = alloca;
                local_types[param_symbol] = llvm_param_type;
            }
            else if (storage_kind == Type::StorageKind::Indirect)
            {
                // Reference type parameter - store the pointer
                auto alloca = builder->CreateAlloca(
                    llvm::PointerType::get(*context, 0),
                    nullptr,
                    param_symbol->name());
                builder->CreateStore(param_value, alloca);
                locals[param_symbol] = alloca;
                local_types[param_symbol] = llvm_param_type;
            }
            else
            {
                // Value type or explicit pointer - create alloca and store
                auto alloca = builder->CreateAlloca(llvm_param_type, nullptr, param_symbol->name());
                builder->CreateStore(param_value, alloca);
                locals[param_symbol] = alloca;
                local_types[param_symbol] = llvm_param_type;
            }

            param_index++;
        }

        // Generate function body
        node->body->accept(this);

        // Ensure terminator
        ensure_terminator();

        // Restore previous state
        current_function = saved_function;
        locals = saved_locals;
        local_types = saved_local_types;
    }

    void CodeGenerator::visit(VariableDeclSyntax *node)
    {
        if (!node || !node->variable || !node->variable->name)
            return;

        auto parent_scope = get_containing_scope(node);
        if (!parent_scope)
            return;

        auto var_symbol = parent_scope->lookup(node->variable->name->text);
        if (!var_symbol)
        {
            report_error(node, "Variable symbol not found for '" + std::string(node->variable->name->text) + "'");
            return;
        }

        auto typed_symbol = var_symbol->as<TypedSymbol>();
        if (!typed_symbol)
            return;

        TypePtr var_type = typed_symbol->type();
        llvm::Type *llvm_type = get_llvm_type(var_type);

        if (llvm_type->isVoidTy())
        {
            report_error(node, "Cannot declare a variable of type 'void'");
            return;
        }

        // Create storage for the variable based on its storage kind
        llvm::Value *alloca = nullptr;

        auto storageKind = var_type->get_storage_kind();
        if (auto array_type = std::get_if<ArrayType>(&var_type->value))
        {
            // Arrays are stored as pointers
            alloca = builder->CreateAlloca(
                llvm::PointerType::get(*context, 0),
                nullptr,
                node->variable->name->text);
        }
        else if (storageKind == Type::StorageKind::Indirect)
        {
            // Reference type: allocate space for a pointer
            alloca = builder->CreateAlloca(
                llvm::PointerType::get(*context, 0),
                nullptr,
                node->variable->name->text);
        }
        else
        {
            // Value type or explicit pointer: allocate space for the actual type
            alloca = builder->CreateAlloca(llvm_type, nullptr, node->variable->name->text);
        }

        locals[var_symbol] = alloca;
        local_types[var_symbol] = llvm_type;

        // Handle initialization
        if (node->initializer)
        {
            auto init_value = genRValue(node->initializer);
            if (init_value)
            {
                storeValue(init_value, alloca, var_type);
            }
        }
        else if (storageKind == Type::StorageKind::Indirect)
        {
            // Reference types without initializer should be null
            auto null_ptr = llvm::ConstantPointerNull::get(
                llvm::PointerType::get(*context, 0));
            builder->CreateStore(null_ptr, alloca);
        }
    }

    void CodeGenerator::visit(PropertyDeclSyntax *node)
    {
        // Properties are handled in visit(TypeDeclSyntax) where we have access to the type context
        // This visitor is for standalone property declarations, which shouldn't occur
        // at the statement level in the current language design
        if (node)
        {
            report_error(node, "Standalone property declarations are not supported");
        }
    }

    void CodeGenerator::visit(ParameterDeclSyntax *node)
    {
        // Parameters are handled in visit(FunctionDeclSyntax)
        // This visitor shouldn't be called directly
        if (node)
        {
            report_error(node, "Parameter declarations should be handled by function declaration");
        }
    }

    // --- Statements ---

    void CodeGenerator::visit(BlockSyntax *node)
    {
        if (!node)
            return;

        for (auto stmt : node->statements)
        {
            if (stmt)
            {
                stmt->accept(this);
            }
        }
    }

    void CodeGenerator::visit(ExpressionStmtSyntax *node)
    {
        if (!node || !node->expression)
            return;

        // Evaluate the expression
        node->expression->accept(this);

        // Pop and discard the result if there is one
        // Expression statements are evaluated for their side effects
        if (!value_stack.empty())
        {
            pop_value();
        }
    }

    void CodeGenerator::visit(ReturnStmtSyntax *node)
    {
        if (!node)
            return;

        if (node->value)
        {
            // Return statement needs the value, not the address
            auto ret_value = genRValue(node->value);

            if (ret_value)
            {
                builder->CreateRet(ret_value);
            }
            else
            {
                report_error(node, "Failed to generate return value");
                // Create unreachable to satisfy LLVM's requirement for terminator
                builder->CreateUnreachable();
            }
        }
        else
        {
            // Void return
            builder->CreateRetVoid();
        }
    }

    // --- Expressions ---

    void CodeGenerator::visit(BinaryExprSyntax *node)
    {
        if (!node || !node->left || !node->right)
            return;

        // Binary operations always need values, not addresses
        auto left = genRValue(node->left);
        if (!left)
            return;

        auto right = genRValue(node->right);
        if (!right)
            return;

        // Determine if we're dealing with floating point or unsigned operations
        bool is_float = left->getType()->isFloatingPointTy();
        bool is_unsigned = isUnsignedType(node->left->resolvedType);

        llvm::Value *result = nullptr;

        switch (node->op)
        {
        // Arithmetic operations
        case BinaryOperatorKind::Add:
            result = is_float ? builder->CreateFAdd(left, right, "fadd")
                              : builder->CreateAdd(left, right, "add");
            break;

        case BinaryOperatorKind::Subtract:
            result = is_float ? builder->CreateFSub(left, right, "fsub")
                              : builder->CreateSub(left, right, "sub");
            break;

        case BinaryOperatorKind::Multiply:
            result = is_float ? builder->CreateFMul(left, right, "fmul")
                              : builder->CreateMul(left, right, "mul");
            break;

        case BinaryOperatorKind::Divide:
            if (is_float)
            {
                result = builder->CreateFDiv(left, right, "fdiv");
            }
            else if (is_unsigned)
            {
                result = builder->CreateUDiv(left, right, "udiv");
            }
            else
            {
                result = builder->CreateSDiv(left, right, "sdiv");
            }
            break;

        case BinaryOperatorKind::Modulo:
            if (is_float)
            {
                result = builder->CreateFRem(left, right, "frem");
            }
            else if (is_unsigned)
            {
                result = builder->CreateURem(left, right, "urem");
            }
            else
            {
                result = builder->CreateSRem(left, right, "srem");
            }
            break;

        // Comparison operations
        case BinaryOperatorKind::Equals:
            result = is_float ? builder->CreateFCmpOEQ(left, right, "fcmp.eq")
                              : builder->CreateICmpEQ(left, right, "icmp.eq");
            break;

        case BinaryOperatorKind::NotEquals:
            result = is_float ? builder->CreateFCmpONE(left, right, "fcmp.ne")
                              : builder->CreateICmpNE(left, right, "icmp.ne");
            break;

        case BinaryOperatorKind::LessThan:
            if (is_float)
            {
                result = builder->CreateFCmpOLT(left, right, "fcmp.lt");
            }
            else if (is_unsigned)
            {
                result = builder->CreateICmpULT(left, right, "icmp.ult");
            }
            else
            {
                result = builder->CreateICmpSLT(left, right, "icmp.slt");
            }
            break;

        case BinaryOperatorKind::LessThanOrEqual:
            if (is_float)
            {
                result = builder->CreateFCmpOLE(left, right, "fcmp.le");
            }
            else if (is_unsigned)
            {
                result = builder->CreateICmpULE(left, right, "icmp.ule");
            }
            else
            {
                result = builder->CreateICmpSLE(left, right, "icmp.sle");
            }
            break;

        case BinaryOperatorKind::GreaterThan:
            if (is_float)
            {
                result = builder->CreateFCmpOGT(left, right, "fcmp.gt");
            }
            else if (is_unsigned)
            {
                result = builder->CreateICmpUGT(left, right, "icmp.ugt");
            }
            else
            {
                result = builder->CreateICmpSGT(left, right, "icmp.sgt");
            }
            break;

        case BinaryOperatorKind::GreaterThanOrEqual:
            if (is_float)
            {
                result = builder->CreateFCmpOGE(left, right, "fcmp.ge");
            }
            else if (is_unsigned)
            {
                result = builder->CreateICmpUGE(left, right, "icmp.uge");
            }
            else
            {
                result = builder->CreateICmpSGE(left, right, "icmp.sge");
            }
            break;

        // Logical operations
        case BinaryOperatorKind::LogicalAnd:
            result = builder->CreateAnd(left, right, "and");
            break;

        case BinaryOperatorKind::LogicalOr:
            result = builder->CreateOr(left, right, "or");
            break;

        // Bitwise operations
        case BinaryOperatorKind::BitwiseAnd:
            result = builder->CreateAnd(left, right, "bitand");
            break;

        case BinaryOperatorKind::BitwiseOr:
            result = builder->CreateOr(left, right, "bitor");
            break;

        case BinaryOperatorKind::BitwiseXor:
            result = builder->CreateXor(left, right, "bitxor");
            break;

        case BinaryOperatorKind::LeftShift:
            result = builder->CreateShl(left, right, "shl");
            break;

        case BinaryOperatorKind::RightShift:
            if (is_unsigned)
            {
                result = builder->CreateLShr(left, right, "lshr");
            }
            else
            {
                result = builder->CreateAShr(left, right, "ashr");
            }
            break;

        default:
            report_error(node, "Unsupported binary operator");
            return;
        }

        if (result)
        {
            push_value(result);
        }
    }

    void CodeGenerator::visit(UnaryExprSyntax *node)
    {
        if (!node || !node->operand)
            return;

        llvm::Value *result = nullptr;

        // Handle increment/decrement operators specially (they modify the operand)
        if (node->op == UnaryOperatorKind::PostIncrement ||
            node->op == UnaryOperatorKind::PreIncrement ||
            node->op == UnaryOperatorKind::PostDecrement ||
            node->op == UnaryOperatorKind::PreDecrement)
        {
            // For increment/decrement, we need the operand as an lvalue
            if (!node->operand->is_l_value())
            {
                report_error(node, "Increment/decrement operators require an lvalue");
                return;
            }

            auto operand_ptr = genLValue(node->operand);
            if (!operand_ptr)
                return;

            auto operand_type = node->operand->resolvedType;
            if (!operand_type)
                return;

            // Load the current value
            auto current_value = loadValue(operand_ptr, operand_type);
            if (!current_value)
                return;

            // Create the increment/decrement value
            llvm::Value *one = nullptr;
            auto llvm_type = current_value->getType();

            if (llvm_type->isFloatingPointTy())
            {
                one = llvm::ConstantFP::get(llvm_type, 1.0);
            }
            else
            {
                one = llvm::ConstantInt::get(llvm_type, 1);
            }

            // Calculate new value
            llvm::Value *new_value = nullptr;
            if (node->op == UnaryOperatorKind::PostIncrement ||
                node->op == UnaryOperatorKind::PreIncrement)
            {
                new_value = llvm_type->isFloatingPointTy()
                                ? builder->CreateFAdd(current_value, one, "inc")
                                : builder->CreateAdd(current_value, one, "inc");
            }
            else
            {
                new_value = llvm_type->isFloatingPointTy()
                                ? builder->CreateFSub(current_value, one, "dec")
                                : builder->CreateSub(current_value, one, "dec");
            }

            // Store the new value
            storeValue(new_value, operand_ptr, operand_type);

            // For post-increment/decrement, return the old value
            // For pre-increment/decrement, return the new value
            if (node->op == UnaryOperatorKind::PostIncrement ||
                node->op == UnaryOperatorKind::PostDecrement)
            {
                result = current_value;
            }
            else
            {
                result = new_value;
            }
        }
        else
        {
            // Regular unary operators that don't modify operands

            // Most unary operators need the value
            llvm::Value *operand = nullptr;

            if (node->op == UnaryOperatorKind::AddressOf)
            {
                // Address-of operator needs lvalue and returns its address
                if (!node->operand->is_l_value())
                {
                    report_error(node, "Cannot take address of rvalue");
                    return;
                }
                operand = genLValue(node->operand);
                result = operand; // The address itself is the result
            }
            else if (node->op == UnaryOperatorKind::Dereference)
            {
                // Dereference operator loads the pointer value
                operand = genRValue(node->operand);
                if (!operand)
                    return;

                // The result is the dereferenced pointer (as an lvalue address)
                // This allows *ptr = value to work
                result = operand;

                // Note: The result of dereference should be marked as lvalue
                // in the type resolver, so parent nodes know they can assign to it
            }
            else
            {
                // All other unary operators need the value
                operand = genRValue(node->operand);
                if (!operand)
                    return;

                auto operand_type = node->operand->resolvedType;
                bool is_unsigned = isUnsignedType(operand_type);

                switch (node->op)
                {
                case UnaryOperatorKind::Plus:
                    // Unary plus is a no-op
                    result = operand;
                    break;

                case UnaryOperatorKind::Minus:
                    result = operand->getType()->isFloatingPointTy()
                                 ? builder->CreateFNeg(operand, "fneg")
                                 : builder->CreateNeg(operand, "neg");
                    break;

                case UnaryOperatorKind::Not:
                    // Logical not
                    result = builder->CreateNot(operand, "not");
                    break;

                case UnaryOperatorKind::BitwiseNot:
                    // Bitwise complement
                    result = builder->CreateNot(operand, "bitnot");
                    break;

                default:
                    report_error(node, "Unsupported unary operator");
                    return;
                }
            }
        }

        if (result)
        {
            push_value(result);
        }
    }

    void CodeGenerator::visit(AssignmentExprSyntax *node)
    {
        if (!node || !node->target || !node->value)
            return;

        // Get the target address (must be lvalue)
        auto target_addr = genLValue(node->target);
        if (!target_addr)
        {
            report_error(node->target, "Assignment target must be an lvalue");
            return;
        }

        auto target_type = node->target->resolvedType;
        if (!target_type)
        {
            report_error(node, "Target type not resolved");
            return;
        }

        llvm::Value *final_value = nullptr;

        if (node->op == AssignmentOperatorKind::Assign)
        {
            // Simple assignment: target = value
            final_value = genRValue(node->value);
            if (!final_value)
                return;
        }
        else
        {
            // Compound assignment: target op= value
            // Load current value
            auto current_value = loadValue(target_addr, target_type);
            if (!current_value)
                return;

            // Get the RHS value
            auto rhs_value = genRValue(node->value);
            if (!rhs_value)
                return;

            // Perform the operation
            bool is_float = current_value->getType()->isFloatingPointTy();
            bool is_unsigned = isUnsignedType(target_type);

            switch (node->op)
            {
            case AssignmentOperatorKind::Add:
                final_value = is_float ? builder->CreateFAdd(current_value, rhs_value, "add.assign")
                                       : builder->CreateAdd(current_value, rhs_value, "add.assign");
                break;

            case AssignmentOperatorKind::Subtract:
                final_value = is_float ? builder->CreateFSub(current_value, rhs_value, "sub.assign")
                                       : builder->CreateSub(current_value, rhs_value, "sub.assign");
                break;

            case AssignmentOperatorKind::Multiply:
                final_value = is_float ? builder->CreateFMul(current_value, rhs_value, "mul.assign")
                                       : builder->CreateMul(current_value, rhs_value, "mul.assign");
                break;

            case AssignmentOperatorKind::Divide:
                if (is_float)
                {
                    final_value = builder->CreateFDiv(current_value, rhs_value, "div.assign");
                }
                else if (is_unsigned)
                {
                    final_value = builder->CreateUDiv(current_value, rhs_value, "udiv.assign");
                }
                else
                {
                    final_value = builder->CreateSDiv(current_value, rhs_value, "sdiv.assign");
                }
                break;

            case AssignmentOperatorKind::Modulo:
                if (is_float)
                {
                    final_value = builder->CreateFRem(current_value, rhs_value, "fmod.assign");
                }
                else if (is_unsigned)
                {
                    final_value = builder->CreateURem(current_value, rhs_value, "umod.assign");
                }
                else
                {
                    final_value = builder->CreateSRem(current_value, rhs_value, "smod.assign");
                }
                break;

            case AssignmentOperatorKind::And:
                final_value = builder->CreateAnd(current_value, rhs_value, "and.assign");
                break;

            case AssignmentOperatorKind::Or:
                final_value = builder->CreateOr(current_value, rhs_value, "or.assign");
                break;

            case AssignmentOperatorKind::Xor:
                final_value = builder->CreateXor(current_value, rhs_value, "xor.assign");
                break;

            case AssignmentOperatorKind::LeftShift:
                final_value = builder->CreateShl(current_value, rhs_value, "shl.assign");
                break;

            case AssignmentOperatorKind::RightShift:
                if (is_unsigned)
                {
                    final_value = builder->CreateLShr(current_value, rhs_value, "lshr.assign");
                }
                else
                {
                    final_value = builder->CreateAShr(current_value, rhs_value, "ashr.assign");
                }
                break;

            default:
                report_error(node, "Unsupported compound assignment operator");
                return;
            }
        }

        if (final_value)
        {
            // Store the value
            storeValue(final_value, target_addr, target_type);

            // Assignment expressions return the assigned value
            push_value(final_value);
        }
    }

    void CodeGenerator::visit(CallExprSyntax *node)
    {
        if (!node || !node->callee)
            return;

        llvm::Function *callee_func = nullptr;
        llvm::Value *this_ptr = nullptr;

        // Handle method calls (callee is MemberAccessExpr)
        if (auto member_expr = node->callee->as<QualifiedNameSyntax>())
        {
            // Get the object (this pointer)
            // For method calls, we need the address of the object
            if (member_expr->object->is_l_value())
            {
                this_ptr = genLValue(member_expr->object);
            }
            else
            {
                // Rvalue object - need to create temporary storage
                auto obj_value = genRValue(member_expr->object);
                this_ptr = ensureAddress(obj_value, member_expr->object->resolvedType);
            }

            // Get the method name and look up the function
            auto obj_type = member_expr->object->resolvedType;
            if (auto type_ref = std::get_if<TypeReference>(&obj_type->value))
            {
                if (auto type_sym = type_ref->definition->as<TypeSymbol>())
                {
                    auto callee_symbol = symbol_table.lookup_handle(node->resolvedCallee)->as<FunctionSymbol>();
                    if (!callee_symbol)
                    {
                        report_error(node->callee, "Unknown method referenced: " + std::string(member_expr->member->text));
                        return;
                    }

                    auto method_name = callee_symbol->get_mangled_name();
                    callee_func = module->getFunction(method_name);
                    if (!callee_func)
                    {
                        report_error(node->callee, "LLVM function not found: '" + method_name + "'");
                        return;
                    }
                }
            }

            if (!callee_func)
            {
                report_error(node->callee, "Could not resolve method call");
                return;
            }
        }
        // Handle regular function calls (callee is BaseNameExprSyntax)
        else if (auto name_expr = node->callee->as<BaseNameExprSyntax>())
        {
            std::string func_name = build_qualified_name(name_expr);

            // Special handling for Alloc function (inline implementation)
            if (func_name == "Alloc")
            {
                if (node->arguments.size() != 1)
                {
                    report_error(node, "Alloc requires exactly one argument (size)");
                    return;
                }

                // Evaluate the size argument
                auto size_value = genRValue(node->arguments[0]);
                if (!size_value)
                    return;

                // Generate alloca instruction directly in the current function
                llvm::Type *i8_type = llvm::Type::getInt8Ty(*context);
                llvm::AllocaInst *alloca_inst = builder->CreateAlloca(i8_type, size_value, "stack.alloc");

                // Set alignment for better performance
                alloca_inst->setAlignment(llvm::Align(8));

                // Push the result pointer
                push_value(alloca_inst);
                return;
            }

            auto callee_symbol = symbol_table.lookup_handle(node->resolvedCallee)->as<FunctionSymbol>();
            if (!callee_symbol)
            {
                report_error(node->callee, "Unknown function referenced: " + func_name);
                return;
            }

            func_name = callee_symbol->get_mangled_name();
            callee_func = module->getFunction(func_name);

            // If not found as a global function, check if it's an implicit method call
            if (!callee_func && current_function)
            {
                // Check if we're inside a method (function has 'this' as first parameter)
                if (current_function->arg_size() > 0)
                {
                    auto first_param = current_function->arg_begin();
                    if (first_param->getName() == "this")
                    {
                        // We're in a method - try to find the function as a method of the same type
                        std::string current_func_name = current_function->getName().str();
                        size_t dot_pos = current_func_name.find('.');
                        if (dot_pos != std::string::npos)
                        {
                            std::string type_name = current_func_name.substr(0, dot_pos);
                            std::string method_name = type_name + "." + func_name;

                            callee_func = module->getFunction(method_name);
                            if (callee_func)
                            {
                                // Use the current function's 'this' parameter
                                this_ptr = first_param;
                            }
                        }
                    }
                }
            }

            if (!callee_func)
            {
                report_error(node->callee, "Unknown LLVM function referenced: " + func_name);
                return;
            }
        }
        else
        {
            report_error(node->callee, "Function call target must be an identifier or member access");
            return;
        }

        // Collect arguments
        std::vector<llvm::Value *> args;

        // For method calls, add 'this' as the first argument
        if (this_ptr)
        {
            args.push_back(this_ptr);
        }

        // Add regular arguments - all need to be values
        for (size_t i = 0; i < node->arguments.size(); i++)
        {
            auto arg = node->arguments[i];
            auto arg_type = arg->resolvedType;

            if (!arg_type)
            {
                report_error(arg, "Argument type not resolved");
                return;
            }

            // Get the argument value
            auto arg_value = genRValue(arg);
            if (!arg_value)
                return;

            // For arrays, just pass the pointer to the first element
            // During bootstrap, we keep array handling simple
            if (auto array_type = std::get_if<ArrayType>(&arg_type->value))
            {
                // Arrays are passed as pointers - ensure we have an address
                if (!arg_value->getType()->isPointerTy())
                {
                    arg_value = ensureAddress(arg_value, arg_type);
                }
            }

            args.push_back(arg_value);
        }

        // Verify argument count
        if (args.size() != callee_func->arg_size())
        {
            report_error(node, "Incorrect number of arguments for function '" +
                                   callee_func->getName().str() + "': expected " +
                                   std::to_string(callee_func->arg_size()) +
                                   ", got " + std::to_string(args.size()));
            return;
        }

        // Verify argument types
        for (size_t i = 0; i < args.size(); ++i)
        {
            if (args[i]->getType() != callee_func->getArg(i)->getType())
            {
                std::string arg_type_str;
                std::string param_type_str;

                llvm::raw_string_ostream arg_stream(arg_type_str);
                llvm::raw_string_ostream param_stream(param_type_str);

                args[i]->getType()->print(arg_stream);
                callee_func->getArg(i)->getType()->print(param_stream);

                report_error(node, "Type mismatch for argument " + std::to_string(i) +
                                       ": got '" + arg_stream.str() +
                                       "', expected '" + param_stream.str() + "'");
                return;
            }
        }

        // Make the call
        auto call_value = builder->CreateCall(callee_func, args,
                                              callee_func->getReturnType()->isVoidTy() ? "" : "call");

        // Push result if non-void
        if (!callee_func->getReturnType()->isVoidTy())
        {
            push_value(call_value);
        }
    }

    void CodeGenerator::visit(BaseNameExprSyntax *node)
    {
        if (!node || !node->name)
            return;

        auto parent_scope = get_containing_scope(node);
        if (!parent_scope)
            return;

        auto var_name = build_qualified_name(node);
        auto var_symbol = parent_scope->lookup(var_name);
        if (!var_symbol)
        {
            report_error(node, "Identifier not found: " + var_name);
            return;
        }

        // Check if this is a property reference
        if (auto prop_symbol = var_symbol->as<PropertySymbol>())
        {
            // This is a property access - we need to call the getter
            // Check if we're inside a method and have access to 'this'
            if (current_function && current_function->arg_size() > 0)
            {
                auto first_param = current_function->arg_begin();
                if (first_param->getName() == "this")
                {
                    // We're in a method context - call the property getter with 'this'
                    std::string getter_name = prop_symbol->get_qualified_name() + ".get";
                    auto getter_func = module->getFunction(getter_name);
                    if (!getter_func)
                    {
                        report_error(node, "Property getter not found: " + getter_name);
                        return;
                    }

                    // Call getter with 'this' pointer as argument
                    auto result = builder->CreateCall(getter_func, {first_param}, "prop.get");

                    // Property getters return values directly
                    // The result is an rvalue
                    push_value(result);
                    return;
                }
            }

            report_error(node, "Property access outside of method context not supported");
            return;
        }

        // Handle regular variables
        auto it = locals.find(var_symbol);
        if (it == locals.end())
        {
            report_error(node, "Variable not found in local scope: " + var_name);
            return;
        }

        auto alloca = it->second;

        // For lvalue expressions, we push the address
        // The parent node will decide whether to load the value
        // This is critical for allowing assignments like: x = 5
        push_value(alloca);
    }

    void CodeGenerator::visit(LiteralExprSyntax *node)
    {
        if (!node)
            return;

        if (node->kind == LiteralKind::String)
        {
            // String literals are special - they become i8* pointers
            std::string text(node->value);

            // Create a global constant for the string
            auto string_constant = llvm::ConstantDataArray::getString(*context, text);

            // Create a global variable to hold the string
            auto global_str = new llvm::GlobalVariable(
                *module,
                string_constant->getType(),
                true, // isConstant
                llvm::GlobalValue::PrivateLinkage,
                string_constant,
                ".str");

            // Get pointer to the first element (i8*)
            std::vector<llvm::Value *> indices = {
                llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), 0),
                llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), 0)};

            auto string_ptr = builder->CreateInBoundsGEP(
                string_constant->getType(),
                global_str,
                indices,
                "str.ptr");

            // String literals are rvalues - push the pointer value directly
            push_value(string_ptr);
        }
        else
        {
            // Create the constant value
            auto constant = create_constant(node);
            if (constant)
            {
                // Literals are rvalues - push the value directly
                push_value(constant);
            }
        }
    }

    void CodeGenerator::visit(IdentifierNameSyntax *node)
    {
        // Identifiers are typically handled by BaseNameExprSyntax which contains identifier parts.
        // Individual identifier nodes usually don't generate code directly.
    }

    void CodeGenerator::visit(NewExprSyntax *node)
    {
        if (!node || !node->resolvedType)
            return;

        llvm::Type *llvm_type = get_llvm_type(node->resolvedType);
        if (!llvm_type)
        {
            report_error(node, "Failed to get LLVM type for 'new' expression");
            return;
        }

        // Determine storage based on type system
        auto storage_kind = node->resolvedType->get_storage_kind();

        llvm::Value *object_ptr = nullptr;

        if (auto array_type = std::get_if<ArrayType>(&node->resolvedType->value))
        {
            llvm::Type *element_type = get_llvm_type(array_type->elementType);
            if (!element_type)
            {
                report_error(node, "Failed to get element type for array allocation");
                return;
            }

            // Determine array size
            llvm::Value *array_size = nullptr;

            // Check if size is specified in the type (fixed size array)
            if (array_type->fixedSize > 0)
            {
                array_size = llvm::ConstantInt::get(
                    llvm::Type::getInt32Ty(*context),
                    array_type->fixedSize);
            }
            // Otherwise, size should be provided as the first argument
            else if (!node->arguments.empty())
            {
                array_size = genRValue(node->arguments[0]);
                if (!array_size)
                {
                    report_error(node, "Failed to evaluate array size expression");
                    return;
                }
            }
            else
            {
                report_error(node, "Array allocation requires a size");
                return;
            }

            // Calculate total size in bytes
            llvm::DataLayout dl = module->getDataLayout();
            uint64_t element_size_bytes = dl.getTypeAllocSize(element_type);
            auto element_size = llvm::ConstantInt::get(
                llvm::Type::getInt32Ty(*context),
                element_size_bytes);

            // Total size = element_size * array_size
            auto total_size = builder->CreateMul(element_size, array_size, "array.size");

            if (storage_kind == Type::StorageKind::Indirect)
            {
                // Reference type: allocate on heap using malloc
                auto malloc_func = module->getFunction("malloc");
                if (!malloc_func)
                {
                    // Declare malloc if not already declared
                    std::vector<llvm::Type *> malloc_params;
                    malloc_params.push_back(llvm::Type::getInt32Ty(*context));

                    auto malloc_type = llvm::FunctionType::get(
                        llvm::PointerType::get(*context, 0),
                        malloc_params,
                        false);

                    malloc_func = llvm::Function::Create(
                        malloc_type,
                        llvm::Function::ExternalLinkage,
                        "malloc",
                        module.get());
                }

                object_ptr = builder->CreateCall(malloc_func, {total_size}, "array.heap.alloc");
            }
            else
            {
                // Value type: allocate on stack using alloca
                // For arrays, we allocate the elements directly
                object_ptr = builder->CreateAlloca(element_type, array_size, "array.stack.alloc");
            }

            // Optionally zero-initialize the array
            builder->CreateMemSet(
                object_ptr,
                llvm::ConstantInt::get(builder->getInt8Ty(), 0),
                total_size,
                llvm::MaybeAlign(1));

            // Return the pointer to the array
            push_value(object_ptr);
            return;
        }

        if (!llvm_type->isStructTy())
        {
            report_error(node, "'new' can only be used with user-defined struct types or arrays");
            return;
        }

        if (storage_kind == Type::StorageKind::Indirect)
        {
            // Reference type: allocate on heap using malloc
            // Calculate size of the struct
            llvm::DataLayout dl = module->getDataLayout();
            uint64_t type_size_bytes = dl.getTypeAllocSize(llvm_type);
            auto size = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), type_size_bytes);

            // Call malloc
            auto malloc_func = module->getFunction("malloc");
            if (!malloc_func)
            {
                // Declare malloc if not already declared
                std::vector<llvm::Type *> malloc_params;
                malloc_params.push_back(llvm::Type::getInt32Ty(*context));

                auto malloc_type = llvm::FunctionType::get(
                    llvm::PointerType::get(*context, 0),
                    malloc_params,
                    false);

                malloc_func = llvm::Function::Create(
                    malloc_type,
                    llvm::Function::ExternalLinkage,
                    "malloc",
                    module.get());
            }

            object_ptr = builder->CreateCall(malloc_func, {size}, "heap.alloc");
        }
        else
        {
            // Value type: create temporary on stack
            object_ptr = builder->CreateAlloca(llvm_type, nullptr, "new.tmp");
        }

        // Initialize the struct
        if (node->arguments.size() > 0)
        {
            // Constructor-style initialization with arguments
            if (auto type_ref = std::get_if<TypeReference>(&node->resolvedType->value))
            {
                if (auto type_sym = type_ref->definition->as<TypeSymbol>())
                {
                    // Process each argument and assign to corresponding field
                    int field_index = 0;
                    for (auto arg : node->arguments)
                    {
                        if (field_index >= static_cast<int>(llvm_type->getStructNumElements()))
                        {
                            report_error(node, "Too many constructor arguments");
                            break;
                        }

                        // Get the argument value
                        auto arg_value = genRValue(arg);
                        if (!arg_value)
                            continue;

                        // Get pointer to the field
                        auto field_ptr = builder->CreateStructGEP(
                            llvm_type,
                            object_ptr,
                            field_index,
                            "field.init");

                        // Store the value in the field
                        builder->CreateStore(arg_value, field_ptr);
                        field_index++;
                    }
                }
            }
        }
        else
        {
            // Zero-initialize if no arguments
            llvm::DataLayout dl = module->getDataLayout();
            uint64_t type_size = dl.getTypeAllocSize(llvm_type);

            builder->CreateMemSet(
                object_ptr,
                llvm::ConstantInt::get(builder->getInt8Ty(), 0),
                type_size,
                llvm::MaybeAlign(dl.getPrefTypeAlign(llvm_type)));
        }

        // For reference types, the pointer itself is the value
        // For value types, we return the pointer to the temporary
        push_value(object_ptr);
    }

    // --- Base/Error/Unimplemented Visitors ---

    void CodeGenerator::visit(BaseSyntax *node) {}
    void CodeGenerator::visit(BaseExprSyntax *node) { report_error(node, "Codegen for this expression type is not yet implemented."); }
    void CodeGenerator::visit(BaseStmtSyntax *node) { report_error(node, "Codegen for this statement type is not yet implemented."); }
    void CodeGenerator::visit(BaseDeclSyntax *node) { report_error(node, "Codegen for this declaration type is not yet implemented."); }
    void CodeGenerator::visit(MissingSyntax *node)
    {
        if (node)
        {
            report_error(node, "Error expression: " + std::string(node->message));
        }
    }

    void CodeGenerator::visit(MissingSyntax *node)
    {
        if (node)
        {
            report_error(node, "Error statement: " + std::string(node->message));
        }
    }

    void CodeGenerator::visit(ArrayLiteralExprSyntax *node)
    {
        if (!node || !node->resolvedType)
        {
            report_error(node, "Array literal has no resolved type");
            return;
        }

        auto array_type = std::get_if<ArrayType>(&node->resolvedType->value);
        if (!array_type)
        {
            report_error(node, "Array literal's resolved type is not an array type");
            return;
        }

        llvm::Type *element_llvm_type = get_llvm_type(array_type->elementType);

        // Allocate space for the array elements on the stack
        size_t num_elements = node->elements.size();
        auto array_size = llvm::ConstantInt::get(
            llvm::Type::getInt32Ty(*context),
            num_elements);

        // Allocate array of elements (like Alloc but for specific type)
        auto array_ptr = builder->CreateAlloca(
            element_llvm_type,
            array_size,
            "array.literal");

        // Initialize array elements
        for (size_t i = 0; i < node->elements.size(); i++)
        {
            // Get the element value
            auto element_value = genRValue(node->elements[i]);
            if (!element_value)
                continue;

            // Calculate pointer to element i
            auto element_ptr = builder->CreateGEP(
                element_llvm_type,
                array_ptr,
                llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), i),
                "array.elem." + std::to_string(i));

            // Store the value
            builder->CreateStore(element_value, element_ptr);
        }

        // Return the pointer to the array
        push_value(array_ptr);
    }

    void CodeGenerator::visit(QualifiedNameSyntax *node)
    {
        if (!node || !node->object || !node->member)
            return;

        // Get the object's resolved type to find the struct type
        auto obj_type = node->object->resolvedType;
        if (!obj_type)
        {
            report_error(node, "Object type not resolved");
            return;
        }

        // Get the LLVM struct type from our type system
        llvm::Type *struct_type = get_llvm_type(obj_type);
        if (!struct_type->isStructTy())
        {
            report_error(node, "Member access on non-struct type");
            return;
        }

        // Find member and determine its type
        if (auto type_ref = std::get_if<TypeReference>(&obj_type->value))
        {
            if (auto type_sym = type_ref->definition->as<TypeSymbol>())
            {
                std::string member_name(node->member->text);
                auto member_symbol = type_sym->lookup_local(member_name);

                if (!member_symbol)
                {
                    report_error(node, "Member not found: " + member_name);
                    return;
                }

                // Handle different member types
                if (auto var_sym = member_symbol->as<VariableSymbol>())
                {
                    // Field access - use GEP to get field address

                    // We need the struct address
                    llvm::Value *struct_ptr = nullptr;

                    if (node->object->is_l_value())
                    {
                        // Object is an lvalue - get its address
                        struct_ptr = genLValue(node->object);
                    }
                    else
                    {
                        // Object is an rvalue - need to create temporary storage
                        auto struct_val = genRValue(node->object);
                        if (!struct_val)
                            return;
                        struct_ptr = ensureAddress(struct_val, obj_type);
                    }

                    if (!struct_ptr)
                        return;

                    // Find the field index
                    int member_index = 0;
                    for (const auto &[name, sym_node] : type_sym->symbols)
                    {
                        if (sym_node->as<VariableSymbol>())
                        {
                            if (name == member_name)
                                break;
                            member_index++;
                        }
                    }

                    // Create GEP to get pointer to the field
                    auto member_ptr = builder->CreateStructGEP(
                        struct_type,
                        struct_ptr,
                        member_index,
                        member_name);

                    // Field access returns an lvalue (address)
                    push_value(member_ptr);
                }
                else if (auto prop_sym = member_symbol->as<PropertySymbol>())
                {
                    // Property access - call getter
                    std::string getter_name = prop_sym->get_qualified_name() + ".get";

                    auto getter_func = module->getFunction(getter_name);
                    if (!getter_func)
                    {
                        report_error(node, "Property getter not found: " + getter_name);
                        return;
                    }

                    // Get struct pointer for 'this' parameter
                    llvm::Value *struct_ptr = nullptr;

                    if (node->object->is_l_value())
                    {
                        struct_ptr = genLValue(node->object);
                    }
                    else
                    {
                        auto struct_val = genRValue(node->object);
                        if (!struct_val)
                            return;
                        struct_ptr = ensureAddress(struct_val, obj_type);
                    }

                    if (!struct_ptr)
                        return;

                    // Call getter with 'this' pointer as argument
                    auto result = builder->CreateCall(getter_func, {struct_ptr}, member_name);

                    // Property getters return rvalues
                    push_value(result);
                }
                else if (auto func_sym = member_symbol->as<FunctionSymbol>())
                {
                    // Method access without call - this is handled by CallExprSyntax
                    // We shouldn't generate code here, just push a placeholder
                    // Actually, this case might not be reachable if methods are only
                    // accessed through CallExprSyntax
                    report_error(node, "Method reference without call not yet supported");
                }
                else
                {
                    report_error(node, "Unsupported member type: " + member_name);
                }
            }
        }
        else
        {
            report_error(node, "Member access on non-user-defined type");
        }
    }

    void CodeGenerator::visit(IndexerExprSyntax *node)
    {
        if (!node || !node->object || !node->index)
            return;

        // Get the index value (always need the value, not address)
        auto index_value = genRValue(node->index);
        if (!index_value)
            return;

        // Get the object's type to determine indexing strategy
        auto obj_type = node->object->resolvedType;
        if (!obj_type)
        {
            report_error(node, "Object type not resolved for indexing");
            return;
        }

        llvm::Value *element_ptr = nullptr;

        // Handle array types: array[index]
        if (auto array_type_ptr = std::get_if<ArrayType>(&obj_type->value))
        {
            llvm::Type *element_type = get_llvm_type(array_type_ptr->elementType);

            // Arrays are pointers, so load the pointer first
            auto array_ptr = genRValue(node->object); // Get the pointer value
            if (!array_ptr)
                return;

            // Simple pointer arithmetic
            element_ptr = builder->CreateGEP(
                element_type,
                array_ptr,
                index_value,
                "array.elem");
        }
        // Handle pointer types: ptr[index] (equivalent to *(ptr + index))
        else if (auto pointer_type_ptr = std::get_if<PointerType>(&obj_type->value))
        {
            llvm::Type *element_type = get_llvm_type(pointer_type_ptr->pointeeType);

            // Get the pointer value
            auto ptr_value = genRValue(node->object);
            if (!ptr_value)
                return;

            // Pointer indexing: ptr[i] is equivalent to *(ptr + i)
            element_ptr = builder->CreateGEP(
                element_type,
                ptr_value,
                index_value,
                "ptr.elem");
        }
        else
        {
            report_error(node, "Indexer operand is not an array or pointer type");
            return;
        }

        if (element_ptr)
        {
            // Indexing returns an lvalue (address to the element)
            push_value(element_ptr);
        }
    }

    void CodeGenerator::visit(CastExprSyntax *node)
    {
        if (!node || !node->expression || !node->targetType)
        {
            report_error(node, "Invalid cast expression");
            return;
        }

        // Get the value to cast (need rvalue)
        auto source_value = genRValue(node->expression);
        if (!source_value)
        {
            report_error(node, "Failed to evaluate cast source expression");
            return;
        }

        // Get source and target types
        auto source_type = node->expression->resolvedType;
        auto target_type = node->resolvedType;

        if (!source_type || !target_type)
        {
            report_error(node, "Cast types not resolved");
            return;
        }

        // Check if this is an identity conversion (no cast needed)
        if (source_type == target_type)
        {
            push_value(source_value);
            return;
        }

        llvm::Value *result = nullptr;

        // Handle primitive type conversions
        auto source_prim = std::get_if<PrimitiveType>(&source_type->value);
        auto target_prim = std::get_if<PrimitiveType>(&target_type->value);

        if (source_prim && target_prim)
        {
            result = castPrimitive(source_value, source_prim->kind, target_prim->kind, node);
        }
        // Handle pointer casts
        else if (std::get_if<PointerType>(&source_type->value) &&
                 std::get_if<PointerType>(&target_type->value))
        {
            // Pointer to pointer cast - in LLVM with opaque pointers, this is a no-op
            result = source_value;
        }
        // Handle array to pointer decay (if needed)
        else if (std::get_if<ArrayType>(&source_type->value) &&
                 std::get_if<PointerType>(&target_type->value))
        {
            // Array to pointer decay - arrays are already pointers in your implementation
            result = source_value;
        }
        // Handle reference type casts (if both are reference types)
        else if (source_type->get_storage_kind() == Type::StorageKind::Indirect &&
                 target_type->get_storage_kind() == Type::StorageKind::Indirect)
        {
            // Reference to reference cast - treat as pointer cast (no-op with opaque pointers)
            result = source_value;
        }
        else
        {
            report_error(node, "Unsupported cast from " + source_type->get_name() + " to " + target_type->get_name());
            return;
        }

        if (result)
        {
            push_value(result);
        }
    }

    void CodeGenerator::visit(ThisExprSyntax *node)
    {
        if (!node)
            return;

        // Check if we're in a method context
        if (!current_function)
        {
            report_error(node, "'this' used outside of function context");
            return;
        }

        // Check if the current function has a 'this' parameter (first argument)
        if (current_function->arg_size() == 0)
        {
            report_error(node, "'this' used in static context");
            return;
        }

        // Check if the first parameter is actually 'this'
        auto first_param = current_function->arg_begin();
        if (first_param->getName() != "this")
        {
            report_error(node, "'this' used in non-method context");
            return;
        }

        // The 'this' pointer is the first argument to the method
        // It's already a pointer to the struct, so we just push it
        // Since 'this' is an lvalue (you can take its address with &this),
        // we need to push the address where 'this' is stored, not its value

        // However, 'this' is special - it's not stored in an alloca like other parameters
        // It's used directly as a pointer value. So for 'this' expressions,
        // we push the pointer value directly (making it an rvalue in our system)
        push_value(first_param);
    }

    void CodeGenerator::visit(LambdaExprSyntax *n) { report_error(n, "Lambda expressions not yet supported."); }
    void CodeGenerator::visit(ConditionalExprSyntax *n) { report_error(n, "Conditional (ternary) expressions not yet supported."); }
    void CodeGenerator::visit(TypeOfExpr *n) { report_error(n, "'typeof' not yet supported."); }
    void CodeGenerator::visit(SizeOfExpr *n) { report_error(n, "'sizeof' not yet supported."); }
    void CodeGenerator::visit(IfStmtSyntax *node)
    {
        if (!node || !node->condition || !node->thenBranch)
            return;

        // Evaluate condition - need value not address
        auto cond_value = genRValue(node->condition);
        if (!cond_value)
            return;

        // Ensure condition is a boolean (i1)
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
                report_error(node->condition, "If condition must be a boolean expression");
                return;
            }
        }

        // Create basic blocks
        auto then_bb = llvm::BasicBlock::Create(*context, "if.then", current_function);
        auto else_bb = node->elseBranch
                           ? llvm::BasicBlock::Create(*context, "if.else", current_function)
                           : nullptr;
        auto merge_bb = llvm::BasicBlock::Create(*context, "if.merge", current_function);

        // Create conditional branch
        if (else_bb)
        {
            builder->CreateCondBr(cond_value, then_bb, else_bb);
        }
        else
        {
            builder->CreateCondBr(cond_value, then_bb, merge_bb);
        }

        // Generate then branch
        builder->SetInsertPoint(then_bb);
        node->thenBranch->accept(this);

        // Add branch to merge block if the block doesn't already have a terminator
        if (!builder->GetInsertBlock()->getTerminator())
        {
            builder->CreateBr(merge_bb);
        }
        // Update then_bb in case it changed (nested control flow)
        then_bb = builder->GetInsertBlock();

        // Generate else branch if present
        if (else_bb)
        {
            builder->SetInsertPoint(else_bb);
            node->elseBranch->accept(this);

            // Add branch to merge block if needed
            if (!builder->GetInsertBlock()->getTerminator())
            {
                builder->CreateBr(merge_bb);
            }
            // Update else_bb in case it changed
            else_bb = builder->GetInsertBlock();
        }

        // Continue code generation at merge block
        builder->SetInsertPoint(merge_bb);
    }

    void CodeGenerator::visit(BreakStmtSyntax *node)
    {
        if (!node)
            return;

        // Check if we're inside a loop
        if (loop_stack.empty())
        {
            report_error(node, "'break' statement used outside of a loop");
            return;
        }

        // Get the current loop's break target
        auto loop_context = loop_stack.top();

        // Jump to the loop's exit block
        builder->CreateBr(loop_context.breakTarget);

        // Create a new unreachable block for any code that might follow
        // (though there shouldn't be any reachable code after break)
        auto unreachable_bb = llvm::BasicBlock::Create(*context, "after.break", current_function);
        builder->SetInsertPoint(unreachable_bb);
    }

    void CodeGenerator::visit(ContinueStmtSyntax *node)
    {
        if (!node)
            return;

        // Check if we're inside a loop
        if (loop_stack.empty())
        {
            report_error(node, "'continue' statement used outside of a loop");
            return;
        }

        // Get the current loop's continue target
        auto loop_context = loop_stack.top();

        // Jump to the loop's continue target (increment block for 'for', condition for 'while')
        builder->CreateBr(loop_context.continueTarget);

        // Create a new unreachable block for any code that might follow
        // (though there shouldn't be any reachable code after continue)
        auto unreachable_bb = llvm::BasicBlock::Create(*context, "after.continue", current_function);
        builder->SetInsertPoint(unreachable_bb);
    }

    void CodeGenerator::visit(WhileStmtSyntax *node)
    {
        if (!node || !node->condition || !node->body)
            return;

        // Create basic blocks for the loop
        auto loop_cond = llvm::BasicBlock::Create(*context, "while.cond", current_function);
        auto loop_body = llvm::BasicBlock::Create(*context, "while.body", current_function);
        auto loop_exit = llvm::BasicBlock::Create(*context, "while.exit", current_function);

        // Push loop context for break/continue
        loop_stack.push({loop_exit, loop_cond});

        // Jump to condition check
        builder->CreateBr(loop_cond);

        // Condition block - evaluate condition each iteration
        builder->SetInsertPoint(loop_cond);

        // Evaluate condition - need value not address
        auto cond_value = genRValue(node->condition);
        if (!cond_value)
        {
            loop_stack.pop();
            return;
        }

        // Ensure condition is a boolean (i1)
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
                report_error(node->condition, "While condition must be a boolean expression");
                loop_stack.pop();
                return;
            }
        }

        // Branch based on condition
        builder->CreateCondBr(cond_value, loop_body, loop_exit);

        // Body block
        builder->SetInsertPoint(loop_body);
        node->body->accept(this);

        // Loop back to condition (if body didn't terminate)
        if (!builder->GetInsertBlock()->getTerminator())
        {
            builder->CreateBr(loop_cond);
        }

        // Pop loop context
        loop_stack.pop();

        // Continue with exit block
        builder->SetInsertPoint(loop_exit);
    }

    void CodeGenerator::visit(ForStmtSyntax *node)
    {
        if (!node || !node->body)
            return;

        // Create basic blocks for the for loop
        auto init_bb = llvm::BasicBlock::Create(*context, "for.init", current_function);
        auto cond_bb = llvm::BasicBlock::Create(*context, "for.cond", current_function);
        auto body_bb = llvm::BasicBlock::Create(*context, "for.body", current_function);
        auto increment_bb = llvm::BasicBlock::Create(*context, "for.inc", current_function);
        auto exit_bb = llvm::BasicBlock::Create(*context, "for.exit", current_function);

        // Push loop context for break/continue
        // For 'for' loops, continue goes to the increment block
        loop_stack.push({exit_bb, increment_bb});

        // Jump to initialization
        builder->CreateBr(init_bb);

        // Initialization block
        builder->SetInsertPoint(init_bb);
        if (node->initializer)
        {
            node->initializer->accept(this);
            // Pop any value from expression statements
            if (!value_stack.empty())
            {
                pop_value();
            }
        }
        builder->CreateBr(cond_bb);

        // Condition block
        builder->SetInsertPoint(cond_bb);
        if (node->condition)
        {
            // Evaluate condition - need value not address
            auto cond_value = genRValue(node->condition);
            if (!cond_value)
            {
                // Error in condition - exit loop
                loop_stack.pop();
                builder->CreateBr(exit_bb);
                builder->SetInsertPoint(exit_bb);
                return;
            }

            // Ensure condition is a boolean (i1)
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
                    report_error(node->condition, "For condition must be a boolean expression");
                    loop_stack.pop();
                    builder->CreateBr(exit_bb);
                    builder->SetInsertPoint(exit_bb);
                    return;
                }
            }

            builder->CreateCondBr(cond_value, body_bb, exit_bb);
        }
        else
        {
            // No condition means infinite loop
            builder->CreateBr(body_bb);
        }

        // Body block
        builder->SetInsertPoint(body_bb);
        node->body->accept(this);

        // Jump to increment block if body didn't terminate
        if (!builder->GetInsertBlock()->getTerminator())
        {
            builder->CreateBr(increment_bb);
        }

        // Increment block
        builder->SetInsertPoint(increment_bb);
        if (!node->updates.empty())
        {
            for (auto update_expr : node->updates)
            {
                if (update_expr)
                {
                    update_expr->accept(this);

                    // Pop the value if there is one (for expression statements)
                    if (!value_stack.empty())
                    {
                        pop_value();
                    }
                }
            }
        }
        // Loop back to condition
        builder->CreateBr(cond_bb);

        // Pop loop context
        loop_stack.pop();

        // Continue with exit block
        builder->SetInsertPoint(exit_bb);
    }

    void CodeGenerator::visit(UsingDirectiveSyntax *n) { /* No codegen needed */ }
    void CodeGenerator::visit(ConstructorDeclSyntax *n) { report_error(n, "Constructors not yet supported."); }
    void CodeGenerator::visit(PropertyAccessorSyntax *n) { report_error(n, "Properties not yet supported."); }
    void CodeGenerator::visit(EnumCaseDeclSyntax *n) { /* No codegen needed */ }
    void CodeGenerator::visit(ArrayTypeSyntax *n) { /* Type expressions are not executed */ }
    void CodeGenerator::visit(PointerTypeSyntax *n) { /* Pointer type expressions are not executed */ }
    void CodeGenerator::visit(TypeParameterDeclSyntax *n) { /* Type parameter declarations don't generate code - handled during monomorphization */ }

} // namespace Bryo