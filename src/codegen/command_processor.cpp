#include "codegen/command_processor.hpp"
#include "common/logger.hpp"
#include <iostream>
#include <sstream>

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/raw_ostream.h"

namespace Mycelium::Scripting::Lang {

CommandProcessor::CommandProcessor(const std::string& module_name) 
    : current_function_(nullptr), current_block_(nullptr) {
    context_ = std::make_unique<llvm::LLVMContext>();
    module_ = std::make_unique<llvm::Module>(module_name, *context_);
    builder_ = std::make_unique<llvm::IRBuilder<>>(*context_);
}

CommandProcessor::~CommandProcessor() = default;

llvm::Type* CommandProcessor::to_llvm_type(IRType type) {
    switch (type.kind) {
        case IRType::Kind::Void:
            return llvm::Type::getVoidTy(*context_);
        case IRType::Kind::I32:
            return llvm::Type::getInt32Ty(*context_);
        case IRType::Kind::I64:
            return llvm::Type::getInt64Ty(*context_);
        case IRType::Kind::I8:
            return llvm::Type::getInt8Ty(*context_);
        case IRType::Kind::I16:
            return llvm::Type::getInt16Ty(*context_);
        case IRType::Kind::Bool:
            return llvm::Type::getInt1Ty(*context_);
        case IRType::Kind::F32:
            return llvm::Type::getFloatTy(*context_);
        case IRType::Kind::F64:
            return llvm::Type::getDoubleTy(*context_);
        case IRType::Kind::Ptr:
            return llvm::PointerType::getUnqual(*context_);
        case IRType::Kind::Struct:
            if (type.struct_layout) {
                // Check if we already have this struct type cached
                auto cache_it = struct_type_cache_.find(type.struct_layout->name);
                if (cache_it != struct_type_cache_.end()) {
                    return cache_it->second;
                }
                
                // Convert struct fields to LLVM types
                std::vector<llvm::Type*> field_types;
                for (const auto& field : type.struct_layout->fields) {
                    llvm::Type* field_type = to_llvm_type(field.type);
                    if (field_type) {
                        field_types.push_back(field_type);
                    }
                }
                
                // Create struct type and cache it
                llvm::StructType* struct_type = llvm::StructType::create(*context_, field_types, type.struct_layout->name);
                struct_type_cache_[type.struct_layout->name] = struct_type;
                return struct_type;
            }
            return nullptr;
        default:
            std::cerr << "Unknown type in to_llvm_type\n";
            return nullptr;
    }
}

llvm::Value* CommandProcessor::get_value(int id) {
    auto it = value_map_.find(id);
    if (it == value_map_.end()) {
        std::cerr << "Value with ID " << id << " not found\n";
        return nullptr;
    }
    return it->second;
}

void CommandProcessor::create_basic_blocks(const std::vector<Command>& commands) {
    // This method is now unused - BasicBlocks are created in create_function_basic_blocks
    // when we encounter FunctionBegin
}

void CommandProcessor::create_function_basic_blocks() {
    // Create all BasicBlocks for the current function to handle forward references
    if (!current_function_ || !commands_) return;
    
    bool in_current_function = false;
    std::string current_function_name;
    
    // First, find the name of the current function
    for (const auto& cmd : *commands_) {
        if (cmd.op == Op::FunctionBegin) {
            if (auto* func_info = std::get_if<std::string>(&cmd.data)) {
                size_t first_colon = func_info->find(':');
                if (first_colon != std::string::npos) {
                    std::string func_name = func_info->substr(0, first_colon);
                    if (current_function_->getName() == func_name) {
                        in_current_function = true;
                        current_function_name = func_name;
                        break;
                    }
                }
            }
        }
    }
    
    if (!in_current_function) return;
    
    // Now scan through commands to find labels for THIS function only
    in_current_function = false;
    for (const auto& cmd : *commands_) {
        if (cmd.op == Op::FunctionBegin) {
            if (auto* func_info = std::get_if<std::string>(&cmd.data)) {
                size_t first_colon = func_info->find(':');
                if (first_colon != std::string::npos) {
                    std::string func_name = func_info->substr(0, first_colon);
                    in_current_function = (func_name == current_function_name);
                }
            }
        } else if (cmd.op == Op::FunctionEnd) {
            if (in_current_function) {
                break; // Stop when we exit the current function
            }
        } else if (cmd.op == Op::Label && in_current_function) {
            if (auto* label_name = std::get_if<std::string>(&cmd.data)) {
                // Create BasicBlock for this label
                llvm::BasicBlock* block = llvm::BasicBlock::Create(*context_, *label_name, current_function_);
                block_map_[*label_name] = block;
                LOG_DEBUG("Created BasicBlock for label '" + *label_name + "' in function '" + current_function_name + "'", LogCategory::CODEGEN);
            }
        }
    }
    
    LOG_INFO("Created " + std::to_string(block_map_.size()) + " BasicBlocks for function '" + current_function_name + "'", LogCategory::CODEGEN);
}

void CommandProcessor::process_command(const Command& cmd) {
    switch (cmd.op) {
        case Op::Const: {
            llvm::Value* constant = nullptr;
            
            if (auto* int_val = std::get_if<int64_t>(&cmd.data)) {
                if (cmd.result.type.kind == IRType::Kind::I32) {
                    constant = llvm::ConstantInt::get(to_llvm_type(cmd.result.type), *int_val, true);
                } else if (cmd.result.type.kind == IRType::Kind::I64) {
                    constant = llvm::ConstantInt::get(to_llvm_type(cmd.result.type), *int_val, true);
                }
            } else if (auto* bool_val = std::get_if<bool>(&cmd.data)) {
                constant = llvm::ConstantInt::get(to_llvm_type(cmd.result.type), *bool_val ? 1 : 0);
            } else if (auto* float_val = std::get_if<double>(&cmd.data)) {
                if (cmd.result.type.kind == IRType::Kind::F32) {
                    constant = llvm::ConstantFP::get(to_llvm_type(cmd.result.type), *float_val);
                } else if (cmd.result.type.kind == IRType::Kind::F64) {
                    constant = llvm::ConstantFP::get(to_llvm_type(cmd.result.type), *float_val);
                }
            }
            
            if (constant && cmd.result.is_valid()) {
                value_map_[cmd.result.id] = constant;
            }
            break;
        }
        
        case Op::Add: {
            llvm::Value* lhs = get_value(cmd.args[0].id);
            llvm::Value* rhs = get_value(cmd.args[1].id);
            if (lhs && rhs && cmd.result.is_valid()) {
                llvm::Value* result = builder_->CreateAdd(lhs, rhs);
                value_map_[cmd.result.id] = result;
            }
            break;
        }
        
        case Op::Sub: {
            llvm::Value* lhs = get_value(cmd.args[0].id);
            llvm::Value* rhs = get_value(cmd.args[1].id);
            if (lhs && rhs && cmd.result.is_valid()) {
                llvm::Value* result = builder_->CreateSub(lhs, rhs);
                value_map_[cmd.result.id] = result;
            }
            break;
        }
        
        case Op::Mul: {
            llvm::Value* lhs = get_value(cmd.args[0].id);
            llvm::Value* rhs = get_value(cmd.args[1].id);
            if (lhs && rhs && cmd.result.is_valid()) {
                llvm::Value* result = builder_->CreateMul(lhs, rhs);
                value_map_[cmd.result.id] = result;
            }
            break;
        }
        
        case Op::Div: {
            llvm::Value* lhs = get_value(cmd.args[0].id);
            llvm::Value* rhs = get_value(cmd.args[1].id);
            if (lhs && rhs && cmd.result.is_valid()) {
                // Use signed division for integers
                llvm::Value* result = builder_->CreateSDiv(lhs, rhs);
                value_map_[cmd.result.id] = result;
            }
            break;
        }
        
        case Op::ICmp: {
            llvm::Value* lhs = get_value(cmd.args[0].id);
            llvm::Value* rhs = get_value(cmd.args[1].id);
            if (lhs && rhs && cmd.result.is_valid()) {
                if (auto* pred = std::get_if<ICmpPredicate>(&cmd.data)) {
                    llvm::CmpInst::Predicate llvm_pred;
                    switch (*pred) {
                        case ICmpPredicate::Eq: llvm_pred = llvm::CmpInst::ICMP_EQ; break;
                        case ICmpPredicate::Ne: llvm_pred = llvm::CmpInst::ICMP_NE; break;
                        case ICmpPredicate::Slt: llvm_pred = llvm::CmpInst::ICMP_SLT; break;
                        case ICmpPredicate::Sle: llvm_pred = llvm::CmpInst::ICMP_SLE; break;
                        case ICmpPredicate::Sgt: llvm_pred = llvm::CmpInst::ICMP_SGT; break;
                        case ICmpPredicate::Sge: llvm_pred = llvm::CmpInst::ICMP_SGE; break;
                        case ICmpPredicate::Ult: llvm_pred = llvm::CmpInst::ICMP_ULT; break;
                        case ICmpPredicate::Ule: llvm_pred = llvm::CmpInst::ICMP_ULE; break;
                        case ICmpPredicate::Ugt: llvm_pred = llvm::CmpInst::ICMP_UGT; break;
                        case ICmpPredicate::Uge: llvm_pred = llvm::CmpInst::ICMP_UGE; break;
                        default: llvm_pred = llvm::CmpInst::ICMP_EQ; break;
                    }
                    llvm::Value* result = builder_->CreateICmp(llvm_pred, lhs, rhs);
                    value_map_[cmd.result.id] = result;
                }
            }
            break;
        }
        
        case Op::And: {
            llvm::Value* lhs = get_value(cmd.args[0].id);
            llvm::Value* rhs = get_value(cmd.args[1].id);
            if (lhs && rhs && cmd.result.is_valid()) {
                llvm::Value* result = builder_->CreateAnd(lhs, rhs);
                value_map_[cmd.result.id] = result;
            }
            break;
        }
        
        case Op::Or: {
            llvm::Value* lhs = get_value(cmd.args[0].id);
            llvm::Value* rhs = get_value(cmd.args[1].id);
            if (lhs && rhs && cmd.result.is_valid()) {
                llvm::Value* result = builder_->CreateOr(lhs, rhs);
                value_map_[cmd.result.id] = result;
            }
            break;
        }
        
        case Op::Not: {
            llvm::Value* operand = get_value(cmd.args[0].id);
            if (operand && cmd.result.is_valid()) {
                // Use LLVM's built-in CreateNot which handles type correctly
                llvm::Value* result = builder_->CreateNot(operand);
                value_map_[cmd.result.id] = result;
            }
            break;
        }
        
        case Op::Alloca: {
            if (auto* type_str = std::get_if<std::string>(&cmd.data)) {
                // Parse the type string to get the actual type
                llvm::Type* alloca_type = nullptr;
                if (*type_str == "i32") {
                    alloca_type = llvm::Type::getInt32Ty(*context_);
                } else if (*type_str == "i64") {
                    alloca_type = llvm::Type::getInt64Ty(*context_);
                } else if (*type_str == "i1") {
                    alloca_type = llvm::Type::getInt1Ty(*context_);
                } else if (*type_str == "ptr") {
                    alloca_type = llvm::PointerType::getUnqual(*context_);
                } else if (type_str->starts_with("struct.")) {
                    // Extract struct name from "struct.StructName" format
                    std::string struct_name = type_str->substr(7); // Skip "struct."
                    
                    // Use the cached struct type if available
                    auto cache_it = struct_type_cache_.find(struct_name);
                    if (cache_it != struct_type_cache_.end()) {
                        alloca_type = cache_it->second;
                    } else {
                        // The struct type hasn't been created yet - we need to create it
                        // This can happen when 'new StructName()' is processed before any member access
                        // For now, we'll defer this by using the result type of the command
                        if (cmd.result.type.pointee_type && cmd.result.type.pointee_type->kind == IRType::Kind::Struct) {
                            alloca_type = to_llvm_type(*cmd.result.type.pointee_type);
                        }
                        
                        if (!alloca_type) {
                            std::cerr << "Error: Could not resolve struct type '" << struct_name << "' during alloca\n";
                        }
                    }
                } else if (*type_str == "struct") {
                    // For generic struct types, we need to get the proper type from the IR
                    // For now, create a generic struct with the layout we know
                    // TODO: Use the actual struct layout information
                    std::vector<llvm::Type*> field_types = {llvm::Type::getInt32Ty(*context_)};
                    alloca_type = llvm::StructType::create(*context_, field_types, "Player");
                }
                
                if (alloca_type && cmd.result.is_valid()) {
                    llvm::Value* alloca = builder_->CreateAlloca(alloca_type);
                    value_map_[cmd.result.id] = alloca;
                    
                    // If this is a parameter allocation, store the function argument
                    if (current_alloca_index_ < param_count_ && current_function_) {
                        auto arg_it = current_function_->arg_begin();
                        std::advance(arg_it, current_alloca_index_);
                        builder_->CreateStore(&*arg_it, alloca);
                    }
                    current_alloca_index_++;
                }
            }
            break;
        }
        
        case Op::Store: {
            llvm::Value* value = get_value(cmd.args[0].id);
            llvm::Value* ptr = get_value(cmd.args[1].id);
            if (value && ptr) {
                builder_->CreateStore(value, ptr);
            }
            break;
        }
        
        case Op::Load: {
            llvm::Value* ptr = get_value(cmd.args[0].id);
            if (ptr && cmd.result.is_valid()) {
                llvm::Type* load_type = to_llvm_type(cmd.result.type);
                llvm::Value* loaded = builder_->CreateLoad(load_type, ptr);
                value_map_[cmd.result.id] = loaded;
            }
            break;
        }
        
        case Op::GEP: {
            llvm::Value* ptr = get_value(cmd.args[0].id);
            if (ptr && cmd.result.is_valid()) {
                // Parse indices from data string
                std::vector<llvm::Value*> indices;
                
                if (auto* indices_str = std::get_if<std::string>(&cmd.data)) {
                    // First index is always 0 for struct field access
                    indices.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), 0));
                    
                    // Parse comma-separated indices
                    std::stringstream ss(*indices_str);
                    std::string index;
                    while (std::getline(ss, index, ',')) {
                        int idx = std::stoi(index);
                        indices.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), idx));
                    }
                }
                
                // Get the pointee type from the pointer argument type
                // For GEP with opaque pointers, we need the struct type that the pointer points to
                llvm::Type* struct_type = nullptr;
                if (cmd.args.size() > 0 && cmd.args[0].type.pointee_type) {
                    struct_type = to_llvm_type(*cmd.args[0].type.pointee_type);
                    if (!struct_type) {
                        std::cerr << "Error: Failed to convert pointee type to LLVM type in GEP\n";
                        break;
                    }
                } else {
                    std::cerr << "Error: GEP requires pointer with known pointee type, but pointer type info is missing\n";
                    break;
                }
                
                llvm::Value* gep = builder_->CreateGEP(struct_type, ptr, indices);
                value_map_[cmd.result.id] = gep;
            }
            break;
        }
        
        case Op::Label: {
            if (auto* label_name = std::get_if<std::string>(&cmd.data)) {
                // Create BasicBlock on-demand if it doesn't exist
                auto it = block_map_.find(*label_name);
                if (it == block_map_.end()) {
                    // Create new BasicBlock for this label
                    if (current_function_) {
                        llvm::BasicBlock* block = llvm::BasicBlock::Create(*context_, *label_name, current_function_);
                        block_map_[*label_name] = block;
                        LOG_DEBUG("Created BasicBlock on-demand for label '" + *label_name + "'", LogCategory::CODEGEN);
                        it = block_map_.find(*label_name);
                    } else {
                        std::cerr << "Error: No current function when creating label '" << *label_name << "'" << std::endl;
                        break;
                    }
                }
                
                // Before switching to the new block, ensure the current block has a terminator
                if (current_block_ && current_block_->getTerminator() == nullptr) {
                    // Add an unreachable instruction to blocks that don't have explicit terminators
                    builder_->CreateUnreachable();
                    LOG_DEBUG("Added unreachable terminator to previous block", LogCategory::CODEGEN);
                }
                
                current_block_ = it->second;
                builder_->SetInsertPoint(current_block_);
                LOG_DEBUG("Set insert point to label '" + *label_name + "'", LogCategory::CODEGEN);
            }
            break;
        }
        
        case Op::Br: {
            if (auto* target_label = std::get_if<std::string>(&cmd.data)) {
                // Create BasicBlock on-demand if it doesn't exist
                auto it = block_map_.find(*target_label);
                if (it == block_map_.end()) {
                    if (current_function_) {
                        llvm::BasicBlock* block = llvm::BasicBlock::Create(*context_, *target_label, current_function_);
                        block_map_[*target_label] = block;
                        LOG_DEBUG("Created BasicBlock on-demand for branch target '" + *target_label + "'", LogCategory::CODEGEN);
                        it = block_map_.find(*target_label);
                    } else {
                        std::cerr << "Error: No current function when creating branch target '" << *target_label << "'" << std::endl;
                        break;
                    }
                }
                builder_->CreateBr(it->second);
            }
            break;
        }
        
        case Op::BrCond: {
            if (auto* labels = std::get_if<std::string>(&cmd.data)) {
                size_t comma = labels->find(',');
                if (comma != std::string::npos && !cmd.args.empty()) {
                    std::string true_label = labels->substr(0, comma);
                    std::string false_label = labels->substr(comma + 1);
                    
                    // Create BasicBlocks on-demand if they don't exist
                    auto true_it = block_map_.find(true_label);
                    if (true_it == block_map_.end()) {
                        if (current_function_) {
                            llvm::BasicBlock* block = llvm::BasicBlock::Create(*context_, true_label, current_function_);
                            block_map_[true_label] = block;
                            LOG_DEBUG("Created BasicBlock on-demand for conditional branch true target '" + true_label + "'", LogCategory::CODEGEN);
                            true_it = block_map_.find(true_label);
                        } else {
                            std::cerr << "Error: No current function when creating conditional branch true target '" << true_label << "'" << std::endl;
                            break;
                        }
                    }
                    
                    auto false_it = block_map_.find(false_label);
                    if (false_it == block_map_.end()) {
                        if (current_function_) {
                            llvm::BasicBlock* block = llvm::BasicBlock::Create(*context_, false_label, current_function_);
                            block_map_[false_label] = block;
                            LOG_DEBUG("Created BasicBlock on-demand for conditional branch false target '" + false_label + "'", LogCategory::CODEGEN);
                            false_it = block_map_.find(false_label);
                        } else {
                            std::cerr << "Error: No current function when creating conditional branch false target '" << false_label << "'" << std::endl;
                            break;
                        }
                    }
                    
                    llvm::Value* condition = get_value(cmd.args[0].id);
                    if (condition) {
                        builder_->CreateCondBr(condition, true_it->second, false_it->second);
                    }
                }
            }
            break;
        }
        
        case Op::Ret: {
            llvm::Value* value = get_value(cmd.args[0].id);
            if (value) {
                builder_->CreateRet(value);
            }
            break;
        }
        
        case Op::RetVoid: {
            builder_->CreateRetVoid();
            break;
        }
        
        case Op::FunctionBegin: {
            if (auto* func_info = std::get_if<std::string>(&cmd.data)) {
                // Parse "name:returntype" or "name:returntype:param1,param2,..."
                // Handle member functions like "Type::method:returntype:params"
                
                // Split by finding the first ':' that's not part of '::'
                size_t name_end = std::string::npos;
                for (size_t i = 0; i < func_info->length(); ++i) {
                    if ((*func_info)[i] == ':') {
                        // Check if it's part of '::'
                        if (i + 1 < func_info->length() && (*func_info)[i + 1] == ':') {
                            i++; // Skip the second ':'
                            continue;
                        }
                        // Found a single ':'
                        name_end = i;
                        break;
                    }
                }
                
                if (name_end != std::string::npos) {
                    std::string name = func_info->substr(0, name_end);
                    std::string remainder = func_info->substr(name_end + 1);
                    
                    size_t second_colon = remainder.find(':');
                    std::string return_type_str;
                    std::string param_types_str;
                    
                    if (second_colon != std::string::npos) {
                        return_type_str = remainder.substr(0, second_colon);
                        param_types_str = remainder.substr(second_colon + 1);
                    } else {
                        return_type_str = remainder;
                    }
                    
                    // Create return type
                    llvm::Type* return_type = to_llvm_type(IRType::void_());
                    if (return_type_str == "i32") {
                        return_type = llvm::Type::getInt32Ty(*context_);
                    } else if (return_type_str == "void") {
                        return_type = llvm::Type::getVoidTy(*context_);
                    } else if (return_type_str == "bool") {
                        return_type = llvm::Type::getInt1Ty(*context_);
                    } else if (return_type_str == "i1") {
                        return_type = llvm::Type::getInt1Ty(*context_);
                    } else if (!return_type_str.empty()) {
                        std::cerr << "Warning: Unknown return type '" << return_type_str << "', using default void" << std::endl;
                    }
                    
                    // Parse parameter types
                    std::vector<llvm::Type*> param_types;
                    if (!param_types_str.empty()) {
                        std::string current_param;
                        for (char c : param_types_str) {
                            if (c == ',') {
                                if (!current_param.empty()) {
                                    if (current_param == "i32") {
                                        param_types.push_back(llvm::Type::getInt32Ty(*context_));
                                    } else if (current_param == "bool") {
                                        param_types.push_back(llvm::Type::getInt1Ty(*context_));
                                    } else if (current_param == "ptr") {
                                        // For now, use opaque pointer type (i8*)
                                        param_types.push_back(llvm::PointerType::get(*context_, 0));
                                    } else {
                                        std::cerr << "Unknown parameter type: " << current_param << std::endl;
                                    }
                                    current_param.clear();
                                }
                            } else {
                                current_param += c;
                            }
                        }
                        // Handle last parameter
                        if (!current_param.empty()) {
                            if (current_param == "i32") {
                                param_types.push_back(llvm::Type::getInt32Ty(*context_));
                            } else if (current_param == "bool") {
                                param_types.push_back(llvm::Type::getInt1Ty(*context_));
                            } else if (current_param == "ptr") {
                                // For now, use opaque pointer type (i8*)
                                param_types.push_back(llvm::PointerType::get(*context_, 0));
                            } else {
                                std::cerr << "Unknown parameter type: " << current_param << std::endl;
                            }
                        }
                    }
                    
                    if (return_type) {
                        llvm::FunctionType* func_type = llvm::FunctionType::get(return_type, param_types, false);
                        current_function_ = llvm::Function::Create(
                            func_type, 
                            llvm::Function::ExternalLinkage,
                            name,
                            module_.get()
                        );
                        LOG_DEBUG("Created LLVM function: '" + name + "' with " + std::to_string(param_types.size()) + " parameters", LogCategory::CODEGEN);
                        
                        // Create entry block
                        current_block_ = llvm::BasicBlock::Create(*context_, "entry", current_function_);
                        builder_->SetInsertPoint(current_block_);
                        
                        // Store function arguments for access by parameter allocations
                        // The first few allocations in the function will be for parameters
                        param_count_ = param_types.size();
                        current_alloca_index_ = 0;
                        
                        // Don't pre-create basic blocks - create them on-demand when referenced
                        // create_function_basic_blocks();
                    }
                }
            }
            break;
        }
        
        case Op::FunctionEnd: {
            current_function_ = nullptr;
            current_block_ = nullptr;
            block_map_.clear();  // Clear block map for next function
            break;
        }
        
        case Op::Call: {
            if (auto* func_name = std::get_if<std::string>(&cmd.data)) {
                // Look up the function in the module
                llvm::Function* callee = module_->getFunction(*func_name);
                if (!callee) {
                    std::cerr << "Error: Function '" << *func_name << "' not found\n";
                    break;
                }
                
                // Collect argument values
                std::vector<llvm::Value*> args;
                for (const auto& arg : cmd.args) {
                    llvm::Value* arg_val = get_value(arg.id);
                    if (!arg_val) {
                        std::cerr << "Error: Argument value with ID " << arg.id << " not found\n";
                        break;
                    }
                    args.push_back(arg_val);
                }
                
                // Create the call instruction
                llvm::Value* call_result = builder_->CreateCall(callee, args);
                if (cmd.result.is_valid()) {
                    value_map_[cmd.result.id] = call_result;
                }
            }
            break;
        }
        
        default:
            std::cerr << "Unknown operation in process_command\n";
            break;
    }
}

