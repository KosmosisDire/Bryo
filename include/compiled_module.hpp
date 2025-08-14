#pragma once
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include <memory>
#include <string>
#include <optional>

namespace Myre
{

class CompiledModule {
private:
    std::unique_ptr<llvm::LLVMContext> context;
    std::unique_ptr<llvm::Module> module;
    std::string module_name;
    bool has_errors;
    std::vector<std::string> errors;
    
public:
    CompiledModule() 
        : context(nullptr), module(nullptr), has_errors(true) {}
    
    CompiledModule(std::unique_ptr<llvm::LLVMContext> ctx,
                   std::unique_ptr<llvm::Module> mod,
                   const std::string& name,
                   const std::vector<std::string>& compilation_errors = {})
        : context(std::move(ctx)), 
          module(std::move(mod)),
          module_name(name),
          has_errors(!compilation_errors.empty()),
          errors(compilation_errors) {}
    
    // Move-only type
    CompiledModule(CompiledModule&&) = default;
    CompiledModule& operator=(CompiledModule&&) = default;
    CompiledModule(const CompiledModule&) = delete;
    CompiledModule& operator=(const CompiledModule&) = delete;
    
    // Check if compilation succeeded
    bool is_valid() const { return module != nullptr && !has_errors; }
    const std::vector<std::string>& get_errors() const { return errors; }
    
    // Output options
    bool write_ir(const std::string& filename) const;
    bool write_object_file(const std::string& filename) const;
    bool write_assembly(const std::string& filename) const;
    int execute_jit();
    
    // Get LLVM IR as string
    std::string get_ir_string() const;
    
    // For debugging
    void dump_ir() const;
};

} // namespace Myre