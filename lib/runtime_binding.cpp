#include "runtime_binding.h"
#include "mycelium_runtime.h" // Include your C runtime header for function pointers & types

static llvm::FunctionType* get_llvm_type_Mycelium_String_new_from_literal(llvm::LLVMContext& context, llvm::Type* myceliumStringTypePtr) {
    llvm::Type* charPtrTy = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(context));
    llvm::Type* sizeTTy = llvm::Type::getInt64Ty(context); // Assuming size_t is i64
    return llvm::FunctionType::get(myceliumStringTypePtr, {charPtrTy, sizeTTy}, false);
}

static llvm::FunctionType* get_llvm_type_Mycelium_String_concat(llvm::LLVMContext& context, llvm::Type* myceliumStringTypePtr) {
    return llvm::FunctionType::get(myceliumStringTypePtr, {myceliumStringTypePtr, myceliumStringTypePtr}, false);
}

static llvm::FunctionType* get_llvm_type_Mycelium_String_print(llvm::LLVMContext& context, llvm::Type* myceliumStringTypePtr) {
    llvm::Type* voidTy = llvm::Type::getVoidTy(context);
    return llvm::FunctionType::get(voidTy, {myceliumStringTypePtr}, false);
}

static llvm::FunctionType* get_llvm_type_Mycelium_String_delete(llvm::LLVMContext& context, llvm::Type* myceliumStringTypePtr) {
    llvm::Type* voidTy = llvm::Type::getVoidTy(context);
    return llvm::FunctionType::get(voidTy, {myceliumStringTypePtr}, false);
}

// --- The Global Registry ---
// THIS IS THE PRIMARY PLACE YOU'LL MODIFY WHEN ADDING NEW RUNTIME FUNCTIONS
static const std::vector<RuntimeFunctionBinding> g_runtime_function_bindings_list = {
    {
        "Mycelium_String_new_from_literal",
        reinterpret_cast<void*>(Mycelium_String_new_from_literal),
        get_llvm_type_Mycelium_String_new_from_literal
    },
    {
        "Mycelium_String_concat",
        reinterpret_cast<void*>(Mycelium_String_concat),
        get_llvm_type_Mycelium_String_concat
    },
    {
        "Mycelium_String_print",
        reinterpret_cast<void*>(Mycelium_String_print),
        get_llvm_type_Mycelium_String_print
    },
    {
        "Mycelium_String_delete",
        reinterpret_cast<void*>(Mycelium_String_delete),
        get_llvm_type_Mycelium_String_delete
    }
    // To add a new runtime function "MyNewFunc" that takes int and returns int:
    // 1. Add MyNewFunc to mycelium_runtime.h and .cpp
    // 2. Create static llvm::FunctionType* get_llvm_type_MyNewFunc(...) here.
    // 3. Add to this list:
    //    { "MyNewFunc_IR_Name", reinterpret_cast<void*>(MyNewFunc_C_Name), get_llvm_type_MyNewFunc }
};

const std::vector<RuntimeFunctionBinding>& get_runtime_bindings() {
    return g_runtime_function_bindings_list;
}