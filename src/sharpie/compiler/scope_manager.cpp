#include "sharpie/compiler/scope_manager.hpp"
#include "sharpie/common/logger.hpp"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include <iostream>
#include <algorithm>

namespace Mycelium::Scripting::Lang
{

    void ScopeManager::reset(llvm::IRBuilder<> *ir_builder, llvm::Module *llvm_module)
    {
        this->builder = ir_builder;
        this->module = llvm_module;
        this->scope_stack.clear();
        LOG_DEBUG("ScopeManager has been reset for a new compilation run.", "SCOPE");
    }

    void ScopeManager::push_scope(ScopeType type, const std::string &debug_name)
    {
        std::string name = debug_name.empty() ? ("scope_" + std::to_string(scope_stack.size())) : debug_name;

        auto scope = std::make_unique<Scope>(type, name);
        scope_stack.push_back(std::move(scope));

        std::cout << "[ScopeManager] Pushed scope: " << name
                  << " (depth: " << scope_stack.size() << ")" << std::endl;
    }

    void ScopeManager::pop_scope()
    {
        if (scope_stack.empty())
        {
            // Silently handle empty scope pops - this can happen in some valid scenarios
            // like when destructors or other cleanup code doesn't need scope management
            return;
        }

        Scope *current = scope_stack.back().get();

        std::cout << "[ScopeManager] Popping scope: " << current->debug_name
                  << " with " << current->managed_objects.size() << " managed objects" << std::endl;

        // Generate cleanup for objects in this scope
        if (current->has_managed_objects())
        {
            llvm::Function *current_function = builder->GetInsertBlock()->getParent();
            generate_scope_cleanup(current, current_function);
        }

        scope_stack.pop_back();
    }

    Scope *ScopeManager::get_current_scope()
    {
        if (scope_stack.empty())
        {
            return nullptr;
        }
        return scope_stack.back().get();
    }

    void ScopeManager::register_managed_object(llvm::AllocaInst *variable_alloca,
                                               llvm::Value *header_ptr,
                                               const SymbolTable::ClassSymbol *class_info,
                                               const std::string &debug_name)
    {
        Scope *current = get_current_scope();
        if (!current)
        {
            std::cerr << "[ScopeManager] ERROR: Cannot register object - no active scope!" << std::endl;
            return;
        }

        // CRITICAL FIX: Only register each unique object (by header_ptr) once
        // Multiple variables may point to the same object via ARC, but we should only
        // clean up each object once when it was originally created via 'new'
        for (const auto &existing_obj : current->managed_objects)
        {
            if (existing_obj.header_ptr == header_ptr)
            {
                LOG_DEBUG("[ScopeManager] Object with header_ptr=" + std::to_string(reinterpret_cast<uintptr_t>(header_ptr)) +
                                                           " already registered - skipping duplicate registration for '" + debug_name + "'",
                                                       "SCOPE");
                return; // Object already tracked, don't register again
            }
        }

        ManagedObject obj(variable_alloca, header_ptr, class_info, debug_name);
        current->add_managed_object(obj);

        // Log registration for debugging double-free issues
        LOG_DEBUG("[ScopeManager] Registered object '" + debug_name +
                                                   "' in scope '" + current->debug_name +
                                                   "' header_ptr=" + std::to_string(reinterpret_cast<uintptr_t>(header_ptr)) +
                                                   " alloca=" + std::to_string(reinterpret_cast<uintptr_t>(variable_alloca)),
                                               "SCOPE");
    }

