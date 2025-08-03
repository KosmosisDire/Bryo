#pragma once
#include <memory>
#include <string>
#include <functional>

// Forward declarations
namespace llvm {
    class LLVMContext;
    class Module;
    class ExecutionEngine;
    class Function;
}

namespace Myre {

class JITEngine {
private:
    std::unique_ptr<llvm::ExecutionEngine> execution_engine_;
    std::unique_ptr<llvm::LLVMContext> context_;
    llvm::Module* module_;
    
public:
    JITEngine();
    ~JITEngine();
    
    // Initialize JIT with a module
    bool initialize(std::unique_ptr<llvm::LLVMContext> context, 
                   std::unique_ptr<llvm::Module> module);
    
    // Initialize JIT with LLVM IR string
    bool initialize_from_ir(const std::string& ir_string, const std::string& module_name = "JITModule");
    
    // Execute a function by name
    int execute_function(const std::string& function_name);
    
    // Get function pointer for more complex execution
    void* get_function_pointer(const std::string& function_name);
    
    // Check if JIT is ready
    bool is_ready() const { return execution_engine_ != nullptr; }
    
    // Debugging
    void dump_functions();
};

} // namespace Myre