void CommandProcessor::process(const std::vector<Command>& commands) {
    LOG_INFO("Processing " + std::to_string(commands.size()) + " commands...", LogCategory::CODEGEN);
    
    // Store commands for two-pass processing
    commands_ = &commands;
    
    // Debug logging removed - fix is working
    
    // Process all commands (BasicBlocks will be created when we hit FunctionBegin)
    for (const auto& cmd : commands) {
        process_command(cmd);
    }
    
    LOG_INFO("Command processing complete.", LogCategory::CODEGEN);
}

void CommandProcessor::dump_module() {
    if (module_) {
        module_->print(llvm::outs(), nullptr);
    }
}

std::string CommandProcessor::get_ir_string() {
    if (!module_) {
        return "";
    }
    
    std::string ir_string;
    llvm::raw_string_ostream stream(ir_string);
    module_->print(stream, nullptr);
    return ir_string;
}

bool CommandProcessor::verify_module() {
    if (!module_) {
        return false;
    }
    
    std::string error_msg;
    llvm::raw_string_ostream error_stream(error_msg);
    
    bool is_valid = !llvm::verifyModule(*module_, &error_stream);
    
    if (!is_valid) {
        std::cerr << "Module verification failed:\n" << error_msg << std::endl;
    }
    
    return is_valid;
}

std::unique_ptr<llvm::LLVMContext> CommandProcessor::take_context() {
    return std::move(context_);
}

std::unique_ptr<llvm::Module> CommandProcessor::take_module() {
    return std::move(module_);
}

std::string CommandProcessor::process_to_ir_string(const std::vector<Command>& commands, const std::string& module_name) {
    CommandProcessor processor(module_name);
    processor.process(commands);
    
    if (processor.verify_module()) {
        return processor.get_ir_string();
    } else {
        std::cerr << "CommandProcessor: Module verification failed" << std::endl;
        return "";
    }
}

} // namespace Mycelium::Scripting::Lang