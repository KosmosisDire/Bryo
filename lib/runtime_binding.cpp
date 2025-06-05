#include "runtime_binding.h"
#include "mycelium_runtime.h" // Include your C runtime header for function pointers & types

// --- LLVM Type Getters for MyceliumString functions ---
static llvm::FunctionType* get_llvm_type_Mycelium_String_new_from_literal(llvm::LLVMContext& context, llvm::Type* myceliumStringTypePtr, llvm::Type* myceliumObjectHeaderTypePtr) {
    (void)myceliumObjectHeaderTypePtr; // Unused for this function
    llvm::Type* charPtrTy = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(context));
    llvm::Type* sizeTTy = llvm::Type::getInt64Ty(context); // Assuming size_t is i64
    return llvm::FunctionType::get(myceliumStringTypePtr, {charPtrTy, sizeTTy}, false);
}

static llvm::FunctionType* get_llvm_type_Mycelium_String_concat(llvm::LLVMContext& context, llvm::Type* myceliumStringTypePtr, llvm::Type* myceliumObjectHeaderTypePtr) {
    (void)myceliumObjectHeaderTypePtr; // Unused
    return llvm::FunctionType::get(myceliumStringTypePtr, {myceliumStringTypePtr, myceliumStringTypePtr}, false);
}

static llvm::FunctionType* get_llvm_type_Mycelium_String_print(llvm::LLVMContext& context, llvm::Type* myceliumStringTypePtr, llvm::Type* myceliumObjectHeaderTypePtr) {
    (void)myceliumObjectHeaderTypePtr; // Unused
    llvm::Type* voidTy = llvm::Type::getVoidTy(context);
    return llvm::FunctionType::get(voidTy, {myceliumStringTypePtr}, false);
}

static llvm::FunctionType* get_llvm_type_Mycelium_String_delete(llvm::LLVMContext& context, llvm::Type* myceliumStringTypePtr, llvm::Type* myceliumObjectHeaderTypePtr) {
    (void)myceliumObjectHeaderTypePtr; // Unused
    llvm::Type* voidTy = llvm::Type::getVoidTy(context);
    return llvm::FunctionType::get(voidTy, {myceliumStringTypePtr}, false);
}

// --- LLVM Type Getters for String Conversion Functions ---
static llvm::FunctionType* get_llvm_type_Mycelium_String_from_int(llvm::LLVMContext& context, llvm::Type* myceliumStringTypePtr, llvm::Type* myceliumObjectHeaderTypePtr) {
    (void)myceliumObjectHeaderTypePtr; // Unused
    return llvm::FunctionType::get(myceliumStringTypePtr, {llvm::Type::getInt32Ty(context)}, false);
}

static llvm::FunctionType* get_llvm_type_Mycelium_String_from_long(llvm::LLVMContext& context, llvm::Type* myceliumStringTypePtr, llvm::Type* myceliumObjectHeaderTypePtr) {
    (void)myceliumObjectHeaderTypePtr; // Unused
    return llvm::FunctionType::get(myceliumStringTypePtr, {llvm::Type::getInt64Ty(context)}, false);
}

static llvm::FunctionType* get_llvm_type_Mycelium_String_from_float(llvm::LLVMContext& context, llvm::Type* myceliumStringTypePtr, llvm::Type* myceliumObjectHeaderTypePtr) {
    (void)myceliumObjectHeaderTypePtr; // Unused
    return llvm::FunctionType::get(myceliumStringTypePtr, {llvm::Type::getFloatTy(context)}, false);
}

static llvm::FunctionType* get_llvm_type_Mycelium_String_from_double(llvm::LLVMContext& context, llvm::Type* myceliumStringTypePtr, llvm::Type* myceliumObjectHeaderTypePtr) {
    (void)myceliumObjectHeaderTypePtr; // Unused
    return llvm::FunctionType::get(myceliumStringTypePtr, {llvm::Type::getDoubleTy(context)}, false);
}

static llvm::FunctionType* get_llvm_type_Mycelium_String_from_bool(llvm::LLVMContext& context, llvm::Type* myceliumStringTypePtr, llvm::Type* myceliumObjectHeaderTypePtr) {
    (void)myceliumObjectHeaderTypePtr; // Unused
    return llvm::FunctionType::get(myceliumStringTypePtr, {llvm::Type::getInt1Ty(context)}, false);
}

