#pragma once

#include "codegen/ir_command.hpp"
#include "semantic/error_system.hpp"
#include <string>
#include <memory>

namespace Myre {

// === IR MODULE ===

class IRModule {
private:
    std::string llvm_ir_;
    std::string module_name_;
    
public:
    IRModule(std::string module_name, std::string llvm_ir)
        : llvm_ir_(std::move(llvm_ir)), module_name_(std::move(module_name)) {}
    
    const std::string& llvm_ir() const { return llvm_ir_; }
    const std::string& module_name() const { return module_name_; }
    
    void write_to_file(const std::string& filename) const {
        // TODO: Implement file writing
    }
    
    std::string to_string() const {
        return "Module '" + module_name_ + "' (" + std::to_string(llvm_ir_.length()) + " chars)";
    }
};

// === IR BUILDER ===

class IRBuilder {
private:
    std::string module_name_;
    
public:
    explicit IRBuilder(std::string module_name = "DefaultModule")
        : module_name_(std::move(module_name)) {}
    
    // Main entry point - convert command stream to LLVM IR
    Result<IRModule> build_ir(const CommandStream& commands) {
        if (!commands.is_finalized()) {
            return codegen_error("Cannot build IR from non-finalized command stream");
        }
        
        try {
            std::string ir = generate_llvm_ir(commands);
            return success(IRModule(module_name_, std::move(ir)));
        } catch (const std::exception& e) {
            return codegen_error("LLVM IR generation failed: " + std::string(e.what()));
        }
    }
    
private:
    std::string generate_llvm_ir(const CommandStream& commands) {
        std::string ir;
        
        // Module header
        ir += "; ModuleID = '" + module_name_ + "'\n";
        ir += "source_filename = \"" + module_name_ + "\"\n\n";
        
        // Process commands
        for (const auto& command : commands) {
            ir += process_command(command);
        }
        
        return ir;
    }
    
    std::string process_command(const IRCommand& command) {
        switch (command.opcode()) {
            case OpCode::ConstantI32:
                return process_constant_i32(command);
            case OpCode::ConstantBool:
                return process_constant_bool(command);
            case OpCode::Alloca:
                return process_alloca(command);
            case OpCode::Load:
                return process_load(command);
            case OpCode::Store:
                return process_store(command);
            case OpCode::GEP:
                return process_gep(command);
            case OpCode::Add:
                return process_add(command);
            case OpCode::ICmpEQ:
                return process_icmp_eq(command);
            case OpCode::Label:
                return process_label(command);
            case OpCode::Branch:
                return process_branch(command);
            case OpCode::BranchCond:
                return process_branch_cond(command);
            case OpCode::Return:
                return process_return(command);
            case OpCode::Call:
                return process_call(command);
            case OpCode::FuncDecl:
                return process_func_decl(command);
            default:
                throw std::runtime_error("Unsupported opcode: " + command.opcode_string());
        }
    }
    
    std::string process_constant_i32(const IRCommand& command) {
        if (!command.has_result() || command.args().empty()) {
            throw std::runtime_error("Invalid constant_i32 command");
        }
        
        const auto& constant_arg = std::get<ConstantArg>(command.args()[0]);
        int32_t value = std::get<int32_t>(constant_arg.value);
        
        return "  " + command.result().to_string() + " = add i32 0, " + std::to_string(value) + "\n";
    }
    
    std::string process_constant_bool(const IRCommand& command) {
        if (!command.has_result() || command.args().empty()) {
            throw std::runtime_error("Invalid constant_bool command");
        }
        
        const auto& constant_arg = std::get<ConstantArg>(command.args()[0]);
        bool value = std::get<bool>(constant_arg.value);
        
        return "  " + command.result().to_string() + " = add i1 0, " + (value ? "1" : "0") + "\n";
    }
    
    std::string process_alloca(const IRCommand& command) {
        if (!command.has_result() || !command.type_hint()) {
            throw std::runtime_error("Invalid alloca command");
        }
        
        std::string type_str = type_to_llvm_string(*command.type_hint());
        return "  " + command.result().to_string() + " = alloca " + type_str + ", align 8\n";
    }
    
    std::string process_load(const IRCommand& command) {
        if (!command.has_result() || command.args().size() != 1) {
            throw std::runtime_error("Invalid load command");
        }
        
        const auto& ptr_ref = std::get<ValueRef>(command.args()[0]);
        std::string result_type = type_to_llvm_string(command.result().type());
        
        return "  " + command.result().to_string() + " = load " + result_type + ", ptr " + ptr_ref.to_string() + ", align 8\n";
    }
    
    std::string process_store(const IRCommand& command) {
        if (command.args().size() != 2) {
            throw std::runtime_error("Invalid store command");
        }
        
        const auto& value_ref = std::get<ValueRef>(command.args()[0]);
        const auto& ptr_ref = std::get<ValueRef>(command.args()[1]);
        
        std::string value_type = type_to_llvm_string(value_ref.type());
        
        return "  store " + value_type + " " + value_ref.to_string() + ", ptr " + ptr_ref.to_string() + ", align 8\n";
    }
    
