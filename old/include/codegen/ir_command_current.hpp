#pragma once

#include "semantic/type_system.hpp"
#include <string>
#include <vector>
#include <variant>
#include <memory>
#include <stdexcept>

namespace Myre {

// === VALUE REFERENCE SYSTEM ===

class ValueRef {
private:
    int id_;
    std::shared_ptr<Type> type_;
    
public:
    ValueRef() : id_(-1), type_(nullptr) {}
    ValueRef(int id, std::shared_ptr<Type> type) : id_(id), type_(std::move(type)) {}
    
    int id() const { return id_; }
    bool is_valid() const { return id_ >= 0 && type_ != nullptr; }
    const Type& type() const { return *type_; }
    std::shared_ptr<Type> type_ptr() const { return type_; }
    
    static ValueRef invalid() { return ValueRef(); }
    
    std::string to_string() const {
        if (!is_valid()) return "invalid";
        return "%" + std::to_string(id_) + ":" + type_->to_string();
    }
};

// === IR COMMANDS ===

enum class OpCode {
    // Values
    ConstantI32,
    ConstantBool,
    ConstantNull,
    
    // Memory
    Alloca,
    Load,
    Store,
    GEP,  // GetElementPtr
    
    // Arithmetic
    Add,
    Sub,
    Mul,
    Div,
    
    // Comparison
    ICmpEQ,
    ICmpNE,
    ICmpSLT,
    ICmpSGT,
    ICmpSLE,
    ICmpSGE,
    
    // Logical
    And,
    Or,
    Not,
    
    // Control Flow
    Label,
    Branch,
    BranchCond,
    Return,
    
    // Functions
    Call,
    FuncDecl,
    
    // Misc
    Unreachable
};

// === COMMAND ARGUMENTS ===

struct ConstantArg {
    std::variant<int32_t, bool> value;
    
    ConstantArg(int32_t v) : value(v) {}
    ConstantArg(bool v) : value(v) {}
    
    std::string to_string() const {
        if (std::holds_alternative<int32_t>(value)) {
            return std::to_string(std::get<int32_t>(value));
        } else {
            return std::get<bool>(value) ? "true" : "false";
        }
    }
};

struct LabelArg {
    std::string name;
    
    LabelArg(std::string n) : name(std::move(n)) {}
    
    std::string to_string() const { return name; }
};

struct FunctionArg {
    std::string name;
    std::shared_ptr<FunctionType> function_type;
    
    FunctionArg(std::string n, std::shared_ptr<FunctionType> ft) 
        : name(std::move(n)), function_type(std::move(ft)) {}
    
    std::string to_string() const { return name + ": " + function_type->to_string(); }
};

using CommandArg = std::variant<ValueRef, ConstantArg, LabelArg, FunctionArg>;

// === IR COMMAND ===

class IRCommand {
private:
    OpCode opcode_;
    ValueRef result_;  // Optional result value
    std::vector<CommandArg> args_;
    std::shared_ptr<Type> type_hint_;  // For type-specific operations
    
public:
    IRCommand(OpCode opcode, ValueRef result = ValueRef::invalid())
        : opcode_(opcode), result_(result) {}
    
    IRCommand(OpCode opcode, ValueRef result, std::vector<CommandArg> args)
        : opcode_(opcode), result_(result), args_(std::move(args)) {}
    
    IRCommand(OpCode opcode, std::vector<CommandArg> args)
        : opcode_(opcode), result_(ValueRef::invalid()), args_(std::move(args)) {}
    
    // Getters
    OpCode opcode() const { return opcode_; }
    const ValueRef& result() const { return result_; }
    const std::vector<CommandArg>& args() const { return args_; }
    
    bool has_result() const { return result_.is_valid(); }
    
    // Type hint for operations that need it
    IRCommand& with_type_hint(std::shared_ptr<Type> type) {
        type_hint_ = std::move(type);
        return *this;
    }
    
    std::shared_ptr<Type> type_hint() const { return type_hint_; }
    
    std::string opcode_string() const {
        switch (opcode_) {
            case OpCode::ConstantI32: return "const_i32";
            case OpCode::ConstantBool: return "const_bool";
            case OpCode::ConstantNull: return "const_null";
            case OpCode::Alloca: return "alloca";
            case OpCode::Load: return "load";
            case OpCode::Store: return "store";
            case OpCode::GEP: return "gep";
            case OpCode::Add: return "add";
            case OpCode::Sub: return "sub";
            case OpCode::Mul: return "mul";
            case OpCode::Div: return "div";
            case OpCode::ICmpEQ: return "icmp_eq";
            case OpCode::ICmpNE: return "icmp_ne";
            case OpCode::ICmpSLT: return "icmp_slt";
            case OpCode::ICmpSGT: return "icmp_sgt";
            case OpCode::ICmpSLE: return "icmp_sle";
            case OpCode::ICmpSGE: return "icmp_sge";
            case OpCode::And: return "and";
            case OpCode::Or: return "or";
            case OpCode::Not: return "not";
            case OpCode::Label: return "label";
            case OpCode::Branch: return "br";
            case OpCode::BranchCond: return "br_cond";
            case OpCode::Return: return "ret";
            case OpCode::Call: return "call";
            case OpCode::FuncDecl: return "func_decl";
            case OpCode::Unreachable: return "unreachable";
        }
        return "unknown";
    }
    
