#include "codegen/jit_engine.hpp"
#include "common/logger.hpp"
#include <iostream>

#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/MemoryBuffer.h"

namespace Myre {

JITEngine::JITEngine() : execution_engine_(nullptr), context_(nullptr), module_(nullptr) {
    // Initialize LLVM targets for JIT compilation
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();
}

JITEngine::~JITEngine() {
    // Explicitly destroy execution engine before context
    execution_engine_.reset();
    context_.reset();
}

bool JITEngine::initialize(std::unique_ptr<llvm::LLVMContext> context, 
                          std::unique_ptr<llvm::Module> module) {
    if (!context || !module) {
        std::cerr << "JITEngine: null context or module provided" << std::endl;
        return false;
    }
    
    // Store module reference before moving
    module_ = module.get();
    
    // Create execution engine (this takes ownership of the module)
    std::string error_msg;
    llvm::EngineBuilder builder(std::move(module));
    execution_engine_.reset(builder
        .setErrorStr(&error_msg)
        .setEngineKind(llvm::EngineKind::JIT)
        .create());
    
    // Store the context (we own it now)
    context_ = std::move(context);
    
    if (!execution_engine_) {
        std::cerr << "JITEngine: Failed to create execution engine: " << error_msg << std::endl;
        return false;
    }
    
    // Finalize the object (required for MCJIT)
    execution_engine_->finalizeObject();
    
    LOG_INFO("JITEngine: Initialized successfully", LogCategory::JIT);
    return true;
}

bool JITEngine::initialize_from_ir(const std::string& ir_string, const std::string& module_name) {
    // Create a new LLVM context
    context_ = std::make_unique<llvm::LLVMContext>();
    
    // Create memory buffer from IR string
    auto memory_buffer = llvm::MemoryBuffer::getMemBuffer(ir_string, module_name);
    
    // Parse the IR
    llvm::SMDiagnostic error;
    std::unique_ptr<llvm::Module> module = llvm::parseIR(*memory_buffer, error, *context_);
    
    if (!module) {
        LOG_ERROR("JITEngine: Failed to parse IR: " + error.getMessage().str(), LogCategory::JIT);
        return false;
    }
    
    LOG_INFO("JITEngine: Successfully parsed IR module '" + module_name + "'", LogCategory::JIT);
    
    // Store module reference before moving
    module_ = module.get();
    
    // Create execution engine
    std::string error_msg;
    llvm::EngineBuilder builder(std::move(module));
    execution_engine_.reset(builder
        .setErrorStr(&error_msg)
        .setEngineKind(llvm::EngineKind::JIT)
        .create());
    
    if (!execution_engine_) {
        LOG_ERROR("JITEngine: Failed to create execution engine: " + error_msg, LogCategory::JIT);
        return false;
    }
    
    // Finalize the object (required for MCJIT)
    execution_engine_->finalizeObject();
    
    LOG_INFO("JITEngine: Initialized from IR string successfully", LogCategory::JIT);
    return true;
}

int JITEngine::execute_function(const std::string& function_name) {
    if (!execution_engine_) {
        LOG_ERROR("JITEngine: Not initialized", LogCategory::JIT);
        return -1;
    }
    
    // Find the function
    llvm::Function* func = module_->getFunction(function_name);
    if (!func) {
        LOG_ERROR("JITEngine: Function '" + function_name + "' not found", LogCategory::JIT);
        return -1;
    }
    
    LOG_INFO("JITEngine: Executing function '" + function_name + "'", LogCategory::JIT);
    
    // Execute the function
    std::vector<llvm::GenericValue> args; // No arguments for now
    llvm::GenericValue result = execution_engine_->runFunction(func, args);
    
    // Convert result based on function return type
    if (func->getReturnType()->isVoidTy()) {
        LOG_DEBUG("JITEngine: Function executed (void return)", LogCategory::JIT);
        return 0;
    } else if (func->getReturnType()->isIntegerTy(32)) {
        int32_t int_result = result.IntVal.getSExtValue();
        LOG_DEBUG("JITEngine: Function returned: " + std::to_string(int_result), LogCategory::JIT);
        return int_result;
    } else {
        LOG_ERROR("JITEngine: Unsupported return type", LogCategory::JIT);
        return -1;
    }
}

void* JITEngine::get_function_pointer(const std::string& function_name) {
    if (!execution_engine_) {
        LOG_ERROR("JITEngine: Not initialized", LogCategory::JIT);
        return nullptr;
    }
    
    // Find the function
    llvm::Function* func = module_->getFunction(function_name);
    if (!func) {
        LOG_ERROR("JITEngine: Function '" + function_name + "' not found", LogCategory::JIT);
        return nullptr;
    }
    
    // Get function pointer
    void* func_ptr = execution_engine_->getPointerToFunction(func);
    if (!func_ptr) {
        LOG_ERROR("JITEngine: Failed to get function pointer for '" + function_name + "'", LogCategory::JIT);
        return nullptr;
    }
    
    LOG_DEBUG("JITEngine: Got function pointer for '" + function_name + "'", LogCategory::JIT);
    return func_ptr;
}

void JITEngine::dump_functions() {
    if (!module_) {
        LOG_ERROR("JITEngine: No module loaded", LogCategory::JIT);
        return;
    }
    
    LOG_INFO("JITEngine: Available functions:", LogCategory::JIT);
    for (auto& func : *module_) {
        LOG_INFO("  - " + func.getName().str(), LogCategory::JIT);
    }
}

} // namespace Myre