    std::string process_gep(const IRCommand& command) {
        if (!command.has_result() || command.args().size() != 2) {
            throw std::runtime_error("Invalid gep command");
        }
        
        const auto& ptr_ref = std::get<ValueRef>(command.args()[0]);
        const auto& index_ref = std::get<ValueRef>(command.args()[1]);
        
        // Simplified GEP - assumes struct access
        return "  " + command.result().to_string() + " = getelementptr inbounds %struct, ptr " + 
               ptr_ref.to_string() + ", i32 0, i32 " + index_ref.to_string() + "\n";
    }
    
    std::string process_add(const IRCommand& command) {
        if (!command.has_result() || command.args().size() != 2) {
            throw std::runtime_error("Invalid add command");
        }
        
        const auto& lhs_ref = std::get<ValueRef>(command.args()[0]);
        const auto& rhs_ref = std::get<ValueRef>(command.args()[1]);
        
        std::string type_str = type_to_llvm_string(command.result().type());
        
        return "  " + command.result().to_string() + " = add " + type_str + " " + 
               lhs_ref.to_string() + ", " + rhs_ref.to_string() + "\n";
    }
    
    std::string process_icmp_eq(const IRCommand& command) {
        if (!command.has_result() || command.args().size() != 2) {
            throw std::runtime_error("Invalid icmp_eq command");
        }
        
        const auto& lhs_ref = std::get<ValueRef>(command.args()[0]);
        const auto& rhs_ref = std::get<ValueRef>(command.args()[1]);
        
        std::string operand_type = type_to_llvm_string(lhs_ref.type());
        
        return "  " + command.result().to_string() + " = icmp eq " + operand_type + " " + 
               lhs_ref.to_string() + ", " + rhs_ref.to_string() + "\n";
    }
    
    std::string process_label(const IRCommand& command) {
        if (command.args().empty()) {
            throw std::runtime_error("Invalid label command");
        }
        
        const auto& label_arg = std::get<LabelArg>(command.args()[0]);
        return "\n" + label_arg.name + ":\n";
    }
    
    std::string process_branch(const IRCommand& command) {
        if (command.args().empty()) {
            throw std::runtime_error("Invalid branch command");
        }
        
        const auto& label_arg = std::get<LabelArg>(command.args()[0]);
        return "  br label %" + label_arg.name + "\n";
    }
    
    std::string process_branch_cond(const IRCommand& command) {
        if (command.args().size() != 3) {
            throw std::runtime_error("Invalid branch_cond command");
        }
        
        const auto& cond_ref = std::get<ValueRef>(command.args()[0]);
        const auto& true_label = std::get<LabelArg>(command.args()[1]);
        const auto& false_label = std::get<LabelArg>(command.args()[2]);
        
        return "  br i1 " + cond_ref.to_string() + ", label %" + true_label.name + 
               ", label %" + false_label.name + "\n";
    }
    
    std::string process_return(const IRCommand& command) {
        if (command.args().empty()) {
            return "  ret void\n";
        } else {
            const auto& value_ref = std::get<ValueRef>(command.args()[0]);
            std::string type_str = type_to_llvm_string(value_ref.type());
            return "  ret " + type_str + " " + value_ref.to_string() + "\n";
        }
    }
    
    std::string process_call(const IRCommand& command) {
        if (command.args().size() < 1) {
            throw std::runtime_error("Invalid call command");
        }
        
        const auto& func_name = std::get<LabelArg>(command.args()[0]);
        
        std::string call_str;
        if (command.has_result()) {
            std::string return_type = type_to_llvm_string(command.result().type());
            call_str = "  " + command.result().to_string() + " = call " + return_type + " @" + func_name.name + "(";
        } else {
            call_str = "  call void @" + func_name.name + "(";
        }
        
        // Add arguments
        for (size_t i = 1; i < command.args().size(); ++i) {
            if (i > 1) call_str += ", ";
            const auto& arg_ref = std::get<ValueRef>(command.args()[i]);
            std::string arg_type = type_to_llvm_string(arg_ref.type());
            call_str += arg_type + " " + arg_ref.to_string();
        }
        
        call_str += ")\n";
        return call_str;
    }
    
    std::string process_func_decl(const IRCommand& command) {
        if (command.args().empty()) {
            throw std::runtime_error("Invalid func_decl command");
        }
        
        const auto& func_arg = std::get<FunctionArg>(command.args()[0]);
        
        std::string decl = "define ";
        decl += type_to_llvm_string(func_arg.function_type->return_type());
        decl += " @" + func_arg.name + "(";
        
        // Add parameters
        const auto& param_types = func_arg.function_type->parameter_types();
        for (size_t i = 0; i < param_types.size(); ++i) {
            if (i > 0) decl += ", ";
            decl += type_to_llvm_string(*param_types[i]);
            decl += " %" + std::to_string(i);
        }
        
        decl += ") {\nentry:\n";
        return decl;
    }
    
    std::string type_to_llvm_string(const Type& type) {
        switch (type.kind()) {
            case Type::Kind::Primitive:
                if (type.name() == "i32") return "i32";
                if (type.name() == "bool") return "i1";
                if (type.name() == "void") return "void";
                break;
            case Type::Kind::Pointer:
                return "ptr";
            case Type::Kind::Struct:
                return "%" + type.name();
            default:
                break;
        }
        throw std::runtime_error("Unsupported type for LLVM: " + type.to_string());
    }
};

} // namespace Myre