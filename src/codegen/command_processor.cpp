#include "codegen/command_processor.hpp"
#include "common/logger.hpp"
#include <iostream>

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
                }
                
                if (alloca_type && cmd.result.is_valid()) {
                    llvm::Value* alloca = builder_->CreateAlloca(alloca_type);
                    value_map_[cmd.result.id] = alloca;
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
                // Parse "name:returntype"
                size_t colon_pos = func_info->find(':');
                if (colon_pos != std::string::npos) {
                    std::string name = func_info->substr(0, colon_pos);
                    std::string return_type_str = func_info->substr(colon_pos + 1);
                    
                    // Create function type
                    llvm::Type* return_type = nullptr;
                    if (return_type_str == "i32") {
                        return_type = llvm::Type::getInt32Ty(*context_);
                    } else if (return_type_str == "void") {
                        return_type = llvm::Type::getVoidTy(*context_);
                    }
                    
                    if (return_type) {
                        llvm::FunctionType* func_type = llvm::FunctionType::get(return_type, false);
                        current_function_ = llvm::Function::Create(
                            func_type, 
                            llvm::Function::ExternalLinkage,
                            name,
                            module_.get()
                        );
                        
                        // Create entry block
                        current_block_ = llvm::BasicBlock::Create(*context_, "entry", current_function_);
                        builder_->SetInsertPoint(current_block_);
                    }
                }
            }
            break;
        }
        
        case Op::FunctionEnd: {
            current_function_ = nullptr;
            current_block_ = nullptr;
            break;
        }
        
        case Op::Call: {
            // TODO: Implement function calls
            std::cerr << "Function calls not yet implemented\n";
            break;
        }
        
        default:
            std::cerr << "Unknown operation in process_command\n";
            break;
    }
}

void CommandProcessor::process(const std::vector<Command>& commands) {
    LOG_INFO("Processing " + std::to_string(commands.size()) + " commands...", LogCategory::CODEGEN);
    
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