static llvm::FunctionType* get_llvm_type_Mycelium_String_from_char(llvm::LLVMContext& context, llvm::Type* myceliumStringTypePtr, llvm::Type* myceliumObjectHeaderTypePtr) {
    (void)myceliumObjectHeaderTypePtr; // Unused
    return llvm::FunctionType::get(myceliumStringTypePtr, {llvm::Type::getInt8Ty(context)}, false);
}

static llvm::FunctionType* get_llvm_type_Mycelium_String_to_int(llvm::LLVMContext& context, llvm::Type* myceliumStringTypePtr, llvm::Type* myceliumObjectHeaderTypePtr) {
    (void)myceliumObjectHeaderTypePtr; // Unused
    return llvm::FunctionType::get(llvm::Type::getInt32Ty(context), {myceliumStringTypePtr}, false);
}

static llvm::FunctionType* get_llvm_type_Mycelium_String_to_long(llvm::LLVMContext& context, llvm::Type* myceliumStringTypePtr, llvm::Type* myceliumObjectHeaderTypePtr) {
    (void)myceliumObjectHeaderTypePtr; // Unused
    return llvm::FunctionType::get(llvm::Type::getInt64Ty(context), {myceliumStringTypePtr}, false);
}

static llvm::FunctionType* get_llvm_type_Mycelium_String_to_float(llvm::LLVMContext& context, llvm::Type* myceliumStringTypePtr, llvm::Type* myceliumObjectHeaderTypePtr) {
    (void)myceliumObjectHeaderTypePtr; // Unused
    return llvm::FunctionType::get(llvm::Type::getFloatTy(context), {myceliumStringTypePtr}, false);
}

static llvm::FunctionType* get_llvm_type_Mycelium_String_to_double(llvm::LLVMContext& context, llvm::Type* myceliumStringTypePtr, llvm::Type* myceliumObjectHeaderTypePtr) {
    (void)myceliumObjectHeaderTypePtr; // Unused
    return llvm::FunctionType::get(llvm::Type::getDoubleTy(context), {myceliumStringTypePtr}, false);
}

static llvm::FunctionType* get_llvm_type_Mycelium_String_to_bool(llvm::LLVMContext& context, llvm::Type* myceliumStringTypePtr, llvm::Type* myceliumObjectHeaderTypePtr) {
    (void)myceliumObjectHeaderTypePtr; // Unused
    return llvm::FunctionType::get(llvm::Type::getInt1Ty(context), {myceliumStringTypePtr}, false);
}

static llvm::FunctionType* get_llvm_type_Mycelium_String_to_char(llvm::LLVMContext& context, llvm::Type* myceliumStringTypePtr, llvm::Type* myceliumObjectHeaderTypePtr) {
    (void)myceliumObjectHeaderTypePtr; // Unused
    return llvm::FunctionType::get(llvm::Type::getInt8Ty(context), {myceliumStringTypePtr}, false);
}

// --- LLVM Type Getters for ARC functions ---
static llvm::FunctionType* get_llvm_type_Mycelium_Object_alloc(llvm::LLVMContext& context, llvm::Type* myceliumStringTypePtr, llvm::Type* myceliumObjectHeaderTypePtr) {
    (void)myceliumStringTypePtr; // Unused
    llvm::Type* sizeTTy = llvm::Type::getInt64Ty(context);    // data_size (size_t)
    llvm::Type* typeIdTy = llvm::Type::getInt32Ty(context);   // type_id (uint32_t)
    // Returns MyceliumObjectHeader*
    return llvm::FunctionType::get(myceliumObjectHeaderTypePtr, {sizeTTy, typeIdTy}, false);
}

static llvm::FunctionType* get_llvm_type_Mycelium_Object_retain(llvm::LLVMContext& context, llvm::Type* myceliumStringTypePtr, llvm::Type* myceliumObjectHeaderTypePtr) {
    (void)myceliumStringTypePtr; // Unused
    llvm::Type* voidTy = llvm::Type::getVoidTy(context);
    // Takes MyceliumObjectHeader*
    return llvm::FunctionType::get(voidTy, {myceliumObjectHeaderTypePtr}, false);
}

