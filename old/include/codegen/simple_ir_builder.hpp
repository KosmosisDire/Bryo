#pragma once

#include "codegen/ir_value.hpp"
#include "codegen/command.hpp"
#include <vector>
#include <string>
#include <unordered_map>

namespace Myre {

// Simplified IR builder that directly emits commands
class SimpleIRBuilder {
private:
    std::vector<Command> commands_;
    int32_t next_value_id_;
    int32_t next_label_id_;
    std::string current_block_;
    bool block_terminated_;
    
    // Variable storage tracking
    std::unordered_map<std::string, ValueRef> variables_;
    
public:
    SimpleIRBuilder() : next_value_id_(0), next_label_id_(0), block_terminated_(false) {}
    
    // Value creation
    ValueRef next_value(IRType type) {
        return ValueRef(next_value_id_++, type);
    }
    
    std::string next_label(const std::string& prefix = "L") {
        return prefix + std::to_string(next_label_id_++);
    }
    
    // Constants
    ValueRef emit_constant_i32(int32_t value) {
        ValueRef result = next_value(IRType::i32_type);
        commands_.push_back(Command(Op::ConstI32, result, {}, value));
        return result;
    }
    
    ValueRef emit_constant_bool(bool value) {
        ValueRef result = next_value(IRType::i1_type);
        commands_.push_back(Command(Op::ConstBool, result, {}, value ? 1 : 0));
        return result;
    }
    
    // Arithmetic
    ValueRef emit_add(ValueRef lhs, ValueRef rhs) {
        ValueRef result = next_value(lhs.type);
        commands_.push_back(Command(Op::Add, result, {lhs, rhs}));
        return result;
    }
    
    ValueRef emit_sub(ValueRef lhs, ValueRef rhs) {
        ValueRef result = next_value(lhs.type);
        commands_.push_back(Command(Op::Sub, result, {lhs, rhs}));
        return result;
    }
    
    ValueRef emit_mul(ValueRef lhs, ValueRef rhs) {
        ValueRef result = next_value(lhs.type);
        commands_.push_back(Command(Op::Mul, result, {lhs, rhs}));
        return result;
    }
    
    ValueRef emit_div(ValueRef lhs, ValueRef rhs) {
        ValueRef result = next_value(lhs.type);
        commands_.push_back(Command(Op::Div, result, {lhs, rhs}));
        return result;
    }
    
    // Comparison
    ValueRef emit_icmp_eq(ValueRef lhs, ValueRef rhs) {
        ValueRef result = next_value(IRType::i1_type);
        commands_.push_back(Command(Op::ICmpEQ, result, {lhs, rhs}));
        return result;
    }
    
    ValueRef emit_icmp_ne(ValueRef lhs, ValueRef rhs) {
        ValueRef result = next_value(IRType::i1_type);
        commands_.push_back(Command(Op::ICmpNE, result, {lhs, rhs}));
        return result;
    }
    
    ValueRef emit_icmp_lt(ValueRef lhs, ValueRef rhs) {
        ValueRef result = next_value(IRType::i1_type);
        commands_.push_back(Command(Op::ICmpLT, result, {lhs, rhs}));
        return result;
    }
    
    ValueRef emit_icmp_gt(ValueRef lhs, ValueRef rhs) {
        ValueRef result = next_value(IRType::i1_type);
        commands_.push_back(Command(Op::ICmpGT, result, {lhs, rhs}));
        return result;
    }
    
    // Logical
    ValueRef emit_and(ValueRef lhs, ValueRef rhs) {
        ValueRef result = next_value(IRType::i1_type);
        commands_.push_back(Command(Op::And, result, {lhs, rhs}));
        return result;
    }
    
    ValueRef emit_or(ValueRef lhs, ValueRef rhs) {
        ValueRef result = next_value(IRType::i1_type);
        commands_.push_back(Command(Op::Or, result, {lhs, rhs}));
        return result;
    }
    
    ValueRef emit_not(ValueRef operand) {
        ValueRef result = next_value(IRType::i1_type);
        commands_.push_back(Command(Op::Not, result, {operand}));
        return result;
    }
    
    // Memory operations
    ValueRef emit_alloca(IRType type, const std::string& name = "") {
        IRType ptr_type(IRType::Ptr);
        ptr_type.element_type = new IRType(type);  // Note: This leaks - in real code use proper memory management
        
        ValueRef result = next_value(ptr_type);
        commands_.push_back(Command(Op::Alloca, result, {}));
        
        if (!name.empty()) {
            variables_[name] = result;
        }
        
        return result;
    }
    
    void emit_store(ValueRef value, ValueRef ptr) {
        commands_.push_back(Command(Op::Store, ValueRef::invalid(), {value, ptr}));
    }
    
    ValueRef emit_load(ValueRef ptr) {
        if (ptr.type.kind != IRType::Ptr || !ptr.type.element_type) {
            return ValueRef::invalid();
        }
        
        ValueRef result = next_value(*ptr.type.element_type);
        commands_.push_back(Command(Op::Load, result, {ptr}));
        return result;
    }
    
    // Control flow
    void emit_label(const std::string& label) {
        commands_.push_back(Command(Op::Label, ValueRef::invalid(), {}, 0, label));
        current_block_ = label;
        block_terminated_ = false;
    }
    
    void emit_br(const std::string& target) {
        if (!block_terminated_) {
            commands_.push_back(Command(Op::Br, ValueRef::invalid(), {}, 0, target));
            block_terminated_ = true;
        }
    }
    
    void emit_br_cond(ValueRef cond, const std::string& true_label, const std::string& false_label) {
        if (!block_terminated_) {
            commands_.push_back(Command(Op::BrCond, ValueRef::invalid(), {cond}, 0, true_label + "," + false_label));
            block_terminated_ = true;
        }
    }
    
    void emit_ret(ValueRef value) {
        if (!block_terminated_) {
            commands_.push_back(Command(Op::Ret, ValueRef::invalid(), {value}));
            block_terminated_ = true;
        }
    }
    
    void emit_ret_void() {
        if (!block_terminated_) {
            commands_.push_back(Command(Op::Ret, ValueRef::invalid(), {}));
            block_terminated_ = true;
        }
    }
    
    // Function calls
    ValueRef emit_call(const std::string& func_name, const std::vector<ValueRef>& args, IRType return_type) {
        ValueRef result = return_type.kind != IRType::Void ? next_value(return_type) : ValueRef::invalid();
        commands_.push_back(Command(Op::Call, result, args, 0, func_name));
        return result;
    }
    
    // Variable lookup
    ValueRef get_variable(const std::string& name) {
        auto it = variables_.find(name);
        return (it != variables_.end()) ? it->second : ValueRef::invalid();
    }
    
    // State queries
    bool has_terminator() const { return block_terminated_; }
    
    // Get generated commands
    const std::vector<Command>& commands() const { return commands_; }
    
    // Clear builder state
    void clear() {
        commands_.clear();
        variables_.clear();
        next_value_id_ = 0;
        next_label_id_ = 0;
        current_block_.clear();
        block_terminated_ = false;
    }
};

} // namespace Myre