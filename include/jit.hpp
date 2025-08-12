// jit_executor.hpp
#pragma once
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/IR/Module.h>
#include <memory>

namespace Myre{

class JIT {
private:
    std::unique_ptr<llvm::orc::LLJIT> jit;
    
public:
    JIT();
    ~JIT() = default;
    
    bool add_module(std::unique_ptr<llvm::Module> module, 
                   std::unique_ptr<llvm::LLVMContext> context);
    
    llvm::Expected<llvm::orc::ExecutorAddr> lookup(const std::string& name);
    
    template<typename FuncType>
    FuncType* get_function(const std::string& name) {
        auto addr = lookup(name);
        if (!addr) {
            llvm::consumeError(addr.takeError());
            return nullptr;
        }
        return addr->toPtr<FuncType*>();
    }
    
    int run_main();
};
}