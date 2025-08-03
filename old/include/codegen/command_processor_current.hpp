#pragma once
#include "codegen/ir_command.hpp"
#include <memory>
#include <unordered_map>
#include <string>

// Forward declarations
namespace llvm {
    class LLVMContext;
    class Module;
    template<typename T, typename Inserter> class IRBuilder;
    class ConstantFolder;
    class IRBuilderDefaultInserter;
    class Value;
    class Type;
    class StructType;
    class Function;
    class BasicBlock;
}

namespace Myre {

class CommandProcessor {
private:
    std::unique_ptr<llvm::LLVMContext> context_;
    std::unique_ptr<llvm::Module> module_;
    std::unique_ptr<llvm::IRBuilder<llvm::ConstantFolder, llvm::IRBuilderDefaultInserter>> builder_;
    
    // Value tracking
    std::unordered_map<int, llvm::Value*> value_map_;
    
    // Current function context
    llvm::Function* current_function_;
    llvm::BasicBlock* current_block_;
    
    // Basic block tracking for control flow
    std::unordered_map<std::string, llvm::BasicBlock*> block_map_;
    const std::vector<Command>* commands_;  // Commands for two-pass processing
    
    // Struct type caching to prevent duplicates
    std::unordered_map<std::string, llvm::StructType*> struct_type_cache_;
    
    // Parameter tracking
    int param_count_ = 0;
    int current_alloca_index_ = 0;
    
    // Type conversion
    llvm::Type* to_llvm_type(IRType type);
    
    // Command processing
    void create_basic_blocks(const std::vector<Command>& commands);  // Pass 1: Create all BasicBlocks
    void create_function_basic_blocks();                             // Create BasicBlocks for current function
    void process_command(const Command& cmd);                        // Pass 2: Process individual commands
    llvm::Value* get_value(int id);
    
public:
    CommandProcessor(const std::string& module_name);
    ~CommandProcessor();  // Needed for unique_ptr with forward declarations
    
    // Process all commands
    void process(const std::vector<Command>& commands);
    
    // Output
    void dump_module();
    std::string get_ir_string();
    
    // Verification
    bool verify_module();
    
    // Transfer ownership for JIT
    std::unique_ptr<llvm::LLVMContext> take_context();
    std::unique_ptr<llvm::Module> take_module();
    
    // Static method to process commands and return IR string
    static std::string process_to_ir_string(const std::vector<Command>& commands, const std::string& module_name = "Module");
};

} // namespace Myre