static llvm::FunctionType* get_llvm_type_Mycelium_Object_release(llvm::LLVMContext& context, llvm::Type* myceliumStringTypePtr, llvm::Type* myceliumObjectHeaderTypePtr) {
    (void)myceliumStringTypePtr; // Unused
    llvm::Type* voidTy = llvm::Type::getVoidTy(context);
    // Takes MyceliumObjectHeader*
    return llvm::FunctionType::get(voidTy, {myceliumObjectHeaderTypePtr}, false);
}

static llvm::FunctionType* get_llvm_type_Mycelium_Object_get_ref_count(llvm::LLVMContext& context, llvm::Type* myceliumStringTypePtr, llvm::Type* myceliumObjectHeaderTypePtr) {
    (void)myceliumStringTypePtr; // Unused
    llvm::Type* int32Ty = llvm::Type::getInt32Ty(context);
    // Takes MyceliumObjectHeader*, returns int32_t
    return llvm::FunctionType::get(int32Ty, {myceliumObjectHeaderTypePtr}, false);
}

// --- LLVM Type Getter for print_int ---
static llvm::FunctionType* get_llvm_type_print_int(llvm::LLVMContext& context, llvm::Type* myceliumStringTypePtr, llvm::Type* myceliumObjectHeaderTypePtr) {
    (void)myceliumStringTypePtr; // Unused
    (void)myceliumObjectHeaderTypePtr; // Unused
    llvm::Type* voidTy = llvm::Type::getVoidTy(context);
    llvm::Type* int32Ty = llvm::Type::getInt32Ty(context);
    return llvm::FunctionType::get(voidTy, {int32Ty}, false);
}

// --- LLVM Type Getter for print_double ---
static llvm::FunctionType* get_llvm_type_print_double(llvm::LLVMContext& context, llvm::Type* myceliumStringTypePtr, llvm::Type* myceliumObjectHeaderTypePtr) {
    (void)myceliumStringTypePtr; // Unused
    (void)myceliumObjectHeaderTypePtr; // Unused
    llvm::Type* voidTy = llvm::Type::getVoidTy(context);
    llvm::Type* doubleTy = llvm::Type::getDoubleTy(context);
    return llvm::FunctionType::get(voidTy, {doubleTy}, false);
}

// --- LLVM Type Getter for print_bool ---
static llvm::FunctionType* get_llvm_type_print_bool(llvm::LLVMContext& context, llvm::Type* myceliumStringTypePtr, llvm::Type* myceliumObjectHeaderTypePtr) {
    (void)myceliumStringTypePtr; // Unused
    (void)myceliumObjectHeaderTypePtr; // Unused
    llvm::Type* voidTy = llvm::Type::getVoidTy(context);
    llvm::Type* boolTy = llvm::Type::getInt1Ty(context); // bool is i1 in LLVM
    return llvm::FunctionType::get(voidTy, {boolTy}, false);
}

// --- LLVM Type Getters for String primitive struct methods ---
static llvm::FunctionType* get_llvm_type_Mycelium_String_get_length(llvm::LLVMContext& context, llvm::Type* myceliumStringTypePtr, llvm::Type* myceliumObjectHeaderTypePtr) {
    (void)myceliumObjectHeaderTypePtr; // Unused
    return llvm::FunctionType::get(llvm::Type::getInt32Ty(context), {myceliumStringTypePtr}, false);
}

static llvm::FunctionType* get_llvm_type_Mycelium_String_substring(llvm::LLVMContext& context, llvm::Type* myceliumStringTypePtr, llvm::Type* myceliumObjectHeaderTypePtr) {
    (void)myceliumObjectHeaderTypePtr; // Unused
    return llvm::FunctionType::get(myceliumStringTypePtr, {myceliumStringTypePtr, llvm::Type::getInt32Ty(context)}, false);
}

