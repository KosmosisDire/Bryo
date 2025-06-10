#pragma once

#include "llvm/IR/Value.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/IRBuilder.h"
#include "class_type_info.hpp"
#include <vector>
#include <memory>
#include <string>
#include <map>

namespace llvm {
    class Function;
    class AllocaInst;
}

namespace Mycelium::Scripting::Lang
{

/**
 * Represents an object that needs ARC management and destruction
 */
struct ManagedObject {
    llvm::AllocaInst* variable_alloca;       // The variable holding the object
    llvm::Value* header_ptr;                 // Pointer to object header for ARC
    const ClassTypeInfo* class_info;        // Class type information
    std::string debug_name;                  // For debugging/error messages
    
    ManagedObject(llvm::AllocaInst* alloca, llvm::Value* header, 
                  const ClassTypeInfo* class_info, const std::string& name)
        : variable_alloca(alloca), header_ptr(header), class_info(class_info), debug_name(name) {}
};

/**
 * Scope types for different language constructs
 */
enum class ScopeType {
    Function,     // Function scope - top level
    Block,        // General block scope {}
    Loop,         // Loop body scope (for, while)
    Conditional   // If/else block scope
};

/**
 * Represents a single scope in the scope hierarchy
 */
class Scope {
public:
    ScopeType type;
    std::string debug_name;
    std::vector<ManagedObject> managed_objects;
    llvm::BasicBlock* cleanup_block = nullptr;  // Optional cleanup block for this scope
    
    Scope(ScopeType scope_type, const std::string& name) 
        : type(scope_type), debug_name(name) {}
    
    void add_managed_object(const ManagedObject& obj) {
        managed_objects.push_back(obj);
    }
    
    // Get objects in reverse creation order for proper LIFO cleanup
    std::vector<ManagedObject> get_cleanup_order() const {
        std::vector<ManagedObject> reversed = managed_objects;
        std::reverse(reversed.begin(), reversed.end());
        return reversed;
    }
    
    bool has_managed_objects() const {
        return !managed_objects.empty();
    }
};

/**
 * Manages scope hierarchy and automatic object cleanup
 */
class ScopeManager {
private:
    std::vector<std::unique_ptr<Scope>> scope_stack;
    llvm::IRBuilder<>* builder;
    llvm::Module* module;
    
public:
    ScopeManager(llvm::IRBuilder<>* ir_builder, llvm::Module* llvm_module)
        : builder(ir_builder), module(llvm_module) {}
    
     void reset(llvm::IRBuilder<>* ir_builder, llvm::Module* llvm_module);

    // Scope management
    void push_scope(ScopeType type, const std::string& debug_name = "");
    void pop_scope();
    Scope* get_current_scope();
    size_t get_scope_depth() const { return scope_stack.size(); }
    
    // Object lifecycle management
    void register_managed_object(llvm::AllocaInst* variable_alloca, 
                                  llvm::Value* header_ptr,
                                  const ClassTypeInfo* class_info,
                                  const std::string& debug_name = "");
    
    // ARC-specific registration (header_ptr computed dynamically at cleanup time)
    void register_arc_managed_object(llvm::AllocaInst* variable_alloca,
                                      const ClassTypeInfo* class_info,
                                      const std::string& debug_name = "");
    
    void unregister_managed_object(llvm::AllocaInst* variable_alloca);
    
    // Cleanup generation
    void generate_scope_cleanup(Scope* scope, llvm::Function* current_function);
    void generate_all_active_cleanup(llvm::Function* current_function);
    void generate_cleanup_for_early_exit(llvm::Function* current_function, 
                                          ScopeType exit_scope_type = ScopeType::Function);
    void cleanup_current_scope_early(); // For break/continue statements
    
    // Control flow helpers
    void prepare_conditional_cleanup(llvm::BasicBlock* true_block, 
                                     llvm::BasicBlock* false_block,
                                     llvm::BasicBlock* merge_block);
    
    void prepare_loop_cleanup(llvm::BasicBlock* body_block,
                              llvm::BasicBlock* exit_block,
                              llvm::BasicBlock* continue_block);
    
    // Debug and diagnostics
    void dump_scope_stack() const;
    std::string get_scope_hierarchy_string() const;

private:
    void generate_object_cleanup(const ManagedObject& obj, llvm::Function* current_function);
    llvm::BasicBlock* create_cleanup_block(llvm::Function* function, const std::string& name);
    void verify_dominance_requirements(llvm::BasicBlock* cleanup_block, llvm::Function* function);
};

} // namespace Mycelium::Scripting::Lang