    std::string to_string() const {
        std::string result = opcode_string();
        
        if (has_result()) {
            result = result_.to_string() + " = " + result;
        }
        
        if (!args_.empty()) {
            result += " ";
            for (size_t i = 0; i < args_.size(); ++i) {
                if (i > 0) result += ", ";
                
                std::visit([&result](const auto& arg) {
                    result += arg.to_string();
                }, args_[i]);
            }
        }
        
        return result;
    }
};

// === COMMAND STREAM ===

class CommandStream {
private:
    std::vector<IRCommand> commands_;
    int next_value_id_;
    bool finalized_;
    
public:
    CommandStream() : next_value_id_(0), finalized_(false) {}
    
    // Mutable during construction
    ValueRef next_value(std::shared_ptr<Type> type) {
        if (finalized_) {
            throw std::runtime_error("Cannot modify finalized command stream");
        }
        return ValueRef(next_value_id_++, std::move(type));
    }
    
    void add_command(IRCommand command) {
        if (finalized_) {
            throw std::runtime_error("Cannot modify finalized command stream");
        }
        commands_.push_back(std::move(command));
    }
    
    // Finalize to make immutable
    void finalize() { finalized_ = true; }
    bool is_finalized() const { return finalized_; }
    
    // Immutable access
    const std::vector<IRCommand>& commands() const { return commands_; }
    size_t size() const { return commands_.size(); }
    bool empty() const { return commands_.empty(); }
    
    const IRCommand& operator[](size_t index) const { return commands_[index]; }
    
    // Iteration
    auto begin() const { return commands_.begin(); }
    auto end() const { return commands_.end(); }
    
    // Serialization support
    std::string to_string() const {
        std::string result = "CommandStream (" + std::to_string(commands_.size()) + " commands):\n";
        for (size_t i = 0; i < commands_.size(); ++i) {
            result += "  " + std::to_string(i) + ": " + commands_[i].to_string() + "\n";
        }
        return result;
    }
    
    // Command stream transformations (return new stream)
    CommandStream optimize() const {
        if (!finalized_) {
            throw std::runtime_error("Cannot optimize non-finalized command stream");
        }
        
        // TODO: Implement optimization passes
        CommandStream optimized = *this;
        optimized.finalized_ = true;
        return optimized;
    }
};

// === COMMAND BUILDER HELPERS ===

namespace CommandFactory {
    
    inline IRCommand constant_i32(ValueRef result, int32_t value) {
        return IRCommand(OpCode::ConstantI32, result, {ConstantArg(value)});
    }
    
    inline IRCommand constant_bool(ValueRef result, bool value) {
        return IRCommand(OpCode::ConstantBool, result, {ConstantArg(value)});
    }
    
    inline IRCommand alloca(ValueRef result, std::shared_ptr<Type> type) {
        return IRCommand(OpCode::Alloca, result).with_type_hint(type);
    }
    
    inline IRCommand load(ValueRef result, ValueRef ptr) {
        return IRCommand(OpCode::Load, result, {ptr});
    }
    
    inline IRCommand store(ValueRef value, ValueRef ptr) {
        return IRCommand(OpCode::Store, {value, ptr});
    }
    
    inline IRCommand gep(ValueRef result, ValueRef ptr, ValueRef index) {
        return IRCommand(OpCode::GEP, result, {ptr, index});
    }
    
    inline IRCommand add(ValueRef result, ValueRef lhs, ValueRef rhs) {
        return IRCommand(OpCode::Add, result, {lhs, rhs});
    }
    
    inline IRCommand icmp_eq(ValueRef result, ValueRef lhs, ValueRef rhs) {
        return IRCommand(OpCode::ICmpEQ, result, {lhs, rhs});
    }
    
    inline IRCommand label(const std::string& name) {
        return IRCommand(OpCode::Label, {LabelArg(name)});
    }
    
    inline IRCommand branch(const std::string& target) {
        return IRCommand(OpCode::Branch, {LabelArg(target)});
    }
    
    inline IRCommand branch_cond(ValueRef cond, const std::string& true_label, const std::string& false_label) {
        return IRCommand(OpCode::BranchCond, {cond, LabelArg(true_label), LabelArg(false_label)});
    }
    
    inline IRCommand ret(ValueRef value) {
        return IRCommand(OpCode::Return, {value});
    }
    
    inline IRCommand ret_void() {
        return IRCommand(OpCode::Return);
    }
    
    inline IRCommand call(ValueRef result, const std::string& function_name, std::vector<ValueRef> args) {
        std::vector<CommandArg> cmd_args;
        cmd_args.push_back(LabelArg(function_name));
        for (const auto& arg : args) {
            cmd_args.push_back(arg);
        }
        return IRCommand(OpCode::Call, result, std::move(cmd_args));
    }
    
    inline IRCommand func_decl(const std::string& name, std::shared_ptr<FunctionType> type) {
        return IRCommand(OpCode::FuncDecl, {FunctionArg(name, type)});
    }
    
} // namespace CommandFactory

} // namespace Myre