static llvm::FunctionType* get_llvm_type_Mycelium_String_get_empty(llvm::LLVMContext& context, llvm::Type* myceliumStringTypePtr, llvm::Type* myceliumObjectHeaderTypePtr) {
    (void)myceliumObjectHeaderTypePtr; // Unused
    return llvm::FunctionType::get(myceliumStringTypePtr, {}, false);
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
    },
    // New String Conversion Bindings
    {
        "Mycelium_String_from_int",
        reinterpret_cast<void*>(Mycelium_String_from_int),
        get_llvm_type_Mycelium_String_from_int
    },
    {
        "Mycelium_String_from_long",
        reinterpret_cast<void*>(Mycelium_String_from_long),
        get_llvm_type_Mycelium_String_from_long
    },
    {
        "Mycelium_String_from_float",
        reinterpret_cast<void*>(Mycelium_String_from_float),
        get_llvm_type_Mycelium_String_from_float
    },
    {
        "Mycelium_String_from_double",
        reinterpret_cast<void*>(Mycelium_String_from_double),
        get_llvm_type_Mycelium_String_from_double
    },
    {
        "Mycelium_String_from_bool",
        reinterpret_cast<void*>(Mycelium_String_from_bool),
        get_llvm_type_Mycelium_String_from_bool
    },
    {
        "Mycelium_String_from_char",
        reinterpret_cast<void*>(Mycelium_String_from_char),
        get_llvm_type_Mycelium_String_from_char
    },
    {
        "Mycelium_String_to_int",
        reinterpret_cast<void*>(Mycelium_String_to_int),
        get_llvm_type_Mycelium_String_to_int
    },
    {
        "Mycelium_String_to_long",
        reinterpret_cast<void*>(Mycelium_String_to_long),
        get_llvm_type_Mycelium_String_to_long
    },
    {
        "Mycelium_String_to_float",
        reinterpret_cast<void*>(Mycelium_String_to_float),
        get_llvm_type_Mycelium_String_to_float
    },
    {
        "Mycelium_String_to_double",
        reinterpret_cast<void*>(Mycelium_String_to_double),
        get_llvm_type_Mycelium_String_to_double
    },
    {
        "Mycelium_String_to_bool",
        reinterpret_cast<void*>(Mycelium_String_to_bool),
        get_llvm_type_Mycelium_String_to_bool
    },
    {
        "Mycelium_String_to_char",
        reinterpret_cast<void*>(Mycelium_String_to_char),
        get_llvm_type_Mycelium_String_to_char
    },
    // New ARC Function Bindings
    {
        "Mycelium_Object_alloc",
        reinterpret_cast<void*>(Mycelium_Object_alloc),
        get_llvm_type_Mycelium_Object_alloc
    },
    {
        "Mycelium_Object_retain",
        reinterpret_cast<void*>(Mycelium_Object_retain),
        get_llvm_type_Mycelium_Object_retain
    },
    {
        "Mycelium_Object_release",
        reinterpret_cast<void*>(Mycelium_Object_release),
        get_llvm_type_Mycelium_Object_release
    },
    {
        "Mycelium_Object_get_ref_count",
        reinterpret_cast<void*>(Mycelium_Object_get_ref_count),
        get_llvm_type_Mycelium_Object_get_ref_count
    },
    // Binding for print_int
    {
        "print_int",
        reinterpret_cast<void*>(print_int),
        get_llvm_type_print_int
    },
    // Binding for print_double
    {
        "print_double",
        reinterpret_cast<void*>(print_double),
        get_llvm_type_print_double
    },
    // Binding for print_bool
    {
        "print_bool",
        reinterpret_cast<void*>(print_bool),
        get_llvm_type_print_bool
    },
    // Bindings for string primitive struct methods
    {
        "Mycelium_String_get_length",
        reinterpret_cast<void*>(Mycelium_String_get_length),
        get_llvm_type_Mycelium_String_get_length
    },
    {
        "Mycelium_String_substring",
        reinterpret_cast<void*>(Mycelium_String_substring),
        get_llvm_type_Mycelium_String_substring
    },
    {
        "Mycelium_String_get_empty",
        reinterpret_cast<void*>(Mycelium_String_get_empty),
        get_llvm_type_Mycelium_String_get_empty
    }
};

const std::vector<RuntimeFunctionBinding>& get_runtime_bindings() {
    return g_runtime_function_bindings_list;
}
