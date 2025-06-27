#pragma once

#include <string>
#include <vector>
#include <cstdint> // For uint64_t
#include "llvm/IR/Type.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Function.h" // For llvm::FunctionType

// Forward declare if necessary, or include actual C runtime header
// For simplicity, let's assume you can include what's needed or functions are simple enough for now.

struct RuntimeFunctionBinding {
    std::string ir_function_name;   // Name used in LLVM IR (e.g., "Mycelium_String_print")
    void*       c_function_pointer; // Actual C function address (e.g., reinterpret_cast<void*>(Mycelium_String_print))

    // Function to create the LLVM FunctionType for this binding
    // Takes LLVMContext and any necessary pre-defined LLVM types (like MyceliumString*)
    // This makes the registry self-contained for IR declaration.
    using LLVMTypeGetter = llvm::FunctionType* (*)(
        llvm::LLVMContext& context,
        llvm::Type* myceliumStringTypePtr, // Existing
        llvm::Type* myceliumObjectHeaderTypePtr // New for ARC objects
    );
    LLVMTypeGetter get_llvm_type;
};

// Accessor for the global registry
const std::vector<RuntimeFunctionBinding>& get_runtime_bindings();

// Helper to register a function (optional, can also populate the list directly)
// void register_runtime_function(const std::string& name, void* ptr, RuntimeFunctionBinding::LLVMTypeGetter type_getter);