    void ScopeManager::register_arc_managed_object(llvm::AllocaInst *variable_alloca,
                                                   const SymbolTable::ClassSymbol *class_info,
                                                   const std::string &debug_name)
    {
        Scope *current = get_current_scope();
        if (!current)
        {
            std::cerr << "[ScopeManager] ERROR: Cannot register ARC object - no active scope!" << std::endl;
            return;
        }

        // For ARC objects, we pass nullptr as header_ptr since it will be computed dynamically
        // This avoids the double-free issues caused by using stale header pointers
        ManagedObject obj(variable_alloca, nullptr, class_info, debug_name);
        current->add_managed_object(obj);

        LOG_DEBUG("[ScopeManager] Registered ARC object '" + debug_name +
                                                   "' in scope '" + current->debug_name +
                                                   "' alloca=" + std::to_string(reinterpret_cast<uintptr_t>(variable_alloca)) + " (header computed dynamically)",
                                               "SCOPE");
    }

    void ScopeManager::unregister_managed_object(llvm::AllocaInst *variable_alloca)
    {
        // Remove from all scopes (in case of reassignment)
        for (auto &scope : scope_stack)
        {
            auto &objects = scope->managed_objects;
            objects.erase(
                std::remove_if(objects.begin(), objects.end(),
                               [variable_alloca](const ManagedObject &obj)
                               {
                                   return obj.variable_alloca == variable_alloca;
                               }),
                objects.end());
        }
    }

    void ScopeManager::generate_scope_cleanup(Scope *scope, llvm::Function *current_function)
    {
        if (!scope || !scope->has_managed_objects())
        {
            return;
        }

        // Generate cleanup for all objects in reverse order (LIFO)
        auto cleanup_objects = scope->get_cleanup_order();
        for (const auto &obj : cleanup_objects)
        {
            generate_object_cleanup(obj, current_function);
        }
    }

    void ScopeManager::generate_all_active_cleanup(llvm::Function *current_function)
    {
        // Generate cleanup for all active scopes in reverse order
        for (auto it = scope_stack.rbegin(); it != scope_stack.rend(); ++it)
        {
            generate_scope_cleanup(it->get(), current_function);
        }
    }

    void ScopeManager::generate_cleanup_for_early_exit(llvm::Function *current_function,
                                                       ScopeType exit_scope_type)
    {
        // Clean up all scopes until we reach the target scope type
        for (auto it = scope_stack.rbegin(); it != scope_stack.rend(); ++it)
        {
            generate_scope_cleanup(it->get(), current_function);
            if ((*it)->type == exit_scope_type)
            {
                break;
            }
        }
    }

    void ScopeManager::cleanup_current_scope_early()
    {
        // This method is called for break/continue statements to ensure
        // proper object cleanup before jumping out of the current scope
        Scope *current = get_current_scope();
        if (!current || !current->has_managed_objects())
        {
            return;
        }

        llvm::Function *current_function = builder->GetInsertBlock()->getParent();
        generate_scope_cleanup(current, current_function);

        // Clear the objects since they've been cleaned up
        current->managed_objects.clear();
    }

    void ScopeManager::generate_object_cleanup(const ManagedObject &obj, llvm::Function *current_function)
    {
        if (!obj.class_info)
        {
            LOG_DEBUG("[ScopeManager] Skipping cleanup for '" + obj.debug_name +
                                                       "' - missing class_info",
                                                   "SCOPE");
            return;
        }

        // For ARC objects, header_ptr is nullptr and will be computed dynamically
        LOG_DEBUG("[ScopeManager] Generating cleanup for ARC object '" + obj.debug_name +
                                                   "' (header computed dynamically)",
                                               "SCOPE");

        // Load the current value from the variable (this is the fields pointer)
        llvm::Value *current_value = builder->CreateLoad(
            obj.variable_alloca->getAllocatedType(),
            obj.variable_alloca,
            obj.debug_name + ".cleanup.load");

        // CRITICAL FIX: The problem is we're trying to use the stored header_ptr from registration time,
        // but the variable might now point to a different object or null.
        // We need to recalculate the header_ptr from the CURRENT variable value, not use the stored one.

        // Check if the current variable value is not null
        llvm::Value *is_not_null = builder->CreateICmpNE(
            current_value,
            llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(current_value->getType())),
            obj.debug_name + ".null.check");

