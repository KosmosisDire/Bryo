// jit_executor.cpp
#include "jit.hpp"
#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <llvm/Support/TargetSelect.h>
#include <iostream>

namespace Myre {

JIT::JIT() {
    // Initialize LLVM targets (if not already done)
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();
    
    // Create LLJIT instance
    auto jit_expected = llvm::orc::LLJITBuilder().create();
    if (!jit_expected) {
        llvm::errs() << "Failed to create JIT: " 
                     << llvm::toString(jit_expected.takeError()) << "\n";
        exit(1);
    }
    jit = std::move(*jit_expected);
    
    // Add process symbols (for linking with system libraries)
    auto& es = jit->getExecutionSession();
    auto& main_dylib = jit->getMainJITDylib();
    
    // Load process symbols for printf, malloc, etc.
    auto generator = llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
        jit->getDataLayout().getGlobalPrefix());
    if (!generator) {
        llvm::errs() << "Failed to create symbol generator: "
                     << llvm::toString(generator.takeError()) << "\n";
        exit(1);
    }
    main_dylib.addGenerator(std::move(*generator));
}

bool JIT::add_module(std::unique_ptr<llvm::Module> module,
                             std::unique_ptr<llvm::LLVMContext> context) {
    auto err = jit->addIRModule(
        llvm::orc::ThreadSafeModule(std::move(module), std::move(context)));
    
    if (err) {
        llvm::errs() << "Failed to add module: " 
                     << llvm::toString(std::move(err)) << "\n";
        return false;
    }
    return true;
}

llvm::Expected<llvm::orc::ExecutorAddr> JIT::lookup(const std::string& name) {
    return jit->lookup(name);
}

int JIT::run_main() {
    // Look up the main function
    auto main_addr = lookup("main");
    if (!main_addr) {
        llvm::errs() << "Failed to find main: " 
                     << llvm::toString(main_addr.takeError()) << "\n";
        return -1;
    }
    
    // Cast to function pointer and execute
    auto* main_func = main_addr->toPtr<int(*)()>();
    if (!main_func) {
        llvm::errs() << "Failed to cast main function\n";
        return -1;
    }
    
    std::cout << "\n=== Executing JIT compiled code ===\n";
    int result = main_func();
    std::cout << "\n=== Execution complete (exit code: " << result << ") ===\n";
    
    return result;
}

} // namespace Myre