        // Create cleanup blocks
        llvm::BasicBlock *cleanup_block = create_cleanup_block(current_function,
                                                               obj.debug_name + ".cleanup");
        llvm::BasicBlock *after_cleanup_block = create_cleanup_block(current_function,
                                                                     obj.debug_name + ".after.cleanup");

        // Conditional branch to cleanup
        builder->CreateCondBr(is_not_null, cleanup_block, after_cleanup_block);

        // Generate cleanup code
        builder->SetInsertPoint(cleanup_block);

        // Call destructor if available (uses fields pointer)
        if (obj.class_info->destructor_func)
        {
            builder->CreateCall(obj.class_info->destructor_func, {current_value});
        }

        // CRITICAL FIX: Calculate header_ptr from CURRENT variable value, not stored value
        // This ensures we get the correct header for whatever object the variable currently points to
        llvm::Function *release_func = module->getFunction("Mycelium_Object_release");
        if (release_func)
        {
            // Calculate the current header pointer from the current fields pointer
            llvm::Value *current_header_ptr = nullptr;
            if (obj.class_info->fieldsType)
            {
                // Use a constant header size (16 bytes for MyceliumObjectHeader: ref_count + type_id + vtable_ptr)
                const uint64_t header_size = 16;
                llvm::Value *offset = llvm::ConstantInt::get(llvm::Type::getInt64Ty(builder->getContext()), -static_cast<int64_t>(header_size));
                current_header_ptr = builder->CreateGEP(llvm::Type::getInt8Ty(builder->getContext()), current_value, offset, obj.debug_name + ".current.header");
            }

            if (current_header_ptr)
            {
                builder->CreateCall(release_func, {current_header_ptr});
            }
        }

        builder->CreateBr(after_cleanup_block);

        // Continue after cleanup
        builder->SetInsertPoint(after_cleanup_block);
    }

    llvm::BasicBlock *ScopeManager::create_cleanup_block(llvm::Function *function, const std::string &name)
    {
        return llvm::BasicBlock::Create(builder->getContext(), name, function);
    }

    void ScopeManager::prepare_conditional_cleanup(llvm::BasicBlock *true_block,
                                                   llvm::BasicBlock *false_block,
                                                   llvm::BasicBlock *merge_block)
    {
        // This method can be used to prepare cleanup for conditional blocks
        // Implementation depends on specific control flow requirements
    }

    void ScopeManager::prepare_loop_cleanup(llvm::BasicBlock *body_block,
                                            llvm::BasicBlock *exit_block,
                                            llvm::BasicBlock *continue_block)
    {
        // This method can be used to prepare cleanup for loop blocks
        // Implementation depends on specific control flow requirements
    }

    void ScopeManager::verify_dominance_requirements(llvm::BasicBlock *cleanup_block, llvm::Function *function)
    {
        // Verify that the cleanup block is properly dominated
        // This is a placeholder for dominance verification logic
    }

    void ScopeManager::dump_scope_stack() const
    {
        std::cout << "[ScopeManager] Scope Stack (depth: " << scope_stack.size() << "):" << std::endl;
        for (size_t i = 0; i < scope_stack.size(); ++i)
        {
            const auto &scope = scope_stack[i];
            std::cout << "  [" << i << "] " << scope->debug_name
                      << " (type: " << static_cast<int>(scope->type)
                      << ", objects: " << scope->managed_objects.size() << ")" << std::endl;

            for (const auto &obj : scope->managed_objects)
            {
                std::cout << "    - " << obj.debug_name << std::endl;
            }
        }
    }

    std::string ScopeManager::get_scope_hierarchy_string() const
    {
        std::string result = "";
        for (size_t i = 0; i < scope_stack.size(); ++i)
        {
            if (i > 0)
                result += " -> ";
            result += scope_stack[i]->debug_name;
        }
        return result;
    }

} // namespace Mycelium::Scripting::Lang